//=============================================================================
// XElevation.h - Windows UAC Elevation Utilities
//=============================================================================
// Provides utilities for checking and requesting administrator privileges.
// These utilities complement the manifest-based elevation by allowing
// runtime checks and programmatic elevation when needed.
//=============================================================================
#pragma once

#ifndef XELEVATION_H
#define XELEVATION_H

#include <Windows.h>
#include <shellapi.h>
#include <sddl.h>

namespace Tootega {

//-----------------------------------------------------------------------------
// XElevation - Static utility class for elevation operations
//-----------------------------------------------------------------------------
class XElevation final {
public:
    XElevation() = delete;
    ~XElevation() = delete;
    XElevation(const XElevation&) = delete;
    XElevation& operator=(const XElevation&) = delete;

    //-------------------------------------------------------------------------
    // IsRunningAsAdmin - Check if current process has administrator privileges
    //-------------------------------------------------------------------------
    // Returns: true if running with admin privileges, false otherwise
    //-------------------------------------------------------------------------
    [[nodiscard]] static bool IsRunningAsAdmin() noexcept {
        BOOL isAdmin = FALSE;
        PSID adminGroup = nullptr;
        
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        
        if (!AllocateAndInitializeSid(
                &ntAuthority,
                2,
                SECURITY_BUILTIN_DOMAIN_RID,
                DOMAIN_ALIAS_RID_ADMINS,
                0, 0, 0, 0, 0, 0,
                &adminGroup))
            return false;
        
        if (!CheckTokenMembership(nullptr, adminGroup, &isAdmin))
            isAdmin = FALSE;
        
        if (adminGroup)
            FreeSid(adminGroup);
        
        return isAdmin != FALSE;
    }

    //-------------------------------------------------------------------------
    // IsElevated - Check if current process token is elevated
    //-------------------------------------------------------------------------
    // Returns: true if process is elevated, false otherwise
    // Note: This is different from IsRunningAsAdmin - a process can be
    //       elevated but not necessarily an administrator
    //-------------------------------------------------------------------------
    [[nodiscard]] static bool IsElevated() noexcept {
        BOOL isElevated = FALSE;
        HANDLE tokenHandle = nullptr;
        
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle))
            return false;
        
        TOKEN_ELEVATION elevation{};
        DWORD returnLength = 0;
        
        if (GetTokenInformation(
                tokenHandle,
                TokenElevation,
                &elevation,
                sizeof(elevation),
                &returnLength)) {
            isElevated = elevation.TokenIsElevated;
        }
        
        if (tokenHandle)
            CloseHandle(tokenHandle);
        
        return isElevated != FALSE;
    }

    //-------------------------------------------------------------------------
    // GetElevationType - Get the elevation type of the current process
    //-------------------------------------------------------------------------
    // Returns: TOKEN_ELEVATION_TYPE enum value
    //          TokenElevationTypeDefault (1) - UAC disabled or standard user
    //          TokenElevationTypeFull (2)    - Elevated administrator
    //          TokenElevationTypeLimited (3) - Non-elevated administrator
    //-------------------------------------------------------------------------
    [[nodiscard]] static TOKEN_ELEVATION_TYPE GetElevationType() noexcept {
        TOKEN_ELEVATION_TYPE elevationType = TokenElevationTypeDefault;
        HANDLE tokenHandle = nullptr;
        
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle))
            return TokenElevationTypeDefault;
        
        DWORD returnLength = 0;
        GetTokenInformation(
            tokenHandle,
            TokenElevationType,
            &elevationType,
            sizeof(elevationType),
            &returnLength);
        
        if (tokenHandle)
            CloseHandle(tokenHandle);
        
        return elevationType;
    }

    //-------------------------------------------------------------------------
    // RequestElevation - Restart the application with elevated privileges
    //-------------------------------------------------------------------------
    // Parameters:
    //   pArguments  - Command line arguments to pass to elevated process
    //   pShowWindow - Window show state (default: SW_SHOWNORMAL)
    // Returns: true if elevation was requested successfully, false otherwise
    // Note: If successful, this function does not return as the process exits
    //-------------------------------------------------------------------------
    [[nodiscard]] static bool RequestElevation(
            LPCWSTR pArguments = nullptr,
            int pShowWindow = SW_SHOWNORMAL) noexcept {
        
        if (IsRunningAsAdmin())
            return true;
        
        WCHAR executablePath[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, executablePath, MAX_PATH) == 0)
            return false;
        
        SHELLEXECUTEINFOW shellInfo{};
        shellInfo.cbSize = sizeof(shellInfo);
        shellInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        shellInfo.hwnd = nullptr;
        shellInfo.lpVerb = L"runas";
        shellInfo.lpFile = executablePath;
        shellInfo.lpParameters = pArguments;
        shellInfo.lpDirectory = nullptr;
        shellInfo.nShow = pShowWindow;
        
        if (!ShellExecuteExW(&shellInfo))
            return false;
        
        if (shellInfo.hProcess)
            CloseHandle(shellInfo.hProcess);
        
        ExitProcess(0);
    }

    //-------------------------------------------------------------------------
    // RequireAdministrator - Verify admin privileges or request elevation
    //-------------------------------------------------------------------------
    // Parameters:
    //   pArguments - Command line arguments to pass if elevation is needed
    // Returns: true if running as admin, does not return if elevation needed
    // Note: Call this at the beginning of main() to ensure elevation
    //-------------------------------------------------------------------------
    [[nodiscard]] static bool RequireAdministrator(
            LPCWSTR pArguments = nullptr) noexcept {
        
        if (IsRunningAsAdmin())
            return true;
        
        return RequestElevation(pArguments);
    }

    //-------------------------------------------------------------------------
    // GetIntegrityLevel - Get the integrity level of the current process
    //-------------------------------------------------------------------------
    // Returns: Integrity level RID or 0 on failure
    //          SECURITY_MANDATORY_LOW_RID      (0x1000) - Low integrity
    //          SECURITY_MANDATORY_MEDIUM_RID   (0x2000) - Medium integrity
    //          SECURITY_MANDATORY_HIGH_RID     (0x3000) - High integrity
    //          SECURITY_MANDATORY_SYSTEM_RID   (0x4000) - System integrity
    //-------------------------------------------------------------------------
    [[nodiscard]] static DWORD GetIntegrityLevel() noexcept {
        DWORD integrityLevel = 0;
        HANDLE tokenHandle = nullptr;
        
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle))
            return 0;
        
        DWORD labelSize = 0;
        GetTokenInformation(tokenHandle, TokenIntegrityLevel, nullptr, 0, &labelSize);
        
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && labelSize > 0) {
            auto* label = static_cast<TOKEN_MANDATORY_LABEL*>(
                LocalAlloc(LPTR, labelSize));
            
            if (label) {
                if (GetTokenInformation(
                        tokenHandle,
                        TokenIntegrityLevel,
                        label,
                        labelSize,
                        &labelSize)) {
                    integrityLevel = *GetSidSubAuthority(
                        label->Label.Sid,
                        static_cast<DWORD>(*GetSidSubAuthorityCount(label->Label.Sid) - 1));
                }
                LocalFree(label);
            }
        }
        
        if (tokenHandle)
            CloseHandle(tokenHandle);
        
        return integrityLevel;
    }

    //-------------------------------------------------------------------------
    // IsHighIntegrity - Check if running at high integrity level
    //-------------------------------------------------------------------------
    // Returns: true if running at high or system integrity level
    //-------------------------------------------------------------------------
    [[nodiscard]] static bool IsHighIntegrity() noexcept {
        return GetIntegrityLevel() >= SECURITY_MANDATORY_HIGH_RID;
    }

    //-------------------------------------------------------------------------
    // GetPrivilegeStatus - Check if a specific privilege is enabled
    //-------------------------------------------------------------------------
    // Parameters:
    //   pPrivilegeName - Name of the privilege (e.g., SE_DEBUG_NAME)
    // Returns: true if the privilege is enabled, false otherwise
    //-------------------------------------------------------------------------
    [[nodiscard]] static bool GetPrivilegeStatus(LPCWSTR pPrivilegeName) noexcept {
        if (!pPrivilegeName)
            return false;
        
        HANDLE tokenHandle = nullptr;
        if (!OpenProcessToken(
                GetCurrentProcess(),
                TOKEN_QUERY,
                &tokenHandle))
            return false;
        
        LUID privilegeLuid{};
        if (!LookupPrivilegeValueW(nullptr, pPrivilegeName, &privilegeLuid)) {
            CloseHandle(tokenHandle);
            return false;
        }
        
        PRIVILEGE_SET privilegeSet{};
        privilegeSet.PrivilegeCount = 1;
        privilegeSet.Control = PRIVILEGE_SET_ALL_NECESSARY;
        privilegeSet.Privilege[0].Luid = privilegeLuid;
        privilegeSet.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;
        
        BOOL hasPrivilege = FALSE;
        PrivilegeCheck(tokenHandle, &privilegeSet, &hasPrivilege);
        
        CloseHandle(tokenHandle);
        return hasPrivilege != FALSE;
    }

    //-------------------------------------------------------------------------
    // EnablePrivilege - Enable a specific privilege in the current process
    //-------------------------------------------------------------------------
    // Parameters:
    //   pPrivilegeName - Name of the privilege (e.g., SE_DEBUG_NAME)
    //   pEnable        - true to enable, false to disable
    // Returns: true if successful, false otherwise
    //-------------------------------------------------------------------------
    [[nodiscard]] static bool EnablePrivilege(
            LPCWSTR pPrivilegeName,
            bool pEnable = true) noexcept {
        
        if (!pPrivilegeName)
            return false;
        
        HANDLE tokenHandle = nullptr;
        if (!OpenProcessToken(
                GetCurrentProcess(),
                TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                &tokenHandle))
            return false;
        
        LUID privilegeLuid{};
        if (!LookupPrivilegeValueW(nullptr, pPrivilegeName, &privilegeLuid)) {
            CloseHandle(tokenHandle);
            return false;
        }
        
        TOKEN_PRIVILEGES tokenPrivileges{};
        tokenPrivileges.PrivilegeCount = 1;
        tokenPrivileges.Privileges[0].Luid = privilegeLuid;
        tokenPrivileges.Privileges[0].Attributes = pEnable ? SE_PRIVILEGE_ENABLED : 0;
        
        bool result = AdjustTokenPrivileges(
            tokenHandle,
            FALSE,
            &tokenPrivileges,
            0,
            nullptr,
            nullptr) != FALSE;
        
        if (result)
            result = GetLastError() == ERROR_SUCCESS;
        
        CloseHandle(tokenHandle);
        return result;
    }
};

} // namespace Tootega

#endif // XELEVATION_H
