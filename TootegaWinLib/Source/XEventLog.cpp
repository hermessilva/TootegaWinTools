// ============================================================================
// XEventLog.cpp - Windows Event Log integration implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XEventLog.h"
#include "XRegistry.h"

namespace Tootega
{
    // ========================================================================
    // XEventLog Implementation
    // ========================================================================

    XEventLog::XEventLog(std::wstring_view pSourceName) noexcept
        : _SourceName(pSourceName)
    {
    }

    XEventLog::~XEventLog() noexcept
    {
        Close();
    }

    bool XEventLog::Open() noexcept
    {
        if (_Handle != nullptr)
            return true;

        _Handle = ::RegisterEventSourceW(nullptr, _SourceName.c_str());
        return _Handle != nullptr;
    }

    void XEventLog::Close() noexcept
    {
        if (_Handle != nullptr)
        {
            DeregisterEventSource(_Handle);
            _Handle = nullptr;
        }
    }

    bool XEventLog::ReportEvent(
        XEventType pType,
        XEventID   pEventID,
        WORD       pCategory,
        std::wstring_view pMessage) noexcept
    {
        if (_Handle == nullptr)
            if (!Open())
                return false;

        LPCWSTR messages[] = { pMessage.data() };

        return ::ReportEventW(
            _Handle,
            static_cast<WORD>(pType),
            pCategory,
            static_cast<DWORD>(pEventID),
            nullptr,
            1,
            0,
            messages,
            nullptr) != FALSE;
    }

    // ========================================================================
    // Static Registration Methods
    // ========================================================================

    bool XEventLog::RegisterSource(std::wstring_view pSourceName) noexcept
    {
        // First, ensure the custom log exists
        HKEY hLogKey = nullptr;
        DWORD disposition = 0;

        LSTATUS result = RegCreateKeyExW(
            HKEY_LOCAL_MACHINE,
            XEventLogConfig::LogRegistryPath,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            nullptr,
            &hLogKey,
            &disposition);

        if (result == ERROR_SUCCESS)
        {
            // Set max log size (16 MB)
            DWORD maxSize = 16 * 1024 * 1024;
            RegSetValueExW(hLogKey, L"MaxSize", 0, REG_DWORD,
                reinterpret_cast<const BYTE*>(&maxSize), sizeof(DWORD));

            // Set retention (0 = overwrite as needed)
            DWORD retention = 0;
            RegSetValueExW(hLogKey, L"Retention", 0, REG_DWORD,
                reinterpret_cast<const BYTE*>(&retention), sizeof(DWORD));

            // Set log file path
            std::wstring logFile = L"%SystemRoot%\\System32\\Winevt\\Logs\\Tootega.evtx";
            RegSetValueExW(hLogKey, L"File", 0, REG_EXPAND_SZ,
                reinterpret_cast<const BYTE*>(logFile.c_str()),
                static_cast<DWORD>((logFile.size() + 1) * sizeof(wchar_t)));

            RegCloseKey(hLogKey);
        }

        // Now register the event source under the custom log
        std::wstring keyPath = XEventLogConfig::SourceRegistryPath;
        keyPath += pSourceName;

        HKEY hKey = nullptr;

        result = RegCreateKeyExW(
            HKEY_LOCAL_MACHINE,
            keyPath.c_str(),
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            nullptr,
            &hKey,
            &disposition);

        if (result != ERROR_SUCCESS)
            return false;

        wchar_t modulePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

        result = RegSetValueExW(
            hKey,
            L"EventMessageFile",
            0,
            REG_EXPAND_SZ,
            reinterpret_cast<const BYTE*>(modulePath),
            static_cast<DWORD>((wcslen(modulePath) + 1) * sizeof(wchar_t)));

        if (result == ERROR_SUCCESS)
        {
            DWORD typesSupported = EVENTLOG_SUCCESS | 
                                   EVENTLOG_ERROR_TYPE | 
                                   EVENTLOG_WARNING_TYPE | 
                                   EVENTLOG_INFORMATION_TYPE;

            result = RegSetValueExW(
                hKey,
                L"TypesSupported",
                0,
                REG_DWORD,
                reinterpret_cast<const BYTE*>(&typesSupported),
                sizeof(DWORD));
        }

        if (result == ERROR_SUCCESS)
        {
            DWORD categoryCount = XEventLogConfig::CategoryCount;
            result = RegSetValueExW(
                hKey,
                L"CategoryCount",
                0,
                REG_DWORD,
                reinterpret_cast<const BYTE*>(&categoryCount),
                sizeof(DWORD));
        }

        if (result == ERROR_SUCCESS)
        {
            result = RegSetValueExW(
                hKey,
                L"CategoryMessageFile",
                0,
                REG_EXPAND_SZ,
                reinterpret_cast<const BYTE*>(modulePath),
                static_cast<DWORD>((wcslen(modulePath) + 1) * sizeof(wchar_t)));
        }

        RegCloseKey(hKey);
        return result == ERROR_SUCCESS;
    }

    bool XEventLog::UnregisterSource(std::wstring_view pSourceName) noexcept
    {
        std::wstring keyPath = XEventLogConfig::SourceRegistryPath;
        keyPath += pSourceName;

        return RegDeleteTreeW(HKEY_LOCAL_MACHINE, keyPath.c_str()) == ERROR_SUCCESS;
    }

    bool XEventLog::IsSourceRegistered(std::wstring_view pSourceName) noexcept
    {
        std::wstring keyPath = XEventLogConfig::SourceRegistryPath;
        keyPath += pSourceName;

        HKEY hKey = nullptr;
        LSTATUS result = RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            keyPath.c_str(),
            0,
            KEY_READ,
            &hKey);

        if (result == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return true;
        }

        return false;
    }

    // ========================================================================
    // Static Convenience Methods - KSP
    // ========================================================================

    bool XEventLog::LogKSPInfo(XEventID pEventID, std::wstring_view pMessage) noexcept
    {
        XEventLog log(XEventLogConfig::KSPSourceName);
        return log.ReportEvent(XEventType::Information, pEventID, XEventLogConfig::CategoryKSP, pMessage);
    }

    bool XEventLog::LogKSPWarning(XEventID pEventID, std::wstring_view pMessage) noexcept
    {
        XEventLog log(XEventLogConfig::KSPSourceName);
        return log.ReportEvent(XEventType::Warning, pEventID, XEventLogConfig::CategoryKSP, pMessage);
    }

    bool XEventLog::LogKSPError(XEventID pEventID, std::wstring_view pMessage) noexcept
    {
        XEventLog log(XEventLogConfig::KSPSourceName);
        return log.ReportEvent(XEventType::Error, pEventID, XEventLogConfig::CategoryKSP, pMessage);
    }

    // ========================================================================
    // Static Convenience Methods - Monitor
    // ========================================================================

    bool XEventLog::LogMonitorInfo(XEventID pEventID, std::wstring_view pMessage) noexcept
    {
        XEventLog log(XEventLogConfig::MonitorSourceName);
        return log.ReportEvent(XEventType::Information, pEventID, XEventLogConfig::CategoryMonitor, pMessage);
    }

    bool XEventLog::LogMonitorWarning(XEventID pEventID, std::wstring_view pMessage) noexcept
    {
        XEventLog log(XEventLogConfig::MonitorSourceName);
        return log.ReportEvent(XEventType::Warning, pEventID, XEventLogConfig::CategoryMonitor, pMessage);
    }

    bool XEventLog::LogMonitorError(XEventID pEventID, std::wstring_view pMessage) noexcept
    {
        XEventLog log(XEventLogConfig::MonitorSourceName);
        return log.ReportEvent(XEventType::Error, pEventID, XEventLogConfig::CategoryMonitor, pMessage);
    }

    // Convenience overloads with generic event IDs
    bool XEventLog::LogMonitorInfo(std::wstring_view pMessage) noexcept
    {
        return LogMonitorInfo(XEventID::GenericInfo, pMessage);
    }

    bool XEventLog::LogMonitorWarning(std::wstring_view pMessage) noexcept
    {
        return LogMonitorWarning(XEventID::GenericWarning, pMessage);
    }

    bool XEventLog::LogMonitorError(std::wstring_view pMessage) noexcept
    {
        return LogMonitorError(XEventID::GenericError, pMessage);
    }

    // ========================================================================
    // XEventLogRegistrar Implementation
    // ========================================================================

    bool XEventLogRegistrar::RegisterAllSources() noexcept
    {
        bool kspOk = XEventLog::RegisterSource(XEventLogConfig::KSPSourceName);
        bool monitorOk = XEventLog::RegisterSource(XEventLogConfig::MonitorSourceName);
        return kspOk && monitorOk;
    }

    bool XEventLogRegistrar::UnregisterAllSources() noexcept
    {
        bool kspOk = XEventLog::UnregisterSource(XEventLogConfig::KSPSourceName);
        bool monitorOk = XEventLog::UnregisterSource(XEventLogConfig::MonitorSourceName);
        return kspOk && monitorOk;
    }

} // namespace Tootega

