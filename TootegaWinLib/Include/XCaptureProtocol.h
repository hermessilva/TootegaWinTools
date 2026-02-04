#pragma once
// ============================================================================
// XCaptureProtocol.h - IPC Protocol definitions for Service <-> Capture Agent
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include <Windows.h>
#include <cstdint>

namespace Tootega
{
    // ========================================================================
    // Protocol Constants
    // ========================================================================

    constexpr wchar_t CAPTURE_PIPE_NAME_FORMAT[] = L"\\\\.\\pipe\\TootegaCapture_Session_%u";
    constexpr DWORD   CAPTURE_PIPE_BUFFER_SIZE   = 16384;  // 16 KB for multi-monitor response
    constexpr DWORD   CAPTURE_PIPE_TIMEOUT_MS    = 5000;
    constexpr DWORD   CAPTURE_HEARTBEAT_INTERVAL = 3000;
    constexpr DWORD   CAPTURE_MAX_MONITORS       = 8;      // Reasonable max for most systems

    // Minimum free disk space required (500 MB)
    constexpr uint64_t CAPTURE_MIN_DISK_SPACE_BYTES = 500ULL * 1024ULL * 1024ULL;

    // Monitor index sentinel value
    constexpr int32_t CAPTURE_ALL_MONITORS = -1;

    // ========================================================================
    // Command Actions
    // ========================================================================

    enum class XCaptureAction : int32_t
    {
        Stop            = 0,    // Stop capture and close all video files
        Start           = 1,    // Start capture on specified monitor(s)
        Pause           = 2,    // Pause capture (keep files open)
        Resume          = 3,    // Resume paused capture
        Status          = 4,    // Request status update
        Shutdown        = 5,    // Terminate agent process
        Ping            = 99    // Heartbeat ping
    };

    // ========================================================================
    // Response Status Codes
    // ========================================================================

    enum class XCaptureStatus : int32_t
    {
        Success         = 0,    // Command executed successfully
        Ready           = 1,    // Agent ready and waiting
        Recording       = 2,    // Currently recording
        Paused          = 3,    // Recording paused
        Stopped         = 4,    // Recording stopped
        
        // Errors (negative values)
        ErrorGeneral    = -1,   // Generic error
        ErrorMonitor    = -2,   // Monitor not found
        ErrorPath       = -3,   // Invalid output path
        ErrorEncoder    = -4,   // Encoder initialization failed
        ErrorCapture    = -5,   // Capture initialization failed
        ErrorBusy       = -6,   // Agent busy with another operation
        ErrorTimeout    = -7,   // Operation timed out
        ErrorPipe       = -8,   // Pipe communication error
        ErrorDiskSpace  = -9,   // Insufficient disk space
        ErrorDiskFull   = -10   // Disk full during capture
    };

    // ========================================================================
    // Video Identification (TenantID, StationID, Monitor, Timestamp)
    // ========================================================================

    #pragma pack(push, 1)
    struct XCaptureIdentity
    {
        wchar_t         TenantID[64];       // Tenant identifier (organization)
        wchar_t         StationID[64];      // Station/workstation identifier
        int32_t         MonitorIndex;       // Monitor index (0-based)
        uint64_t        TimestampStart;     // Capture start timestamp (FILETIME)

        void Initialize() noexcept
        {
            TenantID[0] = L'\0';
            StationID[0] = L'\0';
            MonitorIndex = 0;
            TimestampStart = 0;
        }

        void SetTenantID(const wchar_t* pValue) noexcept
        {
            if (pValue)
                wcsncpy_s(TenantID, pValue, _TRUNCATE);
            else
                TenantID[0] = L'\0';
        }

        void SetStationID(const wchar_t* pValue) noexcept
        {
            if (pValue)
                wcsncpy_s(StationID, pValue, _TRUNCATE);
            else
                StationID[0] = L'\0';
        }

        // Generate filename: TenantID_StationID_MonX_YYYYMMDD_HHMMSS.mp4
        [[nodiscard]] std::wstring GenerateFilename() const noexcept
        {
            FILETIME ft;
            ft.dwLowDateTime = static_cast<DWORD>(TimestampStart & 0xFFFFFFFF);
            ft.dwHighDateTime = static_cast<DWORD>(TimestampStart >> 32);

            SYSTEMTIME st;
            FileTimeToSystemTime(&ft, &st);

            wchar_t buffer[256];
            swprintf_s(buffer,
                L"%s_%s_Mon%d_%04d%02d%02d_%02d%02d%02d.mp4",
                (TenantID[0] != L'\0') ? TenantID : L"Default",
                (StationID[0] != L'\0') ? StationID : L"Unknown",
                MonitorIndex,
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond);

            return buffer;
        }
    };
    #pragma pack(pop)

    // ========================================================================
    // Command Packet (Service -> Agent)
    // ========================================================================

    #pragma pack(push, 1)
    struct XCaptureCommand
    {
        uint32_t        Magic;              // Protocol magic number
        uint32_t        Version;            // Protocol version
        uint32_t        SequenceId;         // Command sequence for tracking
        XCaptureAction  Action;             // Command action
        int32_t         MonitorIndex;       // Target monitor (0-based, -1 for all)
        uint32_t        FrameRate;          // Desired frame rate (1-120)
        uint32_t        Quality;            // Quality level (1-100)
        uint32_t        Flags;              // Option flags (grayscale, etc.)
        XCaptureIdentity Identity;          // Video identification
        wchar_t         OutputPath[MAX_PATH]; // Output folder path (prefix)

        static constexpr uint32_t MAGIC   = 0x43415054; // 'CAPT'
        static constexpr uint32_t VERSION = 2;          // Version 2 with multi-monitor

        // Flags
        static constexpr uint32_t FLAG_GRAYSCALE    = 0x0001;
        static constexpr uint32_t FLAG_ALL_MONITORS = 0x0002;

        void Initialize() noexcept
        {
            Magic = MAGIC;
            Version = VERSION;
            SequenceId = 0;
            Action = XCaptureAction::Status;
            MonitorIndex = 0;
            FrameRate = 30;
            Quality = 70;
            Flags = 0;
            Identity.Initialize();
            OutputPath[0] = L'\0';
        }

        [[nodiscard]] bool IsValid() const noexcept
        {
            return Magic == MAGIC && (Version == VERSION || Version == 1);
        }

        [[nodiscard]] bool IsCaptureAllMonitors() const noexcept
        {
            return MonitorIndex == CAPTURE_ALL_MONITORS || 
                   (Flags & FLAG_ALL_MONITORS) != 0;
        }
    };
    #pragma pack(pop)

    static_assert(sizeof(XCaptureCommand) < CAPTURE_PIPE_BUFFER_SIZE, 
        "Command packet exceeds pipe buffer size");

    // ========================================================================
    // Per-Monitor Status (for multi-monitor response)
    // ========================================================================

    #pragma pack(push, 1)
    struct XMonitorCaptureStatus
    {
        int32_t         MonitorIndex;       // Monitor index
        XCaptureStatus  Status;             // Status for this monitor
        uint64_t        FramesCaptured;     // Frames captured on this monitor
        uint64_t        BytesWritten;       // Bytes written for this monitor
        int32_t         ErrorCode;          // Error code if failed
        wchar_t         FilePath[MAX_PATH]; // Actual output file path

        void Initialize() noexcept
        {
            MonitorIndex = 0;
            Status = XCaptureStatus::Ready;
            FramesCaptured = 0;
            BytesWritten = 0;
            ErrorCode = 0;
            FilePath[0] = L'\0';
        }
    };
    #pragma pack(pop)

    // ========================================================================
    // Response Packet (Agent -> Service)
    // ========================================================================

    #pragma pack(push, 1)
    struct XCaptureResponse
    {
        uint32_t        Magic;              // Protocol magic number
        uint32_t        Version;            // Protocol version
        uint32_t        SequenceId;         // Echo of command sequence
        XCaptureStatus  Status;             // Overall response status
        uint32_t        SessionId;          // Windows session ID
        uint64_t        TotalFramesCaptured;// Total frames across all monitors
        uint64_t        TotalBytesWritten;  // Total bytes across all monitors
        uint64_t        Uptime;             // Agent uptime in milliseconds
        int32_t         ErrorCode;          // Win32 error code if failed
        uint32_t        ActiveMonitorCount; // Number of active capture threads
        uint32_t        MonitorStatusCount; // Number of valid MonitorStatus entries
        XMonitorCaptureStatus MonitorStatus[CAPTURE_MAX_MONITORS]; // Per-monitor status
        wchar_t         Message[256];       // Status message or error description

        static constexpr uint32_t MAGIC   = 0x52455350; // 'RESP'
        static constexpr uint32_t VERSION = 2;

        void Initialize() noexcept
        {
            Magic = MAGIC;
            Version = VERSION;
            SequenceId = 0;
            Status = XCaptureStatus::Ready;
            SessionId = 0;
            TotalFramesCaptured = 0;
            TotalBytesWritten = 0;
            Uptime = 0;
            ErrorCode = 0;
            ActiveMonitorCount = 0;
            MonitorStatusCount = 0;
            for (auto& ms : MonitorStatus)
                ms.Initialize();
            Message[0] = L'\0';
        }

        [[nodiscard]] bool IsValid() const noexcept
        {
            return Magic == MAGIC && (Version == VERSION || Version == 1);
        }

        void SetMessage(const wchar_t* pMsg) noexcept
        {
            if (pMsg)
                wcsncpy_s(Message, pMsg, _TRUNCATE);
            else
                Message[0] = L'\0';
        }

        void AddMonitorStatus(const XMonitorCaptureStatus& pStatus) noexcept
        {
            if (MonitorStatusCount < CAPTURE_MAX_MONITORS)
            {
                MonitorStatus[MonitorStatusCount++] = pStatus;
                TotalFramesCaptured += pStatus.FramesCaptured;
                TotalBytesWritten += pStatus.BytesWritten;
            }
        }
    };
    #pragma pack(pop)

    static_assert(sizeof(XCaptureResponse) < CAPTURE_PIPE_BUFFER_SIZE,
        "Response packet exceeds pipe buffer size");

    // ========================================================================
    // Utility Functions
    // ========================================================================

    inline std::wstring GetCapturePipeName(DWORD pSessionId) noexcept
    {
        wchar_t buffer[128];
        swprintf_s(buffer, CAPTURE_PIPE_NAME_FORMAT, pSessionId);
        return buffer;
    }

    inline const wchar_t* CaptureActionToString(XCaptureAction pAction) noexcept
    {
        switch (pAction)
        {
            case XCaptureAction::Stop:     return L"Stop";
            case XCaptureAction::Start:    return L"Start";
            case XCaptureAction::Pause:    return L"Pause";
            case XCaptureAction::Resume:   return L"Resume";
            case XCaptureAction::Status:   return L"Status";
            case XCaptureAction::Shutdown: return L"Shutdown";
            case XCaptureAction::Ping:     return L"Ping";
            default:                       return L"Unknown";
        }
    }

    inline const wchar_t* CaptureStatusToString(XCaptureStatus pStatus) noexcept
    {
        switch (pStatus)
        {
            case XCaptureStatus::Success:       return L"Success";
            case XCaptureStatus::Ready:         return L"Ready";
            case XCaptureStatus::Recording:     return L"Recording";
            case XCaptureStatus::Paused:        return L"Paused";
            case XCaptureStatus::Stopped:       return L"Stopped";
            case XCaptureStatus::ErrorGeneral:  return L"Error";
            case XCaptureStatus::ErrorMonitor:  return L"Monitor Not Found";
            case XCaptureStatus::ErrorPath:     return L"Invalid Path";
            case XCaptureStatus::ErrorEncoder:  return L"Encoder Error";
            case XCaptureStatus::ErrorCapture:  return L"Capture Error";
            case XCaptureStatus::ErrorBusy:     return L"Agent Busy";
            case XCaptureStatus::ErrorTimeout:  return L"Timeout";
            case XCaptureStatus::ErrorPipe:     return L"Pipe Error";
            case XCaptureStatus::ErrorDiskSpace:return L"Insufficient Disk Space";
            case XCaptureStatus::ErrorDiskFull: return L"Disk Full";
            default:                            return L"Unknown";
        }
    }

    // Check available disk space
    inline uint64_t GetAvailableDiskSpace(const std::wstring& pPath) noexcept
    {
        ULARGE_INTEGER freeBytesAvailable;
        if (GetDiskFreeSpaceExW(pPath.c_str(), &freeBytesAvailable, nullptr, nullptr))
            return freeBytesAvailable.QuadPart;
        return 0;
    }

    // Generate current timestamp as FILETIME
    inline uint64_t GetCurrentTimestamp() noexcept
    {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    }

} // namespace Tootega
