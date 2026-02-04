// ============================================================================
// XCapturePipeServer.cpp - Named Pipe Server for Capture Service (SYSTEM)
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XCapturePipeServer.h"
#include <sddl.h>
#include <AclAPI.h>

#pragma comment(lib, "advapi32.lib")

namespace Tootega
{
    // ========================================================================
    // Lifecycle
    // ========================================================================

    XCapturePipeServer::~XCapturePipeServer() noexcept
    {
        Shutdown();
    }

    XResult<void> XCapturePipeServer::Initialize() noexcept
    {
        if (_Initialized.load(std::memory_order_acquire))
            return XResult<void>();

        auto sdResult = CreateSecurityDescriptor();
        if (!sdResult)
            return sdResult;

        _ShuttingDown.store(false, std::memory_order_release);
        _Initialized.store(true, std::memory_order_release);

        XLogger::Instance().Info(L"XCapturePipeServer: Initialized");
        return XResult<void>();
    }

    void XCapturePipeServer::Shutdown() noexcept
    {
        if (!_Initialized.exchange(false, std::memory_order_acq_rel))
            return;

        _ShuttingDown.store(true, std::memory_order_release);

        std::lock_guard lock(_Mutex);
        for (auto& [sessionId, pipe] : _Sessions)
            CleanupSessionPipe(pipe);

        _Sessions.clear();
        DestroySecurityDescriptor();

        XLogger::Instance().Info(L"XCapturePipeServer: Shutdown complete");
    }

    // ========================================================================
    // Security Descriptor
    // ========================================================================

    XResult<void> XCapturePipeServer::CreateSecurityDescriptor() noexcept
    {
        // SDDL string that allows:
        // - SYSTEM (SY): Full control
        // - Administrators (BA): Full control  
        // - Authenticated Users (AU): Read/Write (connect and communicate)
        // - Interactive Users (IU): Read/Write
        const wchar_t* sddl = 
            L"D:"                           // DACL
            L"(A;OICI;GA;;;SY)"             // SYSTEM: Generic All
            L"(A;OICI;GA;;;BA)"             // Administrators: Generic All
            L"(A;OICI;GRGW;;;AU)"           // Authenticated Users: Read/Write
            L"(A;OICI;GRGW;;;IU)";          // Interactive Users: Read/Write

        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
                sddl, 
                SDDL_REVISION_1, 
                &_SecurityDescriptor, 
                nullptr))
        {
            return XError::FromWin32(GetLastError(), L"Failed to create security descriptor");
        }

        _SecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
        _SecurityAttributes.lpSecurityDescriptor = _SecurityDescriptor;
        _SecurityAttributes.bInheritHandle = FALSE;

        return XResult<void>();
    }

    void XCapturePipeServer::DestroySecurityDescriptor() noexcept
    {
        if (_SecurityDescriptor)
        {
            LocalFree(_SecurityDescriptor);
            _SecurityDescriptor = nullptr;
        }
    }

    // ========================================================================
    // Session Management
    // ========================================================================

    XResult<void> XCapturePipeServer::CreatePipeForSession(DWORD pSessionId) noexcept
    {
        if (!_Initialized.load(std::memory_order_acquire))
            return XError::Application(ERROR_NOT_READY, L"Server not initialized");

        std::lock_guard lock(_Mutex);

        if (_Sessions.contains(pSessionId))
            return XResult<void>(); // Already exists

        HANDLE pipeHandle = CreatePipeInstance(pSessionId);
        if (pipeHandle == INVALID_HANDLE_VALUE)
            return XError::FromWin32(GetLastError(), L"Failed to create named pipe");

        SessionPipe& session = _Sessions[pSessionId];
        session.PipeHandle = pipeHandle;
        session.Connected.store(false, std::memory_order_release);
        session.Reading.store(true, std::memory_order_release);

        session.ReadEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        session.WriteEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        if (!session.ReadEvent || !session.WriteEvent)
        {
            CleanupSessionPipe(session);
            _Sessions.erase(pSessionId);
            return XError::FromWin32(GetLastError(), L"Failed to create events");
        }

        session.ReadOverlapped.hEvent = session.ReadEvent;
        session.WriteOverlapped.hEvent = session.WriteEvent;

        // Start reader thread to handle connection and responses
        session.ReaderThread = std::thread([this, pSessionId]() {
            ReaderThreadProc(pSessionId);
        });

        XLogger::Instance().Info(L"XCapturePipeServer: Created pipe for session {}", pSessionId);
        return XResult<void>();
    }

    XResult<void> XCapturePipeServer::ClosePipeForSession(DWORD pSessionId) noexcept
    {
        std::lock_guard lock(_Mutex);

        auto it = _Sessions.find(pSessionId);
        if (it == _Sessions.end())
            return XResult<void>();

        CleanupSessionPipe(it->second);
        _Sessions.erase(it);

        XLogger::Instance().Info(L"XCapturePipeServer: Closed pipe for session {}", pSessionId);
        return XResult<void>();
    }

    bool XCapturePipeServer::IsSessionConnected(DWORD pSessionId) const noexcept
    {
        std::lock_guard lock(_Mutex);
        auto it = _Sessions.find(pSessionId);
        return it != _Sessions.end() && it->second.Connected.load(std::memory_order_acquire);
    }

    std::vector<DWORD> XCapturePipeServer::GetConnectedSessions() const noexcept
    {
        std::vector<DWORD> result;
        std::lock_guard lock(_Mutex);

        for (const auto& [sessionId, pipe] : _Sessions)
        {
            if (pipe.Connected.load(std::memory_order_acquire))
                result.push_back(sessionId);
        }

        return result;
    }

    // ========================================================================
    // Pipe Creation
    // ========================================================================

    HANDLE XCapturePipeServer::CreatePipeInstance(DWORD pSessionId) noexcept
    {
        std::wstring pipeName = GetCapturePipeName(pSessionId);

        return CreateNamedPipeW(
            pipeName.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,                          // Max instances per session
            CAPTURE_PIPE_BUFFER_SIZE,
            CAPTURE_PIPE_BUFFER_SIZE,
            CAPTURE_PIPE_TIMEOUT_MS,
            &_SecurityAttributes);
    }

    // ========================================================================
    // Reader Thread
    // ========================================================================

    void XCapturePipeServer::ReaderThreadProc(DWORD pSessionId) noexcept
    {
        XLogger::Instance().Debug(L"XCapturePipeServer: Reader thread started for session {}", pSessionId);

        SessionPipe* pSession = nullptr;
        {
            std::lock_guard lock(_Mutex);
            auto it = _Sessions.find(pSessionId);
            if (it == _Sessions.end())
                return;
            pSession = &it->second;
        }

        // Wait for client connection
        BOOL connected = ConnectNamedPipe(pSession->PipeHandle, &pSession->ReadOverlapped);
        if (!connected)
        {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING)
            {
                // Wait for connection with periodic checks for shutdown
                while (pSession->Reading.load(std::memory_order_acquire) && 
                       !_ShuttingDown.load(std::memory_order_acquire))
                {
                    DWORD waitResult = WaitForSingleObject(pSession->ReadEvent, 1000);
                    if (waitResult == WAIT_OBJECT_0)
                    {
                        DWORD bytesTransferred;
                        if (GetOverlappedResult(pSession->PipeHandle, &pSession->ReadOverlapped, 
                                                &bytesTransferred, FALSE))
                            break;
                    }
                }
            }
            else if (err != ERROR_PIPE_CONNECTED)
            {
                XLogger::Instance().Warning(
                    L"XCapturePipeServer: ConnectNamedPipe failed for session {}: {}",
                    pSessionId, err);
                return;
            }
        }

        if (_ShuttingDown.load(std::memory_order_acquire))
            return;

        pSession->Connected.store(true, std::memory_order_release);
        XLogger::Instance().Info(L"XCapturePipeServer: Agent connected for session {}", pSessionId);

        // Read loop
        XCaptureResponse response;
        while (pSession->Reading.load(std::memory_order_acquire) && 
               !_ShuttingDown.load(std::memory_order_acquire))
        {
            ResetEvent(pSession->ReadEvent);
            DWORD bytesRead = 0;

            BOOL success = ReadFile(
                pSession->PipeHandle,
                &response,
                sizeof(response),
                &bytesRead,
                &pSession->ReadOverlapped);

            if (!success)
            {
                DWORD err = GetLastError();
                if (err == ERROR_IO_PENDING)
                {
                    // Wait with timeout
                    DWORD waitResult = WaitForSingleObject(pSession->ReadEvent, 1000);
                    if (waitResult == WAIT_OBJECT_0)
                    {
                        if (!GetOverlappedResult(pSession->PipeHandle, &pSession->ReadOverlapped, 
                                                 &bytesRead, FALSE))
                        {
                            err = GetLastError();
                            if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
                                break; // Client disconnected
                            continue;
                        }
                    }
                    else
                    {
                        continue; // Timeout, check flags and retry
                    }
                }
                else if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
                {
                    break; // Client disconnected
                }
                else
                {
                    continue;
                }
            }

            if (bytesRead >= sizeof(response) && response.IsValid())
            {
                pSession->LastResponse = response;

                if (_ResponseCallback)
                {
                    try { _ResponseCallback(pSessionId, response); }
                    catch (...) {}
                }
            }
        }

        pSession->Connected.store(false, std::memory_order_release);
        XLogger::Instance().Info(L"XCapturePipeServer: Agent disconnected for session {}", pSessionId);

        if (_DisconnectCallback && !_ShuttingDown.load(std::memory_order_acquire))
        {
            try { _DisconnectCallback(pSessionId); }
            catch (...) {}
        }
    }

    // ========================================================================
    // Cleanup
    // ========================================================================

    void XCapturePipeServer::CleanupSessionPipe(SessionPipe& pPipe) noexcept
    {
        pPipe.Reading.store(false, std::memory_order_release);
        pPipe.Connected.store(false, std::memory_order_release);

        if (pPipe.PipeHandle != INVALID_HANDLE_VALUE)
        {
            DisconnectNamedPipe(pPipe.PipeHandle);
            CancelIoEx(pPipe.PipeHandle, nullptr);
            CloseHandle(pPipe.PipeHandle);
            pPipe.PipeHandle = INVALID_HANDLE_VALUE;
        }

        if (pPipe.ReadEvent)
        {
            SetEvent(pPipe.ReadEvent);
            CloseHandle(pPipe.ReadEvent);
            pPipe.ReadEvent = nullptr;
        }

        if (pPipe.WriteEvent)
        {
            CloseHandle(pPipe.WriteEvent);
            pPipe.WriteEvent = nullptr;
        }

        if (pPipe.ReaderThread.joinable())
            pPipe.ReaderThread.join();
    }

    // ========================================================================
    // Command Sending
    // ========================================================================

    XResult<void> XCapturePipeServer::SendCommand(DWORD pSessionId, const XCaptureCommand& pCommand) noexcept
    {
        std::lock_guard lock(_Mutex);

        auto it = _Sessions.find(pSessionId);
        if (it == _Sessions.end())
            return XError::Application(ERROR_NOT_FOUND, L"Session not found");

        SessionPipe& session = it->second;
        if (!session.Connected.load(std::memory_order_acquire))
            return XError::Application(ERROR_PIPE_NOT_CONNECTED, L"Agent not connected");

        XCaptureCommand cmd = pCommand;
        cmd.SequenceId = session.NextSequenceId++;

        ResetEvent(session.WriteEvent);
        DWORD bytesWritten = 0;

        BOOL success = WriteFile(
            session.PipeHandle,
            &cmd,
            sizeof(cmd),
            &bytesWritten,
            &session.WriteOverlapped);

        if (!success)
        {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING)
            {
                DWORD waitResult = WaitForSingleObject(session.WriteEvent, CAPTURE_PIPE_TIMEOUT_MS);
                if (waitResult != WAIT_OBJECT_0)
                    return XError::Application(ERROR_TIMEOUT, L"Write timeout");

                if (!GetOverlappedResult(session.PipeHandle, &session.WriteOverlapped, &bytesWritten, FALSE))
                    return XError::FromWin32(GetLastError(), L"Write failed");
            }
            else
            {
                return XError::FromWin32(err, L"Write failed");
            }
        }

        return XResult<void>();
    }

    XResult<XCaptureResponse> XCapturePipeServer::SendCommandAndWait(
        DWORD pSessionId, 
        const XCaptureCommand& pCommand,
        DWORD pTimeoutMs) noexcept
    {
        uint32_t sequenceId;
        {
            std::lock_guard lock(_Mutex);
            auto it = _Sessions.find(pSessionId);
            if (it == _Sessions.end())
                return XError::Application(ERROR_NOT_FOUND, L"Session not found");
            sequenceId = it->second.NextSequenceId;
        }

        auto sendResult = SendCommand(pSessionId, pCommand);
        if (!sendResult)
            return sendResult.Error();

        // Wait for response with matching sequence ID
        auto startTime = GetTickCount64();
        while (GetTickCount64() - startTime < pTimeoutMs)
        {
            {
                std::lock_guard lock(_Mutex);
                auto it = _Sessions.find(pSessionId);
                if (it != _Sessions.end() && it->second.LastResponse.SequenceId == sequenceId)
                    return it->second.LastResponse;
            }
            Sleep(10);
        }

        return XError::Application(ERROR_TIMEOUT, L"Response timeout");
    }

    // ========================================================================
    // Convenience Commands
    // ========================================================================

    XResult<XCaptureResponse> XCapturePipeServer::TriggerSessionCapture(
        DWORD pSessionId,
        int32_t pMonitorIndex,
        const std::wstring& pOutputFolder,
        const std::wstring& pTenantID,
        const std::wstring& pStationID,
        uint32_t pFrameRate,
        uint32_t pQuality,
        bool pGrayscale) noexcept
    {
        if (!_Initialized.load(std::memory_order_acquire))
            return XError::Application(ERROR_NOT_READY, L"Server not initialized");

        if (!IsSessionConnected(pSessionId))
            return XError::Application(ERROR_PIPE_NOT_CONNECTED, L"Session not connected");

        // Check disk space before starting
        if (!pOutputFolder.empty())
        {
            uint64_t freeSpace = GetAvailableDiskSpace(pOutputFolder);
            if (freeSpace < CAPTURE_MIN_DISK_SPACE_BYTES)
            {
                XCaptureResponse errorResp;
                errorResp.Initialize();
                errorResp.Status = XCaptureStatus::ErrorDiskSpace;
                errorResp.ErrorCode = ERROR_DISK_FULL;
                errorResp.SetMessage(L"Insufficient disk space (< 500 MB)");
                return errorResp;
            }
        }

        XCaptureCommand cmd;
        cmd.Initialize();
        cmd.Action = XCaptureAction::Start;
        cmd.MonitorIndex = pMonitorIndex;
        cmd.FrameRate = pFrameRate;
        cmd.Quality = pQuality;
        cmd.Flags = pGrayscale ? XCaptureCommand::FLAG_GRAYSCALE : 0;

        // Set identity for video naming
        cmd.Identity.SetTenantID(pTenantID.c_str());
        cmd.Identity.SetStationID(pStationID.c_str());
        cmd.Identity.TimestampStart = GetCurrentTimestamp();

        // If capturing all monitors, set the flag
        if (pMonitorIndex == CAPTURE_ALL_MONITORS)
            cmd.Flags |= XCaptureCommand::FLAG_ALL_MONITORS;

        // Set output folder
        wcsncpy_s(cmd.OutputPath, pOutputFolder.c_str(), _TRUNCATE);

        XLogger::Instance().Info(
            L"XCapturePipeServer: TriggerSessionCapture - Session {}, Monitor {}, Tenant '{}', Station '{}'",
            pSessionId, 
            pMonitorIndex == CAPTURE_ALL_MONITORS ? L"ALL" : std::to_wstring(pMonitorIndex).c_str(),
            pTenantID.empty() ? L"(default)" : pTenantID.c_str(),
            pStationID.empty() ? L"(default)" : pStationID.c_str());

        return SendCommandAndWait(pSessionId, cmd, 10000); // 10s timeout for multi-monitor init
    }

    XResult<XCaptureResponse> XCapturePipeServer::StartCapture(
        DWORD pSessionId,
        int32_t pMonitorIndex,
        const std::wstring& pOutputPath,
        uint32_t pFrameRate,
        uint32_t pQuality,
        bool pGrayscale) noexcept
    {
        XCaptureCommand cmd;
        cmd.Initialize();
        cmd.Action = XCaptureAction::Start;
        cmd.MonitorIndex = pMonitorIndex;
        cmd.FrameRate = pFrameRate;
        cmd.Quality = pQuality;
        cmd.Flags = pGrayscale ? XCaptureCommand::FLAG_GRAYSCALE : 0;
        wcsncpy_s(cmd.OutputPath, pOutputPath.c_str(), _TRUNCATE);

        return SendCommandAndWait(pSessionId, cmd);
    }

    XResult<XCaptureResponse> XCapturePipeServer::StopCapture(DWORD pSessionId) noexcept
    {
        XCaptureCommand cmd;
        cmd.Initialize();
        cmd.Action = XCaptureAction::Stop;

        return SendCommandAndWait(pSessionId, cmd);
    }

    XResult<XCaptureResponse> XCapturePipeServer::GetStatus(DWORD pSessionId) noexcept
    {
        XCaptureCommand cmd;
        cmd.Initialize();
        cmd.Action = XCaptureAction::Status;

        return SendCommandAndWait(pSessionId, cmd);
    }

    XResult<XCaptureResponse> XCapturePipeServer::Ping(DWORD pSessionId) noexcept
    {
        XCaptureCommand cmd;
        cmd.Initialize();
        cmd.Action = XCaptureAction::Ping;

        return SendCommandAndWait(pSessionId, cmd, 2000);
    }

    XResult<void> XCapturePipeServer::ShutdownAgent(DWORD pSessionId) noexcept
    {
        XCaptureCommand cmd;
        cmd.Initialize();
        cmd.Action = XCaptureAction::Shutdown;

        return SendCommand(pSessionId, cmd);
    }

} // namespace Tootega
