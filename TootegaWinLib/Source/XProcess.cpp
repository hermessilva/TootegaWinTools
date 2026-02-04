// ============================================================================
// XProcess.cpp - Process launching utilities for Windows Services
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XProcess.h"
#include "XTypes.h"
#include "XLogger.h"

#include <wtsapi32.h>
#include <userenv.h>

namespace Tootega
{
    // ========================================================================
    // Public Methods
    // ========================================================================

    XResult<DWORD> XProcessLauncher::LaunchAgentInActiveSession(LPCWSTR pProcessPath) noexcept
    {
        return LaunchAgentInActiveSession(pProcessPath, nullptr);
    }

    XResult<DWORD> XProcessLauncher::LaunchAgentInActiveSession(
        LPCWSTR pProcessPath,
        LPCWSTR pCommandLine) noexcept
    {
        if (!pProcessPath || *pProcessPath == L'\0')
            return XError::Application(ERROR_INVALID_PARAMETER, L"Process path cannot be null or empty");

        auto sessionResult = GetActiveConsoleSessionId();
        if (!sessionResult)
            return sessionResult.Error();

        return LaunchAgentInSession(sessionResult.Value(), pProcessPath, pCommandLine);
    }

    XResult<DWORD> XProcessLauncher::LaunchAgentInSession(
        DWORD pSessionId,
        LPCWSTR pProcessPath,
        LPCWSTR pCommandLine) noexcept
    {
        if (!pProcessPath || *pProcessPath == L'\0')
            return XError::Application(ERROR_INVALID_PARAMETER, L"Process path cannot be null or empty");

        XLogger::Instance().Debug(
            L"XProcessLauncher: Launching process in session {}: {}",
            pSessionId, pProcessPath);

        HANDLE userToken = nullptr;
        if (!WTSQueryUserToken(pSessionId, &userToken))
        {
            auto err = GetLastError();
            XLogger::Instance().Error(
                L"XProcessLauncher: WTSQueryUserToken failed for session {}: 0x{:08X}",
                pSessionId, err);
            return XError::FromWin32(err, L"WTSQueryUserToken failed");
        }

        auto tokenGuard = MakeUniqueHandle(userToken);

        HANDLE duplicatedToken = nullptr;
        if (!DuplicateTokenEx(
            userToken,
            TOKEN_ALL_ACCESS,
            nullptr,
            SecurityIdentification,
            TokenPrimary,
            &duplicatedToken))
        {
            auto err = GetLastError();
            XLogger::Instance().Error(
                L"XProcessLauncher: DuplicateTokenEx failed: 0x{:08X}", err);
            return XError::FromWin32(err, L"DuplicateTokenEx failed");
        }

        auto dupTokenGuard = MakeUniqueHandle(duplicatedToken);

        return LaunchProcessAsUser(duplicatedToken, pProcessPath, pCommandLine);
    }

    XResult<DWORD> XProcessLauncher::GetActiveConsoleSessionId() noexcept
    {
        DWORD sessionId = WTSGetActiveConsoleSessionId();
        if (sessionId == 0xFFFFFFFF)
        {
            XLogger::Instance().Error(
                L"XProcessLauncher: No active console session found");
            return XError::Application(ERROR_NO_SUCH_LOGON_SESSION, L"No active console session");
        }

        XLogger::Instance().Debug(
            L"XProcessLauncher: Active console session ID: {}", sessionId);
        return sessionId;
    }

    // ========================================================================
    // Private Methods
    // ========================================================================

    XResult<DWORD> XProcessLauncher::LaunchProcessAsUser(
        HANDLE pUserToken,
        LPCWSTR pProcessPath,
        LPCWSTR pCommandLine) noexcept
    {
        LPVOID environment = nullptr;
        if (!CreateEnvironmentBlock(&environment, pUserToken, FALSE))
        {
            auto err = GetLastError();
            XLogger::Instance().Warning(
                L"XProcessLauncher: CreateEnvironmentBlock failed: 0x{:08X} (continuing without user environment)",
                err);
        }

        struct EnvironmentGuard
        {
            LPVOID Env;
            ~EnvironmentGuard() { if (Env) DestroyEnvironmentBlock(Env); }
        } envGuard{ environment };

        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        si.lpDesktop = const_cast<LPWSTR>(L"winsta0\\default");
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOW;

        PROCESS_INFORMATION pi = {};

        std::wstring cmdLine;
        if (pCommandLine && *pCommandLine != L'\0')
        {
            cmdLine = pCommandLine;
        }
        else
        {
            cmdLine = L"\"";
            cmdLine += pProcessPath;
            cmdLine += L"\"";
        }

        DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE;
        if (environment)
            creationFlags |= CREATE_UNICODE_ENVIRONMENT;

        BOOL success = CreateProcessAsUserW(
            pUserToken,
            pProcessPath,
            cmdLine.data(),
            nullptr,
            nullptr,
            FALSE,
            creationFlags,
            environment,
            nullptr,
            &si,
            &pi);

        if (!success)
        {
            auto err = GetLastError();
            XLogger::Instance().Error(
                L"XProcessLauncher: CreateProcessAsUserW failed for '{}': 0x{:08X}",
                pProcessPath, err);
            return XError::FromWin32(err, L"CreateProcessAsUserW failed");
        }

        auto processGuard = MakeUniqueHandle(pi.hProcess);
        auto threadGuard = MakeUniqueHandle(pi.hThread);

        XLogger::Instance().Info(
            L"XProcessLauncher: Successfully launched process '{}' with PID {}",
            pProcessPath, pi.dwProcessId);

        return pi.dwProcessId;
    }

} // namespace Tootega

