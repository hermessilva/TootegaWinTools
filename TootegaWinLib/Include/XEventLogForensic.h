#pragma once
// ============================================================================
// XEventLogForensic.h - Windows Event Log Forensic Reader and Exporter
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================
//
// Generic forensics class for reading and exporting Windows event logs.
// Supports multiple channels: CAPI2, Security, Application, System.
// Provides JSON export for audit and incident response.
//
// ============================================================================

#include "XPlatform.h"
#include <winevt.h>
#include <sddl.h>
#include <fstream>
#include <sstream>

#pragma comment(lib, "wevtapi.lib")

namespace Tootega
{
    // ========================================================================
    // XForensicEvent - Parsed event structure (generic)
    // ========================================================================

    struct XForensicEvent
    {
        // Event identification
        DWORD           EventId{0};
        std::wstring    EventType;
        std::wstring    ProviderName;
        std::wstring    Channel;
        
        // Timestamp
        FILETIME        FileTime{};
        std::wstring    Timestamp;
        
        // Process information
        DWORD           ProcessId{0};
        std::wstring    ProcessName;
        
        // User information
        std::wstring    UserSid;
        std::wstring    UserName;
        
        // Event-specific data (key-value pairs)
        std::vector<std::pair<std::wstring, std::wstring>> Data;
        
        // Raw data
        std::wstring    RawXml;

        [[nodiscard]] bool IsValid() const noexcept { return EventId != 0; }
        
        [[nodiscard]] std::wstring GetDataValue(const std::wstring& pKey) const
        {
            for (const auto& [key, value] : Data)
                if (key == pKey)
                    return value;
            return {};
        }
        
        [[nodiscard]] std::string ToJson() const;
        [[nodiscard]] std::wstring ToJsonW() const;
    };

    // ========================================================================
    // XForensicReport - Collection of forensic events
    // ========================================================================

    struct XForensicReport
    {
        std::wstring                    ComputerName;
        std::wstring                    GeneratedAt;
        std::wstring                    Channel;
        std::wstring                    QueryDescription;
        DWORD                           TotalEvents{0};
        std::vector<XForensicEvent>     Events;

        [[nodiscard]] std::string ToJson(bool pPrettyPrint = true) const;
        [[nodiscard]] std::wstring ToJsonW(bool pPrettyPrint = true) const;
        
        bool SaveToFile(const std::wstring& pFilePath) const;
    };

    // ========================================================================
    // XEventLogForensic - Generic event log reader and exporter
    // ========================================================================

    class XEventLogForensic final
    {
    public:
        XEventLogForensic() = default;
        ~XEventLogForensic() = default;

        XEventLogForensic(const XEventLogForensic&) = delete;
        XEventLogForensic& operator=(const XEventLogForensic&) = delete;

        // ====================================================================
        // Query Methods
        // ====================================================================

        /// Query events using XPath query
        [[nodiscard]] XForensicReport QueryEvents(
            std::wstring_view pChannel,
            std::wstring_view pXPathQuery,
            std::wstring_view pDescription = L"");

        /// Query events by Event IDs within time range
        [[nodiscard]] XForensicReport QueryEventsByIds(
            std::wstring_view pChannel,
            std::span<const DWORD> pEventIds,
            DWORD pHoursBack = 0);

        /// Query all events in time range
        [[nodiscard]] XForensicReport QueryEventsInTimeRange(
            std::wstring_view pChannel,
            DWORD pHoursBack);

        // ====================================================================
        // Export Methods
        // ====================================================================

        /// Export report to JSON file
        bool ExportToJsonFile(
            const XForensicReport& pReport,
            const std::wstring& pFilePath);

        /// Export report to JSON string
        [[nodiscard]] std::string ExportToJsonString(
            const XForensicReport& pReport,
            bool pPrettyPrint = true);

        // ====================================================================
        // Configuration
        // ====================================================================

        /// Set maximum events to retrieve (0 = unlimited)
        void SetMaxEvents(DWORD pMaxEvents) noexcept { _MaxEvents = pMaxEvents; }

        /// Include raw XML in events
        void SetIncludeRawXml(bool pInclude) noexcept { _IncludeRawXml = pInclude; }

        /// Get last error message
        [[nodiscard]] const std::wstring& GetLastErrorMessage() const noexcept { return _LastError; }

        // ====================================================================
        // Static Utilities
        // ====================================================================

        /// Check if a log channel is enabled
        [[nodiscard]] static bool IsChannelEnabled(std::wstring_view pChannel);

        /// Enable a log channel (requires admin)
        [[nodiscard]] static bool EnableChannel(std::wstring_view pChannel);

        /// Get current computer name
        [[nodiscard]] static std::wstring GetComputerName();

        /// Get current timestamp in ISO 8601
        [[nodiscard]] static std::wstring GetCurrentTimestamp();

        /// Convert FILETIME to ISO 8601
        [[nodiscard]] static std::wstring FileTimeToIso8601(const FILETIME& pFileTime);

        /// Convert SID string to user name
        [[nodiscard]] static std::wstring SidToUserName(const std::wstring& pSid);

        /// Get process name from PID
        [[nodiscard]] static std::wstring GetProcessNameFromPid(DWORD pPid);

        // ====================================================================
        // Well-Known Channels
        // ====================================================================

        static constexpr std::wstring_view ChannelSecurity = 
            L"Security";
        static constexpr std::wstring_view ChannelCapi2 = 
            L"Microsoft-Windows-CAPI2/Operational";
        static constexpr std::wstring_view ChannelSystem = 
            L"System";
        static constexpr std::wstring_view ChannelApplication = 
            L"Application";

    private:
        // ====================================================================
        // Private Methods
        // ====================================================================

        [[nodiscard]] XForensicReport ExecuteQuery(
            std::wstring_view pChannel,
            const std::wstring& pQuery,
            std::wstring_view pDescription);

        [[nodiscard]] XForensicEvent ParseEvent(EVT_HANDLE pEvent);

        [[nodiscard]] std::wstring GetEventXml(EVT_HANDLE pEvent);

        [[nodiscard]] std::wstring ExtractXmlValue(
            std::wstring_view pXml,
            std::wstring_view pStartTag,
            std::wstring_view pEndTag);

        [[nodiscard]] std::wstring ExtractXmlAttribute(
            std::wstring_view pXml,
            std::wstring_view pElement,
            std::wstring_view pAttribute);

        void ParseEventData(std::wstring_view pXml, XForensicEvent& pEvent);

        void InitializeReport(XForensicReport& pReport, std::wstring_view pChannel);

        // ====================================================================
        // Private Members
        // ====================================================================

        DWORD           _MaxEvents{0};
        bool            _IncludeRawXml{false};
        std::wstring    _LastError;
    };

} // namespace Tootega
