// ============================================================================
// XLogger.cpp - Thread-safe logging system implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XLogger.h"
#include "XEventLog.h"
#include "XFile.h"
#include "XString.h"

namespace Tootega
{
    XLogger& XLogger::Instance() noexcept
    {
        static XLogger instance;
        return instance;
    }

    XLogger::XLogger()
    {
    }

    XLogger::~XLogger()
    {
        Shutdown();
    }

    void XLogger::Initialize(std::wstring_view pAppName, std::wstring_view pLogDirectory)
    {
        std::lock_guard lock(_Mutex);

        if (_Initialized.load(std::memory_order_relaxed))
            return;

        _AppName = pAppName;

        if (pLogDirectory.empty())
        {
            wchar_t path[MAX_PATH]{};
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path)))
            {
                _LogDirectory = path;
                _LogDirectory += L"\\Tootega\\";
                _LogDirectory += _AppName;
                _LogDirectory += L"\\Logs";
            }
        }
        else
        {
            _LogDirectory = pLogDirectory;
        }

        EnsureLogDirectory();
        _Initialized.store(true, std::memory_order_relaxed);
    }

    void XLogger::Shutdown() noexcept
    {
        std::lock_guard lock(_Mutex);

        if (_FileHandle != INVALID_HANDLE_VALUE)
        {
            FlushFileBuffers(_FileHandle);
            CloseHandle(_FileHandle);
            _FileHandle = INVALID_HANDLE_VALUE;
        }

        _Initialized.store(false, std::memory_order_relaxed);
    }

    void XLogger::EnsureLogDirectory()
    {
        if (_LogDirectory.empty())
            return;

        CreateDirectoryW(_LogDirectory.c_str(), nullptr);

        size_t pos = 0;
        while ((pos = _LogDirectory.find(L'\\', pos + 1)) != std::wstring::npos)
        {
            std::wstring subPath = _LogDirectory.substr(0, pos);
            CreateDirectoryW(subPath.c_str(), nullptr);
        }
        CreateDirectoryW(_LogDirectory.c_str(), nullptr);
    }

    void XLogger::WriteLog(XLogLevel pLevel, std::wstring_view pMessage)
    {
        auto timestamp = GetTimestamp();
        auto levelStr = LevelToString(pLevel);

        std::wstring line = std::format(L"[{}] [{}] {}\r\n", timestamp, levelStr, pMessage);

        std::lock_guard lock(_Mutex);

        auto targets = _Targets.load(std::memory_order_relaxed);

        if (HasFlag(targets, XLogTarget::Console))
            WriteToConsole(pLevel, timestamp, pMessage);

        if (HasFlag(targets, XLogTarget::DebugOutput))
            WriteToDebugOutput(line);

        if (HasFlag(targets, XLogTarget::File))
            WriteToFile(line);

        if (HasFlag(targets, XLogTarget::EventLog))
            WriteToEventLog(pLevel, pMessage);
    }

    void XLogger::WriteToConsole(XLogLevel pLevel, std::wstring_view pTimestamp, std::wstring_view pMessage)
    {
        if (_ServiceMode.load(std::memory_order_relaxed))
            return;

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole == INVALID_HANDLE_VALUE)
            return;

        WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        switch (pLevel)
        {
        case XLogLevel::Trace:
            color = FOREGROUND_INTENSITY;
            break;
        case XLogLevel::Debug:
            color = FOREGROUND_GREEN | FOREGROUND_BLUE;
            break;
        case XLogLevel::Info:
            color = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            break;
        case XLogLevel::Warning:
            color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            break;
        case XLogLevel::Error:
            color = FOREGROUND_RED | FOREGROUND_INTENSITY;
            break;
        case XLogLevel::Critical:
            color = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            break;
        default:
            break;
        }

        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        GetConsoleScreenBufferInfo(hConsole, &csbi);

        SetConsoleTextAttribute(hConsole, color);

        auto line = std::format(L"[{}] [{}] {}\n", pTimestamp, LevelToString(pLevel), pMessage);
        DWORD written = 0;
        WriteConsoleW(hConsole, line.c_str(), static_cast<DWORD>(line.size()), &written, nullptr);

        SetConsoleTextAttribute(hConsole, csbi.wAttributes);
    }

    void XLogger::WriteToFile(std::wstring_view pLine)
    {
        if (_LogDirectory.empty())
            return;

        auto dateStr = GetDateString();
        if (dateStr != _CurrentDate || _FileHandle == INVALID_HANDLE_VALUE)
        {
            if (_FileHandle != INVALID_HANDLE_VALUE)
            {
                FlushFileBuffers(_FileHandle);
                CloseHandle(_FileHandle);
                _FileHandle = INVALID_HANDLE_VALUE;
            }

            _CurrentDate = dateStr;
            _CurrentLogFile = std::format(L"{}\\{}_{}.log", _LogDirectory, _AppName, _CurrentDate);

            _FileHandle = CreateFileW(
                _CurrentLogFile.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ,
                nullptr,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

            if (_FileHandle != INVALID_HANDLE_VALUE)
                SetFilePointer(_FileHandle, 0, nullptr, FILE_END);
        }

        if (_FileHandle != INVALID_HANDLE_VALUE)
        {
            std::string utf8Line;
            int size = WideCharToMultiByte(CP_UTF8, 0, pLine.data(), static_cast<int>(pLine.size()), nullptr, 0, nullptr, nullptr);
            if (size > 0)
            {
                utf8Line.resize(size);
                WideCharToMultiByte(CP_UTF8, 0, pLine.data(), static_cast<int>(pLine.size()), utf8Line.data(), size, nullptr, nullptr);

                DWORD written = 0;
                WriteFile(_FileHandle, utf8Line.c_str(), static_cast<DWORD>(utf8Line.size()), &written, nullptr);
            }
        }
    }

    void XLogger::WriteToDebugOutput(std::wstring_view pLine)
    {
        OutputDebugStringW(pLine.data());
    }

    void XLogger::WriteToEventLog(XLogLevel pLevel, std::wstring_view pMessage)
    {
        XEventType eventType = XEventType::Information;
        XEventID eventID = XEventID::GenericInfo;

        switch (pLevel)
        {
        case XLogLevel::Warning:
            eventType = XEventType::Warning;
            eventID = XEventID::GenericWarning;
            break;
        case XLogLevel::Error:
        case XLogLevel::Critical:
            eventType = XEventType::Error;
            eventID = XEventID::GenericError;
            break;
        default:
            eventType = XEventType::Information;
            eventID = XEventID::GenericInfo;
            break;
        }

        if (_EventLog == nullptr)
        {
            _EventLog = std::make_unique<XEventLog>(_AppName);
            (void)_EventLog->Open();
        }

        if (_EventLog != nullptr && _EventLog->IsOpen())
            (void)_EventLog->ReportEvent(eventType, eventID, 0, pMessage);
    }

    void XLogger::LogSeparator()
    {
        Log(XLogLevel::Info, L"========================================");
    }

    void XLogger::Flush()
    {
        std::lock_guard lock(_Mutex);
        if (_FileHandle != INVALID_HANDLE_VALUE)
            FlushFileBuffers(_FileHandle);
    }

    std::wstring_view XLogger::LevelToString(XLogLevel pLevel) noexcept
    {
        switch (pLevel)
        {
        case XLogLevel::Trace:    return L"TRACE";
        case XLogLevel::Debug:    return L"DEBUG";
        case XLogLevel::Info:     return L"INFO ";
        case XLogLevel::Warning:  return L"WARN ";
        case XLogLevel::Error:    return L"ERROR";
        case XLogLevel::Critical: return L"CRIT ";
        default:                  return L"?????";
        }
    }

    std::wstring XLogger::GetTimestamp()
    {
        SYSTEMTIME st{};
        GetLocalTime(&st);
        return std::format(L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    }

    std::wstring XLogger::GetDateString()
    {
        SYSTEMTIME st{};
        GetLocalTime(&st);
        return std::format(L"{:04}-{:02}-{:02}", st.wYear, st.wMonth, st.wDay);
    }

} // namespace Tootega
