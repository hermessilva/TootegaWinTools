#pragma once
// ============================================================================
// XProcess.h - Process launching utilities for Windows Services
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XPlatform.h"
#include "XResult.h"

#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Userenv.lib")

namespace Tootega
{
    // ========================================================================
    // XProcessLauncher - Launch processes in user sessions from SYSTEM context
    // ========================================================================

    class XProcessLauncher final
    {
    public:
        TOOTEGA_DISABLE_COPY_MOVE(XProcessLauncher);

        XProcessLauncher() = default;
        ~XProcessLauncher() = default;

        [[nodiscard]] static XResult<DWORD> LaunchAgentInActiveSession(LPCWSTR pProcessPath) noexcept;

        [[nodiscard]] static XResult<DWORD> LaunchAgentInActiveSession(
            LPCWSTR pProcessPath,
            LPCWSTR pCommandLine) noexcept;

        [[nodiscard]] static XResult<DWORD> LaunchAgentInSession(
            DWORD pSessionId,
            LPCWSTR pProcessPath,
            LPCWSTR pCommandLine = nullptr) noexcept;

        [[nodiscard]] static XResult<DWORD> GetActiveConsoleSessionId() noexcept;

    private:
        [[nodiscard]] static XResult<DWORD> LaunchProcessAsUser(
            HANDLE pUserToken,
            LPCWSTR pProcessPath,
            LPCWSTR pCommandLine) noexcept;
    };

} // namespace Tootega

