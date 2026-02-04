#pragma once
// ============================================================================
// XEventLog.h - Windows Event Log integration
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XPlatform.h"
#include "XTypes.h"

namespace Tootega
{
    // ========================================================================
    // Event Log Configuration
    // ========================================================================

    namespace XEventLogConfig
    {
        inline constexpr wchar_t LogName[]         = L"Tootega";
        inline constexpr wchar_t ProviderName[]    = L"Tootega";
        inline constexpr wchar_t KSPSourceName[]   = L"TootegaKSP";
        inline constexpr wchar_t MonitorSourceName[] = L"TootegaMonitor";
        inline constexpr wchar_t InstallerSourceName[] = L"TootegaInstaller";
        inline constexpr wchar_t AuditSourceName[]   = L"TootegaAudit";
        inline constexpr wchar_t ForensicSourceName[] = L"TootegaForensic";
        
        inline constexpr wchar_t LogRegistryPath[] = L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Tootega";
        inline constexpr wchar_t SourceRegistryPath[] = L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Tootega\\";
        
        inline constexpr DWORD CategoryCount = 5;
        inline constexpr WORD CategoryKSP       = 1;
        inline constexpr WORD CategoryMonitor   = 2;
        inline constexpr WORD CategoryInstaller = 3;
        inline constexpr WORD CategoryAudit     = 4;
        inline constexpr WORD CategoryForensic  = 5;
    }

    // ========================================================================
    // Event IDs
    // ========================================================================

    enum class XEventID : DWORD
    {
        // General Events (1-99)
        GenericInfo       = 1,
        GenericWarning    = 2,
        GenericError      = 3,

        // KSP Events (100-199)
        KSPInstalled      = 100,
        KSPUninstalled    = 101,
        KSPInitialized    = 102,
        KSPShutdown       = 103,
        KSPKeyCreated     = 110,
        KSPKeyDeleted     = 111,
        KSPKeyAccessed    = 112,
        KSPSignOperation  = 120,
        KSPDecryptOperation = 121,
        KSPError          = 199,

        // Monitor Events (200-299)
        MonitorStarted    = 200,
        MonitorStopped    = 201,
        MonitorServiceInstalled   = 202,
        MonitorServiceUninstalled = 203,
        MonitorCertificateDetected = 210,
        MonitorCertificateMigrated = 211,
        MonitorCertificateUsage    = 212,
        MonitorAPISync    = 220,
        MonitorError      = 299,

        // Installer Events (300-399)
        InstallerStarted          = 300,
        InstallerCompleted        = 301,
        InstallerFailed           = 302,
        InstallerUpgradeStarted   = 310,
        InstallerUpgradeCompleted = 311,
        InstallerUpgradeFailed    = 312,
        UninstallerStarted        = 320,
        UninstallerCompleted      = 321,
        UninstallerBlocked        = 322,
        UninstallerUnauthorized   = 323,
        InstallerError            = 399,

        // Audit Events (400-499)
        AuditFileAccess           = 400,
        AuditFileModified         = 401,
        AuditFileTampered         = 402,
        AuditRegistryAccess       = 410,
        AuditRegistryModified     = 411,
        AuditRegistryTampered     = 412,
        AuditServiceControlAttempt = 420,
        AuditServiceStopBlocked   = 421,
        AuditServicePauseBlocked  = 422,
        AuditServiceDisableBlocked = 423,
        AuditIntegrityCheckPassed = 430,
        AuditIntegrityCheckFailed = 431,
        AuditSecurityDescriptorSet = 440,
        AuditSecurityDescriptorFailed = 441,
        AuditError                = 499,

        // Forensic Events (500-599)
        ForensicHashCalculated    = 500,
        ForensicHashVerified      = 501,
        ForensicHashMismatch      = 502,
        ForensicProcessAttach     = 510,
        ForensicProcessDetach     = 511,
        ForensicDLLInjectionBlocked = 512,
        ForensicMemoryTampering   = 513,
        ForensicDebuggerDetected  = 520,
        ForensicDebuggerBlocked   = 521,
        ForensicPrivilegeEscalation = 530,
        ForensicSuspiciousActivity = 540,
        ForensicEvidenceCollected = 550,
        ForensicError             = 599
    };

    // ========================================================================
    // Event Types
    // ========================================================================

    enum class XEventType : WORD
    {
        Success     = EVENTLOG_SUCCESS,
        Error       = EVENTLOG_ERROR_TYPE,
        Warning     = EVENTLOG_WARNING_TYPE,
        Information = EVENTLOG_INFORMATION_TYPE,
        AuditSuccess = EVENTLOG_AUDIT_SUCCESS,
        AuditFailure = EVENTLOG_AUDIT_FAILURE
    };

    // ========================================================================
    // XEventLog - Windows Event Log wrapper
    // ========================================================================

    class XEventLog final
    {
    public:
        explicit XEventLog(std::wstring_view pSourceName) noexcept;
        ~XEventLog() noexcept;

        TOOTEGA_DISABLE_COPY_MOVE(XEventLog);

        [[nodiscard]] bool Open() noexcept;
        void Close() noexcept;
        [[nodiscard]] bool IsOpen() const noexcept { return _Handle != nullptr; }

        bool ReportEvent(
            XEventType pType,
            XEventID   pEventID,
            WORD       pCategory,
            std::wstring_view pMessage) noexcept;

        template<typename... TArgs>
        bool ReportInfo(XEventID pEventID, WORD pCategory, std::wstring_view pFormat, TArgs&&... pArgs)
        {
            std::wstring message = FormatMessage(pFormat, std::forward<TArgs>(pArgs)...);
            return ReportEvent(XEventType::Information, pEventID, pCategory, message);
        }

        template<typename... TArgs>
        bool ReportWarning(XEventID pEventID, WORD pCategory, std::wstring_view pFormat, TArgs&&... pArgs)
        {
            std::wstring message = FormatMessage(pFormat, std::forward<TArgs>(pArgs)...);
            return ReportEvent(XEventType::Warning, pEventID, pCategory, message);
        }

        template<typename... TArgs>
        bool ReportError(XEventID pEventID, WORD pCategory, std::wstring_view pFormat, TArgs&&... pArgs)
        {
            std::wstring message = FormatMessage(pFormat, std::forward<TArgs>(pArgs)...);
            return ReportEvent(XEventType::Error, pEventID, pCategory, message);
        }

        template<typename... TArgs>
        bool ReportSuccess(XEventID pEventID, WORD pCategory, std::wstring_view pFormat, TArgs&&... pArgs)
        {
            std::wstring message = FormatMessage(pFormat, std::forward<TArgs>(pArgs)...);
            return ReportEvent(XEventType::Success, pEventID, pCategory, message);
        }

        // Static registration methods
        [[nodiscard]] static bool RegisterSource(std::wstring_view pSourceName) noexcept;
        [[nodiscard]] static bool UnregisterSource(std::wstring_view pSourceName) noexcept;
        [[nodiscard]] static bool IsSourceRegistered(std::wstring_view pSourceName) noexcept;

        // Convenience static methods for quick logging
        static bool LogKSPInfo(XEventID pEventID, std::wstring_view pMessage) noexcept;
        static bool LogKSPWarning(XEventID pEventID, std::wstring_view pMessage) noexcept;
        static bool LogKSPError(XEventID pEventID, std::wstring_view pMessage) noexcept;

        static bool LogMonitorInfo(XEventID pEventID, std::wstring_view pMessage) noexcept;
        static bool LogMonitorWarning(XEventID pEventID, std::wstring_view pMessage) noexcept;
        static bool LogMonitorError(XEventID pEventID, std::wstring_view pMessage) noexcept;

        // Convenience overloads with generic event IDs for ad-hoc logging
        static bool LogMonitorInfo(std::wstring_view pMessage) noexcept;
        static bool LogMonitorWarning(std::wstring_view pMessage) noexcept;
        static bool LogMonitorError(std::wstring_view pMessage) noexcept;

    private:
        template<typename... TArgs>
        [[nodiscard]] static std::wstring FormatMessage(std::wstring_view pFormat, TArgs&&... pArgs)
        {
            if constexpr (sizeof...(pArgs) > 0)
                return std::vformat(pFormat, std::make_wformat_args(pArgs...));
            else
                return std::wstring(pFormat);
        }

        std::wstring _SourceName;
        HANDLE       _Handle{nullptr};
    };

    // ========================================================================
    // XEventLogRegistrar - RAII registration helper
    // ========================================================================

    class XEventLogRegistrar final
    {
    public:
        XEventLogRegistrar() = default;
        ~XEventLogRegistrar() = default;

        TOOTEGA_DISABLE_COPY_MOVE(XEventLogRegistrar);

        [[nodiscard]] static bool RegisterAllSources() noexcept;
        [[nodiscard]] static bool UnregisterAllSources() noexcept;
    };

} // namespace Tootega

