#pragma once
// ============================================================================
// XCapturePipeServer.h - Named Pipe Server for Capture Service (SYSTEM)
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include <TootegaWinLib.h>
#include <XCaptureProtocol.h>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>

namespace Tootega
{
    // ========================================================================
    // XCapturePipeServer - Manages pipes to multiple Capture Agents
    // ========================================================================

    class XCapturePipeServer final
    {
    public:
        // Callback types
        using ResponseCallback = std::function<void(DWORD pSessionId, const XCaptureResponse& pResponse)>;
        using DisconnectCallback = std::function<void(DWORD pSessionId)>;

        TOOTEGA_DISABLE_COPY_MOVE(XCapturePipeServer);

        XCapturePipeServer() noexcept = default;
        ~XCapturePipeServer() noexcept;

        // Lifecycle
        [[nodiscard]] XResult<void> Initialize() noexcept;
        void Shutdown() noexcept;

        // Session management
        [[nodiscard]] XResult<void> CreatePipeForSession(DWORD pSessionId) noexcept;
        [[nodiscard]] XResult<void> ClosePipeForSession(DWORD pSessionId) noexcept;
        [[nodiscard]] bool IsSessionConnected(DWORD pSessionId) const noexcept;

        // Commands
        [[nodiscard]] XResult<void> SendCommand(DWORD pSessionId, const XCaptureCommand& pCommand) noexcept;
        [[nodiscard]] XResult<XCaptureResponse> SendCommandAndWait(
            DWORD pSessionId, 
            const XCaptureCommand& pCommand,
            DWORD pTimeoutMs = CAPTURE_PIPE_TIMEOUT_MS) noexcept;

        // =============================================================
        // TriggerSessionCapture - Main entry point for service
        // =============================================================
        // Starts capture on specified monitor(s) in a session.
        // If pMonitorIndex == -1 (CAPTURE_ALL_MONITORS), captures all monitors.
        // pOutputFolder: Base folder where video files will be saved.
        // Files are named: {TenantID}_{StationID}_Mon{N}_{Timestamp}.mp4
        [[nodiscard]] XResult<XCaptureResponse> TriggerSessionCapture(
            DWORD pSessionId,
            int32_t pMonitorIndex = CAPTURE_ALL_MONITORS,
            const std::wstring& pOutputFolder = L"",
            const std::wstring& pTenantID = L"",
            const std::wstring& pStationID = L"",
            uint32_t pFrameRate = 30,
            uint32_t pQuality = 70,
            bool pGrayscale = true) noexcept;

        // Convenience commands
        [[nodiscard]] XResult<XCaptureResponse> StartCapture(
            DWORD pSessionId,
            int32_t pMonitorIndex,
            const std::wstring& pOutputPath,
            uint32_t pFrameRate = 30,
            uint32_t pQuality = 70,
            bool pGrayscale = false) noexcept;

        [[nodiscard]] XResult<XCaptureResponse> StopCapture(DWORD pSessionId) noexcept;
        [[nodiscard]] XResult<XCaptureResponse> GetStatus(DWORD pSessionId) noexcept;
        [[nodiscard]] XResult<XCaptureResponse> Ping(DWORD pSessionId) noexcept;
        [[nodiscard]] XResult<void> ShutdownAgent(DWORD pSessionId) noexcept;

        // Callbacks
        void SetResponseCallback(ResponseCallback pCallback) noexcept { _ResponseCallback = std::move(pCallback); }
        void SetDisconnectCallback(DisconnectCallback pCallback) noexcept { _DisconnectCallback = std::move(pCallback); }

        // Accessors
        [[nodiscard]] std::vector<DWORD> GetConnectedSessions() const noexcept;

    private:
        struct SessionPipe
        {
            HANDLE                  PipeHandle{INVALID_HANDLE_VALUE};
            OVERLAPPED              ReadOverlapped{};
            OVERLAPPED              WriteOverlapped{};
            HANDLE                  ReadEvent{nullptr};
            HANDLE                  WriteEvent{nullptr};
            std::atomic<bool>       Connected{false};
            std::atomic<bool>       Reading{false};
            std::thread             ReaderThread;
            XCaptureResponse        LastResponse;
            uint32_t                NextSequenceId{1};
        };

        [[nodiscard]] XResult<void> CreateSecurityDescriptor() noexcept;
        void DestroySecurityDescriptor() noexcept;
        [[nodiscard]] HANDLE CreatePipeInstance(DWORD pSessionId) noexcept;
        void ReaderThreadProc(DWORD pSessionId) noexcept;
        void CleanupSessionPipe(SessionPipe& pPipe) noexcept;

        mutable std::mutex                  _Mutex;
        std::map<DWORD, SessionPipe>        _Sessions;
        PSECURITY_DESCRIPTOR                _SecurityDescriptor{nullptr};
        SECURITY_ATTRIBUTES                 _SecurityAttributes{};
        std::atomic<bool>                   _Initialized{false};
        std::atomic<bool>                   _ShuttingDown{false};
        ResponseCallback                    _ResponseCallback;
        DisconnectCallback                  _DisconnectCallback;
    };

} // namespace Tootega
