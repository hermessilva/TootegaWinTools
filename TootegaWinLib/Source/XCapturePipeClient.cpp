// ============================================================================
// XCapturePipeClient.cpp - Named Pipe Client for Capture Agent (User Session)
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XCapturePipeClient.h"

namespace Tootega
{
    // ========================================================================
    // Lifecycle
    // ========================================================================

    XCapturePipeClient::~XCapturePipeClient() noexcept
    {
        Disconnect();
    }

    XResult<void> XCapturePipeClient::Connect(DWORD pSessionId) noexcept
    {
        if (_Connected.load(std::memory_order_acquire))
            return XResult<void>();

        // Get current session if not specified
        if (pSessionId == 0)
        {
            if (!ProcessIdToSessionId(GetCurrentProcessId(), &pSessionId))
                return XError::FromWin32(GetLastError(), L"Failed to get session ID");
        }

        _SessionId = pSessionId;
        std::wstring pipeName = GetCapturePipeName(_SessionId);

        XLogger::Instance().Debug(L"XCapturePipeClient: Connecting to {}", pipeName);

        _PipeHandle = CreateFileW(
            pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            nullptr);

        if (_PipeHandle == INVALID_HANDLE_VALUE)
            return XError::FromWin32(GetLastError(), L"Failed to connect to pipe");

        // Set pipe to message mode
        DWORD mode = PIPE_READMODE_MESSAGE;
        if (!SetNamedPipeHandleState(_PipeHandle, &mode, nullptr, nullptr))
        {
            CloseHandle(_PipeHandle);
            _PipeHandle = INVALID_HANDLE_VALUE;
            return XError::FromWin32(GetLastError(), L"Failed to set pipe mode");
        }

        // Create events for overlapped I/O
        _ReadEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        _WriteEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        if (!_ReadEvent || !_WriteEvent)
        {
            Disconnect();
            return XError::FromWin32(GetLastError(), L"Failed to create events");
        }

        _ReadOverlapped.hEvent = _ReadEvent;
        _WriteOverlapped.hEvent = _WriteEvent;
        _StartTime = GetTickCount64();
        _Connected.store(true, std::memory_order_release);

        XLogger::Instance().Info(L"XCapturePipeClient: Connected to session {}", _SessionId);
        return XResult<void>();
    }

    XResult<void> XCapturePipeClient::ConnectWithRetry(
        DWORD pSessionId,
        DWORD pMaxRetries,
        DWORD pRetryDelayMs) noexcept
    {
        for (DWORD attempt = 0; attempt < pMaxRetries; ++attempt)
        {
            auto result = Connect(pSessionId);
            if (result)
                return result;

            DWORD err = GetLastError();

            // If pipe doesn't exist yet, wait and retry
            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PIPE_BUSY)
            {
                XLogger::Instance().Debug(
                    L"XCapturePipeClient: Pipe not ready, retry {}/{} in {}ms",
                    attempt + 1, pMaxRetries, pRetryDelayMs);

                if (err == ERROR_PIPE_BUSY)
                    WaitNamedPipeW(GetCapturePipeName(pSessionId).c_str(), pRetryDelayMs);
                else
                    Sleep(pRetryDelayMs);

                continue;
            }

            // Other errors - don't retry
            return result;
        }

        return XError::Application(ERROR_TIMEOUT, L"Failed to connect after retries");
    }

    void XCapturePipeClient::Disconnect() noexcept
    {
        StopListening();

        _Connected.store(false, std::memory_order_release);

        if (_PipeHandle != INVALID_HANDLE_VALUE)
        {
            CancelIoEx(_PipeHandle, nullptr);
            CloseHandle(_PipeHandle);
            _PipeHandle = INVALID_HANDLE_VALUE;
        }

        if (_ReadEvent)
        {
            CloseHandle(_ReadEvent);
            _ReadEvent = nullptr;
        }

        if (_WriteEvent)
        {
            CloseHandle(_WriteEvent);
            _WriteEvent = nullptr;
        }

        XLogger::Instance().Info(L"XCapturePipeClient: Disconnected");
    }

    // ========================================================================
    // Listening
    // ========================================================================

    XResult<void> XCapturePipeClient::StartListening() noexcept
    {
        if (!_Connected.load(std::memory_order_acquire))
            return XError::Application(ERROR_NOT_READY, L"Not connected");

        if (_Listening.load(std::memory_order_acquire))
            return XResult<void>();

        _StopRequested.store(false, std::memory_order_release);
        _Listening.store(true, std::memory_order_release);

        _ListenerThread = std::thread([this]() {
            ListenerThreadProc();
        });

        XLogger::Instance().Info(L"XCapturePipeClient: Started listening");
        return XResult<void>();
    }

    void XCapturePipeClient::StopListening() noexcept
    {
        if (!_Listening.exchange(false, std::memory_order_acq_rel))
            return;

        _StopRequested.store(true, std::memory_order_release);

        // Cancel pending I/O
        if (_PipeHandle != INVALID_HANDLE_VALUE)
            CancelIoEx(_PipeHandle, &_ReadOverlapped);

        if (_ReadEvent)
            SetEvent(_ReadEvent);

        if (_ListenerThread.joinable())
            _ListenerThread.join();

        XLogger::Instance().Info(L"XCapturePipeClient: Stopped listening");
    }

    // ========================================================================
    // Listener Thread
    // ========================================================================

    void XCapturePipeClient::ListenerThreadProc() noexcept
    {
        XLogger::Instance().Debug(L"XCapturePipeClient: Listener thread started");

        XCaptureCommand command;

        while (!_StopRequested.load(std::memory_order_acquire) && 
               _Connected.load(std::memory_order_acquire))
        {
            ResetEvent(_ReadEvent);
            DWORD bytesRead = 0;

            BOOL success = ReadFile(
                _PipeHandle,
                &command,
                sizeof(command),
                &bytesRead,
                &_ReadOverlapped);

            if (!success)
            {
                DWORD err = GetLastError();
                if (err == ERROR_IO_PENDING)
                {
                    // Wait with periodic timeout to check stop flag
                    while (!_StopRequested.load(std::memory_order_acquire))
                    {
                        DWORD waitResult = WaitForSingleObject(_ReadEvent, 500);
                        if (waitResult == WAIT_OBJECT_0)
                        {
                            if (!GetOverlappedResult(_PipeHandle, &_ReadOverlapped, &bytesRead, FALSE))
                            {
                                err = GetLastError();
                                if (err == ERROR_OPERATION_ABORTED)
                                    continue;
                                if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
                                {
                                    XLogger::Instance().Warning(L"XCapturePipeClient: Pipe broken");
                                    _Connected.store(false, std::memory_order_release);
                                    return;
                                }
                            }
                            break;
                        }
                    }

                    if (_StopRequested.load(std::memory_order_acquire))
                        break;
                }
                else if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
                {
                    XLogger::Instance().Warning(L"XCapturePipeClient: Service disconnected");
                    _Connected.store(false, std::memory_order_release);
                    return;
                }
                else if (err == ERROR_OPERATION_ABORTED)
                {
                    continue;
                }
                else
                {
                    XLogger::Instance().Warning(L"XCapturePipeClient: Read error: {}", err);
                    continue;
                }
            }

            if (bytesRead >= sizeof(command) && command.IsValid())
            {
                XLogger::Instance().Debug(
                    L"XCapturePipeClient: Received command {} (seq {})",
                    CaptureActionToString(command.Action),
                    command.SequenceId);

                auto processResult = ProcessCommand(command);
                if (!processResult)
                {
                    XLogger::Instance().Warning(
                        L"XCapturePipeClient: Failed to process command: {}",
                        processResult.Error().FormatMessage());
                }

                // Check for shutdown command
                if (command.Action == XCaptureAction::Shutdown)
                {
                    XLogger::Instance().Info(L"XCapturePipeClient: Shutdown requested");
                    _StopRequested.store(true, std::memory_order_release);
                    break;
                }
            }
        }

        _Listening.store(false, std::memory_order_release);
        XLogger::Instance().Debug(L"XCapturePipeClient: Listener thread exiting");
    }

    // ========================================================================
    // Command Processing
    // ========================================================================

    XResult<void> XCapturePipeClient::ProcessCommand(const XCaptureCommand& pCommand) noexcept
    {
        XCaptureResponse response;
        response.Initialize();
        response.SequenceId = pCommand.SequenceId;
        response.SessionId = _SessionId;
        response.Uptime = GetUptime();

        if (_CommandCallback)
        {
            try
            {
                response = _CommandCallback(pCommand);
                response.SequenceId = pCommand.SequenceId;
                response.SessionId = _SessionId;
                response.Uptime = GetUptime();
            }
            catch (const std::exception& ex)
            {
                response.Status = XCaptureStatus::ErrorGeneral;
                response.ErrorCode = ERROR_INTERNAL_ERROR;
                
                // Convert exception message
                std::string msg = ex.what();
                std::wstring wmsg(msg.begin(), msg.end());
                response.SetMessage(wmsg.c_str());
            }
            catch (...)
            {
                response.Status = XCaptureStatus::ErrorGeneral;
                response.ErrorCode = ERROR_INTERNAL_ERROR;
                response.SetMessage(L"Unknown exception in command handler");
            }
        }
        else
        {
            // No handler - just acknowledge
            response.Status = XCaptureStatus::Success;
            response.SetMessage(L"No handler registered");
        }

        return SendResponse(response);
    }

    // ========================================================================
    // Response Sending
    // ========================================================================

    XResult<void> XCapturePipeClient::SendResponse(const XCaptureResponse& pResponse) noexcept
    {
        if (!_Connected.load(std::memory_order_acquire))
            return XError::Application(ERROR_PIPE_NOT_CONNECTED, L"Not connected");

        ResetEvent(_WriteEvent);
        DWORD bytesWritten = 0;

        BOOL success = WriteFile(
            _PipeHandle,
            &pResponse,
            sizeof(pResponse),
            &bytesWritten,
            &_WriteOverlapped);

        if (!success)
        {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING)
            {
                DWORD waitResult = WaitForSingleObject(_WriteEvent, CAPTURE_PIPE_TIMEOUT_MS);
                if (waitResult != WAIT_OBJECT_0)
                    return XError::Application(ERROR_TIMEOUT, L"Write timeout");

                if (!GetOverlappedResult(_PipeHandle, &_WriteOverlapped, &bytesWritten, FALSE))
                    return XError::FromWin32(GetLastError(), L"Write failed");
            }
            else
            {
                return XError::FromWin32(err, L"Write failed");
            }
        }

        return XResult<void>();
    }

    // ========================================================================
    // Utilities
    // ========================================================================

    uint64_t XCapturePipeClient::GetUptime() const noexcept
    {
        if (_StartTime == 0)
            return 0;
        return GetTickCount64() - _StartTime;
    }

} // namespace Tootega
