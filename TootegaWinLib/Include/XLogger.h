#pragma once
// ============================================================================
// XLogger.h - Thread-safe logging system
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XPlatform.h"
#include "XTypes.h"

namespace Tootega
{
    // Forward declaration
    class XEventLog;

    // ========================================================================
    // Log Level
    // ========================================================================

    enum class XLogLevel
    {
        Trace,
        Debug,
        Info,
        Warning,
        Error,
        Critical,
        None
    };

    // ========================================================================
    // Log Output Target
    // ========================================================================

    enum class XLogTarget : DWORD
    {
        None        = 0x00,
        Console     = 0x01,
        File        = 0x02,
        DebugOutput = 0x04,
        EventLog    = 0x08,
        All         = Console | File | DebugOutput
    };

    inline XLogTarget operator|(XLogTarget pA, XLogTarget pB) noexcept
    {
        return static_cast<XLogTarget>(static_cast<DWORD>(pA) | static_cast<DWORD>(pB));
    }

    inline XLogTarget operator&(XLogTarget pA, XLogTarget pB) noexcept
    {
        return static_cast<XLogTarget>(static_cast<DWORD>(pA) & static_cast<DWORD>(pB));
    }

    inline bool HasFlag(XLogTarget pTarget, XLogTarget pFlag) noexcept
    {
        return (pTarget & pFlag) == pFlag;
    }

    // ========================================================================
    // XLogger - Thread-safe singleton logger
    // ========================================================================

    class XLogger final
    {
    public:
        static XLogger& Instance() noexcept;

        TOOTEGA_DISABLE_COPY_MOVE(XLogger);

        void Initialize(std::wstring_view pAppName, std::wstring_view pLogDirectory = {});
        void Shutdown() noexcept;

        void SetMinLevel(XLogLevel pLevel) noexcept { _MinLevel.store(pLevel, std::memory_order_relaxed); }
        [[nodiscard]] XLogLevel GetMinLevel() const noexcept { return _MinLevel.load(std::memory_order_relaxed); }

        void SetTargets(XLogTarget pTargets) noexcept { _Targets.store(pTargets, std::memory_order_relaxed); }
        [[nodiscard]] XLogTarget GetTargets() const noexcept { return _Targets.load(std::memory_order_relaxed); }

        void SetServiceMode(bool pServiceMode) noexcept { _ServiceMode.store(pServiceMode, std::memory_order_relaxed); }
        [[nodiscard]] bool IsServiceMode() const noexcept { return _ServiceMode.load(std::memory_order_relaxed); }

        template<typename... TArgs>
        void Log(XLogLevel pLevel, std::wstring_view pFormat, TArgs&&... pArgs)
        {
            if (pLevel < _MinLevel.load(std::memory_order_relaxed))
                return;

            std::wstring message;
            if constexpr (sizeof...(pArgs) > 0)
                message = std::vformat(pFormat, std::make_wformat_args(pArgs...));
            else
                message = std::wstring(pFormat);

            WriteLog(pLevel, message);
        }

        template<typename... TArgs>
        void Trace(std::wstring_view pFormat, TArgs&&... pArgs)
        {
            Log(XLogLevel::Trace, pFormat, std::forward<TArgs>(pArgs)...);
        }

        template<typename... TArgs>
        void Debug(std::wstring_view pFormat, TArgs&&... pArgs)
        {
            Log(XLogLevel::Debug, pFormat, std::forward<TArgs>(pArgs)...);
        }

        template<typename... TArgs>
        void Info(std::wstring_view pFormat, TArgs&&... pArgs)
        {
            Log(XLogLevel::Info, pFormat, std::forward<TArgs>(pArgs)...);
        }

        template<typename... TArgs>
        void Warning(std::wstring_view pFormat, TArgs&&... pArgs)
        {
            Log(XLogLevel::Warning, pFormat, std::forward<TArgs>(pArgs)...);
        }

        template<typename... TArgs>
        void Error(std::wstring_view pFormat, TArgs&&... pArgs)
        {
            Log(XLogLevel::Error, pFormat, std::forward<TArgs>(pArgs)...);
        }

        template<typename... TArgs>
        void Critical(std::wstring_view pFormat, TArgs&&... pArgs)
        {
            Log(XLogLevel::Critical, pFormat, std::forward<TArgs>(pArgs)...);
        }

        void LogSeparator();
        void Flush();

    private:
        XLogger();
        ~XLogger();

        void EnsureLogDirectory();
        void WriteLog(XLogLevel pLevel, std::wstring_view pMessage);
        void WriteToConsole(XLogLevel pLevel, std::wstring_view pTimestamp, std::wstring_view pMessage);
        void WriteToFile(std::wstring_view pLine);
        void WriteToDebugOutput(std::wstring_view pLine);
        void WriteToEventLog(XLogLevel pLevel, std::wstring_view pMessage);

        [[nodiscard]] static std::wstring_view LevelToString(XLogLevel pLevel) noexcept;
        [[nodiscard]] static std::wstring GetTimestamp();
        [[nodiscard]] static std::wstring GetDateString();

        std::mutex                      _Mutex;
        std::wstring                    _AppName;
        std::wstring                    _LogDirectory;
        std::wstring                    _CurrentLogFile;
        std::wstring                    _CurrentDate;
        HANDLE                          _FileHandle{INVALID_HANDLE_VALUE};
        std::unique_ptr<XEventLog>      _EventLog;
        std::atomic<XLogLevel>          _MinLevel{XLogLevel::Info};
        std::atomic<XLogTarget>         _Targets{XLogTarget::DebugOutput};
        std::atomic<bool>               _ServiceMode{false};
        std::atomic<bool>               _Initialized{false};
    };

    // ========================================================================
    // Convenience Macros
    // ========================================================================

#define XLOG_TRACE(fmt, ...)    Tootega::XLogger::Instance().Trace(fmt, ##__VA_ARGS__)
#define XLOG_DEBUG(fmt, ...)    Tootega::XLogger::Instance().Debug(fmt, ##__VA_ARGS__)
#define XLOG_INFO(fmt, ...)     Tootega::XLogger::Instance().Info(fmt, ##__VA_ARGS__)
#define XLOG_WARNING(fmt, ...)  Tootega::XLogger::Instance().Warning(fmt, ##__VA_ARGS__)
#define XLOG_ERROR(fmt, ...)    Tootega::XLogger::Instance().Error(fmt, ##__VA_ARGS__)
#define XLOG_CRITICAL(fmt, ...) Tootega::XLogger::Instance().Critical(fmt, ##__VA_ARGS__)
#define XLOG_SEPARATOR()        Tootega::XLogger::Instance().LogSeparator()

} // namespace Tootega
