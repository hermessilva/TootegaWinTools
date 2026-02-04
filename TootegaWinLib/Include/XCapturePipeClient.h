#pragma once
// ============================================================================
// XCapturePipeClient.h - Named Pipe Client for Capture Agent (User Session)
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include <TootegaWinLib.h>
#include <XCaptureProtocol.h>
#include <functional>
#include <thread>
#include <atomic>

namespace Tootega
{
    // ========================================================================
    // XCapturePipeClient - Connects to Service pipe and processes commands
    // ========================================================================

    class XCapturePipeClient final
    {
    public:
        // Callback for command processing - return response to send back
        using CommandCallback = std::function<XCaptureResponse(const XCaptureCommand& pCommand)>;

        TOOTEGA_DISABLE_COPY_MOVE(XCapturePipeClient);

        XCapturePipeClient() noexcept = default;
        ~XCapturePipeClient() noexcept;

        // Lifecycle
        [[nodiscard]] XResult<void> Connect(DWORD pSessionId = 0) noexcept;
        [[nodiscard]] XResult<void> ConnectWithRetry(
            DWORD pSessionId = 0,
            DWORD pMaxRetries = 10,
            DWORD pRetryDelayMs = 1000) noexcept;
        void Disconnect() noexcept;

        // Processing
        void SetCommandCallback(CommandCallback pCallback) noexcept { _CommandCallback = std::move(pCallback); }
        [[nodiscard]] XResult<void> StartListening() noexcept;
        void StopListening() noexcept;

        // Manual response (for async operations)
        [[nodiscard]] XResult<void> SendResponse(const XCaptureResponse& pResponse) noexcept;

        // Status
        [[nodiscard]] bool IsConnected() const noexcept { return _Connected.load(std::memory_order_acquire); }
        [[nodiscard]] bool IsListening() const noexcept { return _Listening.load(std::memory_order_acquire); }
        [[nodiscard]] DWORD GetSessionId() const noexcept { return _SessionId; }
        [[nodiscard]] uint64_t GetUptime() const noexcept;

    private:
        void ListenerThreadProc() noexcept;
        [[nodiscard]] XResult<void> ProcessCommand(const XCaptureCommand& pCommand) noexcept;

        HANDLE                      _PipeHandle{INVALID_HANDLE_VALUE};
        OVERLAPPED                  _ReadOverlapped{};
        OVERLAPPED                  _WriteOverlapped{};
        HANDLE                      _ReadEvent{nullptr};
        HANDLE                      _WriteEvent{nullptr};
        std::thread                 _ListenerThread;
        std::atomic<bool>           _Connected{false};
        std::atomic<bool>           _Listening{false};
        std::atomic<bool>           _StopRequested{false};
        DWORD                       _SessionId{0};
        ULONGLONG                   _StartTime{0};
        CommandCallback             _CommandCallback;
    };

} // namespace Tootega
