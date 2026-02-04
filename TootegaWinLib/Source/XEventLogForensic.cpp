// ============================================================================
// XEventLogForensic.cpp - Windows Event Log Forensic Reader Implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XEventLogForensic.h"
#include "XProcess.h"
#include <psapi.h>
#include <iomanip>

#pragma comment(lib, "psapi.lib")

namespace Tootega
{
    // ========================================================================
    // Helper: JSON escape
    // ========================================================================

    namespace
    {
        std::string EscapeJsonString(const std::wstring& pStr)
        {
            std::string result;
            result.reserve(pStr.size());
            for (wchar_t c : pStr)
            {
                if (c == L'"') result += "\\\"";
                else if (c == L'\\') result += "\\\\";
                else if (c == L'\n') result += "\\n";
                else if (c == L'\r') result += "\\r";
                else if (c == L'\t') result += "\\t";
                else if (c < 128) result += static_cast<char>(c);
                else result += '?';
            }
            return result;
        }
    }

    // ========================================================================
    // XForensicEvent Implementation
    // ========================================================================

    std::string XForensicEvent::ToJson() const
    {
        std::ostringstream oss;

        oss << "{"
            << "\"eventId\":" << EventId << ","
            << "\"eventType\":\"" << EscapeJsonString(EventType) << "\","
            << "\"providerName\":\"" << EscapeJsonString(ProviderName) << "\","
            << "\"channel\":\"" << EscapeJsonString(Channel) << "\","
            << "\"timestamp\":\"" << EscapeJsonString(Timestamp) << "\","
            << "\"process\":{"
            << "\"processId\":" << ProcessId << ","
            << "\"processName\":\"" << EscapeJsonString(ProcessName) << "\""
            << "},"
            << "\"user\":{"
            << "\"sid\":\"" << EscapeJsonString(UserSid) << "\","
            << "\"name\":\"" << EscapeJsonString(UserName) << "\""
            << "}";

        if (!Data.empty())
        {
            oss << ",\"data\":{";
            bool first = true;
            for (const auto& [key, value] : Data)
            {
                if (!first) oss << ",";
                first = false;
                oss << "\"" << EscapeJsonString(key) << "\":\"" << EscapeJsonString(value) << "\"";
            }
            oss << "}";
        }

        oss << "}";
        return oss.str();
    }

    std::wstring XForensicEvent::ToJsonW() const
    {
        std::string json = ToJson();
        return std::wstring(json.begin(), json.end());
    }

    // ========================================================================
    // XForensicReport Implementation
    // ========================================================================

    std::string XForensicReport::ToJson(bool pPrettyPrint) const
    {
        std::ostringstream oss;
        const char* nl = pPrettyPrint ? "\n" : "";
        const char* tab = pPrettyPrint ? "  " : "";
        const char* tab2 = pPrettyPrint ? "    " : "";

        oss << "{" << nl;
        oss << tab << "\"report\": {" << nl;
        oss << tab2 << "\"computerName\": \"" << EscapeJsonString(ComputerName) << "\"," << nl;
        oss << tab2 << "\"generatedAt\": \"" << EscapeJsonString(GeneratedAt) << "\"," << nl;
        oss << tab2 << "\"channel\": \"" << EscapeJsonString(Channel) << "\"," << nl;
        oss << tab2 << "\"queryDescription\": \"" << EscapeJsonString(QueryDescription) << "\"" << nl;
        oss << tab << "}," << nl;

        oss << tab << "\"statistics\": {" << nl;
        oss << tab2 << "\"totalEvents\": " << TotalEvents << nl;
        oss << tab << "}," << nl;

        oss << tab << "\"events\": [" << nl;
        
        for (size_t i = 0; i < Events.size(); ++i)
        {
            oss << tab2 << Events[i].ToJson();
            if (i < Events.size() - 1)
                oss << ",";
            oss << nl;
        }
        
        oss << tab << "]" << nl;
        oss << "}" << nl;

        return oss.str();
    }

    std::wstring XForensicReport::ToJsonW(bool pPrettyPrint) const
    {
        std::string json = ToJson(pPrettyPrint);
        return std::wstring(json.begin(), json.end());
    }

    bool XForensicReport::SaveToFile(const std::wstring& pFilePath) const
    {
        std::ofstream file(pFilePath, std::ios::out | std::ios::binary);
        if (!file.is_open())
            return false;

        // Write UTF-8 BOM
        const char bom[] = { '\xEF', '\xBB', '\xBF' };
        file.write(bom, sizeof(bom));

        std::string json = ToJson(true);
        file.write(json.c_str(), json.size());
        
        return file.good();
    }

    // ========================================================================
    // XEventLogForensic - Query Methods
    // ========================================================================

    XForensicReport XEventLogForensic::QueryEvents(
        std::wstring_view pChannel,
        std::wstring_view pXPathQuery,
        std::wstring_view pDescription)
    {
        return ExecuteQuery(pChannel, std::wstring(pXPathQuery), pDescription);
    }

    XForensicReport XEventLogForensic::QueryEventsByIds(
        std::wstring_view pChannel,
        std::span<const DWORD> pEventIds,
        DWORD pHoursBack)
    {
        // Build XPath query for multiple Event IDs
        std::wstring query = L"*[System[(";
        
        for (size_t i = 0; i < pEventIds.size(); ++i)
        {
            if (i > 0) query += L" or ";
            query += std::format(L"EventID={}", pEventIds[i]);
        }
        
        query += L")";

        // Add time filter if specified
        if (pHoursBack > 0)
        {
            FILETIME ftNow;
            GetSystemTimeAsFileTime(&ftNow);

            ULARGE_INTEGER uliNow;
            uliNow.LowPart = ftNow.dwLowDateTime;
            uliNow.HighPart = ftNow.dwHighDateTime;

            ULONGLONG hoursIn100ns = static_cast<ULONGLONG>(pHoursBack) * 60ULL * 60ULL * 10000000ULL;
            ULARGE_INTEGER uliStart;
            uliStart.QuadPart = uliNow.QuadPart - hoursIn100ns;

            FILETIME ftStart;
            ftStart.dwLowDateTime = uliStart.LowPart;
            ftStart.dwHighDateTime = uliStart.HighPart;

            SYSTEMTIME stStart;
            FileTimeToSystemTime(&ftStart, &stStart);

            std::wstring startTime = std::format(
                L"{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.000Z",
                stStart.wYear, stStart.wMonth, stStart.wDay,
                stStart.wHour, stStart.wMinute, stStart.wSecond);

            query += std::format(L" and TimeCreated[@SystemTime >= '{}']", startTime);
        }

        query += L"]]";

        // Build description
        std::wstring desc = L"Event IDs: ";
        for (size_t i = 0; i < pEventIds.size(); ++i)
        {
            if (i > 0) desc += L", ";
            desc += std::to_wstring(pEventIds[i]);
        }
        if (pHoursBack > 0)
            desc += std::format(L" (last {} hours)", pHoursBack);

        return ExecuteQuery(pChannel, query, desc);
    }

    XForensicReport XEventLogForensic::QueryEventsInTimeRange(
        std::wstring_view pChannel,
        DWORD pHoursBack)
    {
        FILETIME ftNow;
        GetSystemTimeAsFileTime(&ftNow);

        ULARGE_INTEGER uliNow;
        uliNow.LowPart = ftNow.dwLowDateTime;
        uliNow.HighPart = ftNow.dwHighDateTime;

        ULONGLONG hoursIn100ns = static_cast<ULONGLONG>(pHoursBack) * 60ULL * 60ULL * 10000000ULL;
        ULARGE_INTEGER uliStart;
        uliStart.QuadPart = uliNow.QuadPart - hoursIn100ns;

        FILETIME ftStart;
        ftStart.dwLowDateTime = uliStart.LowPart;
        ftStart.dwHighDateTime = uliStart.HighPart;

        SYSTEMTIME stStart;
        FileTimeToSystemTime(&ftStart, &stStart);

        std::wstring startTime = std::format(
            L"{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.000Z",
            stStart.wYear, stStart.wMonth, stStart.wDay,
            stStart.wHour, stStart.wMinute, stStart.wSecond);

        std::wstring query = std::format(
            L"*[System[TimeCreated[@SystemTime >= '{}']]]",
            startTime);

        return ExecuteQuery(pChannel, query, std::format(L"Last {} hours", pHoursBack));
    }

    // ========================================================================
    // XEventLogForensic - Export Methods
    // ========================================================================

    bool XEventLogForensic::ExportToJsonFile(
        const XForensicReport& pReport,
        const std::wstring& pFilePath)
    {
        return pReport.SaveToFile(pFilePath);
    }

    std::string XEventLogForensic::ExportToJsonString(
        const XForensicReport& pReport,
        bool pPrettyPrint)
    {
        return pReport.ToJson(pPrettyPrint);
    }

    // ========================================================================
    // XEventLogForensic - Static Utilities
    // ========================================================================

    bool XEventLogForensic::IsChannelEnabled(std::wstring_view pChannel)
    {
        EVT_HANDLE hLog = EvtOpenChannelConfig(nullptr, pChannel.data(), 0);
        if (!hLog)
            return false;

        PEVT_VARIANT pProperty = nullptr;
        DWORD dwBufferUsed = 0;

        EvtGetChannelConfigProperty(hLog, EvtChannelConfigEnabled, 0, 0, nullptr, &dwBufferUsed);
        
        pProperty = static_cast<PEVT_VARIANT>(malloc(dwBufferUsed));
        if (!pProperty)
        {
            EvtClose(hLog);
            return false;
        }

        bool enabled = false;
        if (EvtGetChannelConfigProperty(hLog, EvtChannelConfigEnabled, 0, dwBufferUsed, pProperty, &dwBufferUsed))
            enabled = (pProperty->BooleanVal != FALSE);

        free(pProperty);
        EvtClose(hLog);
        return enabled;
    }

    bool XEventLogForensic::EnableChannel(std::wstring_view pChannel)
    {
        EVT_HANDLE hLog = EvtOpenChannelConfig(nullptr, pChannel.data(), 0);
        if (!hLog)
            return false;

        EVT_VARIANT propValue;
        propValue.Type = EvtVarTypeBoolean;
        propValue.BooleanVal = TRUE;

        bool success = EvtSetChannelConfigProperty(hLog, EvtChannelConfigEnabled, 0, &propValue) != FALSE;
        
        if (success)
            success = EvtSaveChannelConfig(hLog, 0) != FALSE;

        EvtClose(hLog);
        return success;
    }

    std::wstring XEventLogForensic::GetComputerName()
    {
        wchar_t szComputer[MAX_COMPUTERNAME_LENGTH + 1] = {};
        DWORD dwSize = sizeof(szComputer) / sizeof(szComputer[0]);

        if (::GetComputerNameW(szComputer, &dwSize))
            return szComputer;

        return L"UNKNOWN";
    }

    std::wstring XEventLogForensic::GetCurrentTimestamp()
    {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        return FileTimeToIso8601(ft);
    }

    std::wstring XEventLogForensic::FileTimeToIso8601(const FILETIME& pFileTime)
    {
        SYSTEMTIME st;
        FileTimeToSystemTime(&pFileTime, &st);

        return std::format(
            L"{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    }

    std::wstring XEventLogForensic::SidToUserName(const std::wstring& pSid)
    {
        if (pSid.empty())
            return {};

        PSID pSidStruct = nullptr;
        if (!ConvertStringSidToSidW(pSid.c_str(), &pSidStruct))
            return pSid;

        wchar_t szName[256] = {};
        wchar_t szDomain[256] = {};
        DWORD dwNameLen = sizeof(szName) / sizeof(szName[0]);
        DWORD dwDomainLen = sizeof(szDomain) / sizeof(szDomain[0]);
        SID_NAME_USE sidUse;

        std::wstring result;
        if (LookupAccountSidW(nullptr, pSidStruct, szName, &dwNameLen, szDomain, &dwDomainLen, &sidUse))
        {
            if (szDomain[0] != L'\0')
                result = std::format(L"{}\\{}", szDomain, szName);
            else
                result = szName;
        }
        else
        {
            result = pSid;
        }

        LocalFree(pSidStruct);
        return result;
    }

    std::wstring XEventLogForensic::GetProcessNameFromPid(DWORD pPid)
    {
        if (pPid == 0)
            return L"System";

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pPid);
        if (!hProcess)
            return std::format(L"PID:{}", static_cast<unsigned int>(pPid));

        wchar_t szPath[MAX_PATH] = {};
        DWORD dwSize = MAX_PATH;

        std::wstring result;
        if (QueryFullProcessImageNameW(hProcess, 0, szPath, &dwSize))
        {
            std::wstring fullPath(szPath);
            auto lastSlash = fullPath.rfind(L'\\');
            if (lastSlash != std::wstring::npos)
                result = fullPath.substr(lastSlash + 1);
            else
                result = fullPath;
        }
        else
        {
            result = std::format(L"PID:{}", pPid);
        }

        CloseHandle(hProcess);
        return result;
    }

    // ========================================================================
    // XEventLogForensic - Private Methods
    // ========================================================================

    XForensicReport XEventLogForensic::ExecuteQuery(
        std::wstring_view pChannel,
        const std::wstring& pQuery,
        std::wstring_view pDescription)
    {
        XForensicReport report;
        InitializeReport(report, pChannel);
        report.QueryDescription = pDescription;

        EVT_HANDLE hQuery = EvtQuery(
            nullptr,
            pChannel.data(),
            pQuery.c_str(),
            EvtQueryChannelPath | EvtQueryReverseDirection);

        if (!hQuery)
        {
            DWORD dwErr = GetLastError();
            std::wostringstream oss;
            oss << L"EvtQuery failed: 0x" << std::hex << std::setw(8) << std::setfill(L'0') << dwErr;
            _LastError = oss.str();
            return report;
        }

        constexpr DWORD BATCH_SIZE = 100;
        EVT_HANDLE hEvents[BATCH_SIZE];
        DWORD dwReturned = 0;

        while (true)
        {
            if (!EvtNext(hQuery, BATCH_SIZE, hEvents, INFINITE, 0, &dwReturned))
            {
                DWORD dwErr = GetLastError();
                if (dwErr != ERROR_NO_MORE_ITEMS)
                {
                    std::wostringstream oss;
                    oss << L"EvtNext failed: 0x" << std::hex << std::setw(8) << std::setfill(L'0') << dwErr;
                    _LastError = oss.str();
                }
                break;
            }

            for (DWORD i = 0; i < dwReturned; ++i)
            {
                auto evt = ParseEvent(hEvents[i]);
                evt.Channel = std::wstring(pChannel);
                
                if (evt.IsValid())
                    report.Events.push_back(std::move(evt));

                EvtClose(hEvents[i]);

                if (_MaxEvents > 0 && report.Events.size() >= _MaxEvents)
                    break;
            }

            if (_MaxEvents > 0 && report.Events.size() >= _MaxEvents)
                break;
        }

        EvtClose(hQuery);

        report.TotalEvents = static_cast<DWORD>(report.Events.size());
        return report;
    }

    XForensicEvent XEventLogForensic::ParseEvent(EVT_HANDLE pEvent)
    {
        XForensicEvent evt;

        std::wstring xml = GetEventXml(pEvent);
        if (xml.empty())
            return evt;

        if (_IncludeRawXml)
            evt.RawXml = xml;

        // Parse EventID
        std::wstring eventIdStr = ExtractXmlValue(xml, L"<EventID>", L"</EventID>");
        if (eventIdStr.empty())
            eventIdStr = ExtractXmlValue(xml, L"<EventID Qualifiers=", L"</EventID>");
        
        if (!eventIdStr.empty())
        {
            // Handle <EventID Qualifiers="...">123</EventID>
            auto gtPos = eventIdStr.find(L'>');
            if (gtPos != std::wstring::npos)
                eventIdStr = eventIdStr.substr(gtPos + 1);
            evt.EventId = static_cast<DWORD>(_wtoi(eventIdStr.c_str()));
        }

        // Parse Provider Name
        evt.ProviderName = ExtractXmlAttribute(xml, L"Provider", L"Name");

        // Parse TimeCreated
        std::wstring timeStr = ExtractXmlAttribute(xml, L"TimeCreated", L"SystemTime");
        evt.Timestamp = timeStr;

        if (!timeStr.empty())
        {
            SYSTEMTIME st{};
            swscanf_s(timeStr.c_str(), 
                L"%4hu-%2hu-%2huT%2hu:%2hu:%2hu",
                &st.wYear, &st.wMonth, &st.wDay,
                &st.wHour, &st.wMinute, &st.wSecond);
            SystemTimeToFileTime(&st, &evt.FileTime);
        }

        // Parse Process ID
        std::wstring pidStr = ExtractXmlAttribute(xml, L"Execution", L"ProcessID");
        if (!pidStr.empty())
            evt.ProcessId = static_cast<DWORD>(_wtoi(pidStr.c_str()));

        if (evt.ProcessId > 0)
            evt.ProcessName = GetProcessNameFromPid(evt.ProcessId);

        // Parse User SID
        evt.UserSid = ExtractXmlAttribute(xml, L"Security", L"UserID");
        if (!evt.UserSid.empty())
            evt.UserName = SidToUserName(evt.UserSid);

        // Parse EventData section
        ParseEventData(xml, evt);

        return evt;
    }

    std::wstring XEventLogForensic::GetEventXml(EVT_HANDLE pEvent)
    {
        DWORD dwBufferUsed = 0;
        DWORD dwPropertyCount = 0;

        EvtRender(nullptr, pEvent, EvtRenderEventXml, 0, nullptr, &dwBufferUsed, &dwPropertyCount);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return {};

        std::wstring xml(dwBufferUsed / sizeof(wchar_t), L'\0');

        if (!EvtRender(nullptr, pEvent, EvtRenderEventXml, dwBufferUsed, 
                       xml.data(), &dwBufferUsed, &dwPropertyCount))
            return {};

        while (!xml.empty() && xml.back() == L'\0')
            xml.pop_back();

        return xml;
    }

    std::wstring XEventLogForensic::ExtractXmlValue(
        std::wstring_view pXml,
        std::wstring_view pStartTag,
        std::wstring_view pEndTag)
    {
        auto startPos = pXml.find(pStartTag);
        if (startPos == std::wstring_view::npos)
            return {};

        startPos += pStartTag.length();
        auto endPos = pXml.find(pEndTag, startPos);
        if (endPos == std::wstring_view::npos)
            return {};

        return std::wstring(pXml.substr(startPos, endPos - startPos));
    }

    std::wstring XEventLogForensic::ExtractXmlAttribute(
        std::wstring_view pXml,
        std::wstring_view pElement,
        std::wstring_view pAttribute)
    {
        std::wstring elementStart = std::format(L"<{}", pElement);
        auto elemPos = pXml.find(elementStart);
        if (elemPos == std::wstring_view::npos)
            return {};

        auto elemEnd = pXml.find(L'>', elemPos);
        if (elemEnd == std::wstring_view::npos)
            return {};

        std::wstring attrPattern = std::format(L"{}=\"", pAttribute);
        auto attrPos = pXml.find(attrPattern, elemPos);
        if (attrPos == std::wstring_view::npos || attrPos > elemEnd)
            return {};

        attrPos += attrPattern.length();
        auto attrEnd = pXml.find(L'"', attrPos);
        if (attrEnd == std::wstring_view::npos)
            return {};

        return std::wstring(pXml.substr(attrPos, attrEnd - attrPos));
    }

    void XEventLogForensic::ParseEventData(std::wstring_view pXml, XForensicEvent& pEvent)
    {
        // Find <EventData> section
        auto dataStart = pXml.find(L"<EventData>");
        if (dataStart == std::wstring_view::npos)
            return;

        auto dataEnd = pXml.find(L"</EventData>", dataStart);
        if (dataEnd == std::wstring_view::npos)
            return;

        std::wstring_view eventData = pXml.substr(dataStart, dataEnd - dataStart);

        // Parse <Data Name="...">...</Data> elements
        size_t pos = 0;
        while (true)
        {
            auto dataElem = eventData.find(L"<Data Name=\"", pos);
            if (dataElem == std::wstring_view::npos)
                break;

            dataElem += 12; // Skip <Data Name="
            auto nameEnd = eventData.find(L'"', dataElem);
            if (nameEnd == std::wstring_view::npos)
                break;

            std::wstring name(eventData.substr(dataElem, nameEnd - dataElem));

            auto valueStart = eventData.find(L'>', nameEnd);
            if (valueStart == std::wstring_view::npos)
                break;
            valueStart++;

            auto valueEnd = eventData.find(L"</Data>", valueStart);
            if (valueEnd == std::wstring_view::npos)
                break;

            std::wstring value(eventData.substr(valueStart, valueEnd - valueStart));

            pEvent.Data.emplace_back(std::move(name), std::move(value));

            pos = valueEnd + 7; // Skip </Data>
        }
    }

    void XEventLogForensic::InitializeReport(XForensicReport& pReport, std::wstring_view pChannel)
    {
        pReport.ComputerName = GetComputerName();
        pReport.GeneratedAt = GetCurrentTimestamp();
        pReport.Channel = pChannel;
        pReport.QueryDescription.clear();
        pReport.TotalEvents = 0;
        pReport.Events.clear();
    }

} // namespace Tootega
