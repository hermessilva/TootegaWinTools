/*
** SevenZipView Setup - Standalone Installer/Uninstaller
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
**
** This is a simple, self-contained installer that:
** - Detects if SevenZipView is already installed
** - Offers to Install (if not installed) or Uninstall (if installed)
** - Extracts embedded DLL to Program Files\Tootega\SevenZipView
** - Registers/Unregisters the COM DLL using regsvr32
** - Requests UAC elevation automatically
** - Restarts Explorer to apply changes
**
** Build:
**   cl /EHsc /O2 /MT /Fe:SevenZipViewSetup.exe Setup.cpp 
**      shell32.lib ole32.lib user32.lib advapi32.lib shlwapi.lib
*/

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <tlhelp32.h>

#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")

// Product information
#define PRODUCT_NAME        L"SevenZipView"
#define PRODUCT_VERSION     L"1.0.0"
#define PRODUCT_PUBLISHER   L"Tootega Pesquisa e Inovacao"
#define DLL_NAME            L"SevenZipView.dll"
#define INSTALL_SUBDIR      L"Tootega\\SevenZipView"

// Registry paths for uninstall information
#define UNINSTALL_KEY       L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SevenZipView"

// Resource ID for embedded DLL
#define IDR_EMBEDDED_DLL    101
#define RT_DLL_RESOURCE     L"DLL"

// Forward declarations
bool IsElevated();
bool ElevateAndRestart(int argc, wchar_t* argv[]);
bool IsInstalled();
std::wstring GetInstallPath();
bool StopExplorer();
bool StartExplorer();
bool StopShellHosts();
bool RegisterDll(const std::wstring& dllPath);
bool UnregisterDll(const std::wstring& dllPath);
bool ExtractEmbeddedDll(const std::wstring& installDir);
bool CreateUninstallEntry(const std::wstring& installDir, const std::wstring& setupExePath);
bool RemoveUninstallEntry();
bool RemoveInstallDir(const std::wstring& installDir);
void NotifyShellChange();
int ShowInstallDialog();
int ShowUninstallDialog();
void ShowSuccessMessage(bool isInstall);
void ShowErrorMessage(const std::wstring& message);

// ============================================================================
// Main entry point
// ============================================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);

    // Initialize COM
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // Parse command line
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    bool silentMode = false;
    bool forceInstall = false;
    bool forceUninstall = false;

    for (int i = 1; i < argc; i++) {
        if (_wcsicmp(argv[i], L"/S") == 0 || _wcsicmp(argv[i], L"/silent") == 0) {
            silentMode = true;
        }
        else if (_wcsicmp(argv[i], L"/I") == 0 || _wcsicmp(argv[i], L"/install") == 0) {
            forceInstall = true;
        }
        else if (_wcsicmp(argv[i], L"/U") == 0 || _wcsicmp(argv[i], L"/uninstall") == 0) {
            forceUninstall = true;
        }
        else if (_wcsicmp(argv[i], L"/?") == 0 || _wcsicmp(argv[i], L"/help") == 0) {
            MessageBoxW(nullptr,
                L"SevenZipView Setup\n\n"
                L"Usage: SevenZipViewSetup.exe [options]\n\n"
                L"Options:\n"
                L"  /S, /silent     Silent installation\n"
                L"  /I, /install    Force install mode\n"
                L"  /U, /uninstall  Force uninstall mode\n"
                L"  /?, /help       Show this help\n\n"
                L"Without options, the installer detects if SevenZipView\n"
                L"is installed and offers the appropriate action.",
                L"SevenZipView Setup - Help",
                MB_ICONINFORMATION | MB_OK);
            LocalFree(argv);
            CoUninitialize();
            return 0;
        }
    }

    // Check for admin rights
    if (!IsElevated()) {
        // Request elevation
        if (!silentMode) {
            if (ElevateAndRestart(argc, argv)) {
                LocalFree(argv);
                CoUninitialize();
                return 0; // New elevated process started
            }
        }
        if (!silentMode) {
            ShowErrorMessage(L"Administrator privileges are required.\nPlease run as Administrator.");
        }
        LocalFree(argv);
        CoUninitialize();
        return 1;
    }

    LocalFree(argv);

    // Determine action
    bool isInstalled = IsInstalled();
    bool doInstall = false;
    bool doUninstall = false;

    if (forceInstall) {
        doInstall = true;
    }
    else if (forceUninstall) {
        doUninstall = true;
    }
    else if (silentMode) {
        // Silent mode: install if not installed, uninstall if installed
        doInstall = !isInstalled;
        doUninstall = isInstalled;
    }
    else {
        // Interactive mode: show dialog
        if (isInstalled) {
            int result = ShowUninstallDialog();
            if (result == IDYES) {
                doUninstall = true;
            }
        }
        else {
            int result = ShowInstallDialog();
            if (result == IDYES) {
                doInstall = true;
            }
        }
    }

    // Perform action
    bool success = false;

    if (doInstall) {
        // Get install path
        std::wstring installDir = GetInstallPath();
        std::wstring targetDll = installDir + L"\\" + DLL_NAME;

        // Stop processes
        StopShellHosts();
        StopExplorer();
        Sleep(1000);

        // Unregister old if exists
        if (PathFileExistsW(targetDll.c_str())) {
            UnregisterDll(targetDll);
            Sleep(500);
        }

        // Extract embedded DLL
        if (!ExtractEmbeddedDll(installDir)) {
            if (!silentMode) {
                ShowErrorMessage(L"Failed to extract DLL to installation directory.");
            }
            StartExplorer();
            CoUninitialize();
            return 1;
        }

        // Register DLL
        if (!RegisterDll(targetDll)) {
            if (!silentMode) {
                ShowErrorMessage(L"Failed to register the DLL.\nThe files were copied but registration failed.");
            }
            StartExplorer();
            CoUninitialize();
            return 1;
        }

        // Get setup exe path for uninstall
        wchar_t setupExePath[MAX_PATH];
        GetModuleFileNameW(nullptr, setupExePath, MAX_PATH);

        // Copy setup to install dir for uninstall capability
        std::wstring setupTarget = installDir + L"\\SevenZipViewSetup.exe";
        CopyFileW(setupExePath, setupTarget.c_str(), FALSE);

        // Create uninstall entry
        CreateUninstallEntry(installDir, setupTarget);

        // Notify shell and restart explorer
        NotifyShellChange();
        StartExplorer();

        success = true;
        if (!silentMode) {
            ShowSuccessMessage(true);
        }
    }
    else if (doUninstall) {
        std::wstring installDir = GetInstallPath();
        std::wstring targetDll = installDir + L"\\" + DLL_NAME;

        // Stop processes
        StopShellHosts();
        StopExplorer();
        Sleep(1000);

        // Unregister DLL
        if (PathFileExistsW(targetDll.c_str())) {
            UnregisterDll(targetDll);
            Sleep(500);
        }

        // Remove uninstall entry
        RemoveUninstallEntry();

        // Remove files and directory
        RemoveInstallDir(installDir);

        // Notify shell and restart explorer
        NotifyShellChange();
        StartExplorer();

        success = true;
        if (!silentMode) {
            ShowSuccessMessage(false);
        }
    }

    CoUninitialize();
    return success ? 0 : 1;
}

// ============================================================================
// Helper functions
// ============================================================================

bool IsElevated()
{
    BOOL elevated = FALSE;
    HANDLE hToken = nullptr;
    
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(elevation);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size)) {
            elevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    
    return elevated != FALSE;
}

bool ElevateAndRestart(int argc, wchar_t* argv[])
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // Build command line
    std::wstring cmdLine;
    for (int i = 1; i < argc; i++) {
        if (!cmdLine.empty()) cmdLine += L" ";
        cmdLine += argv[i];
    }

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.lpParameters = cmdLine.c_str();
    sei.nShow = SW_SHOWNORMAL;

    return ShellExecuteExW(&sei) != FALSE;
}

bool IsInstalled()
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }

    // Also check if DLL exists
    std::wstring installDir = GetInstallPath();
    std::wstring dllPath = installDir + L"\\" + DLL_NAME;
    return PathFileExistsW(dllPath.c_str()) != FALSE;
}

std::wstring GetInstallPath()
{
    wchar_t programFiles[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, programFiles))) {
        std::wstring path = programFiles;
        path += L"\\";
        path += INSTALL_SUBDIR;
        return path;
    }
    return L"C:\\Program Files\\" INSTALL_SUBDIR;
}

bool StopExplorer()
{
    // Find and terminate explorer.exe
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"explorer.exe") == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return true;
}

bool StartExplorer()
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    wchar_t cmd[] = L"explorer.exe";
    if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }
    return false;
}

bool StopShellHosts()
{
    const wchar_t* processes[] = { L"dllhost.exe", L"prevhost.exe", L"SearchProtocolHost.exe" };
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            for (const auto& proc : processes) {
                if (_wcsicmp(pe.szExeFile, proc) == 0) {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProcess) {
                        TerminateProcess(hProcess, 0);
                        CloseHandle(hProcess);
                    }
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return true;
}

bool RegisterDll(const std::wstring& dllPath)
{
    wchar_t systemDir[MAX_PATH];
    GetSystemDirectoryW(systemDir, MAX_PATH);

    std::wstring regsvr32 = std::wstring(systemDir) + L"\\regsvr32.exe";
    std::wstring args = L"/s \"" + dllPath + L"\"";

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"open";
    sei.lpFile = regsvr32.c_str();
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 30000);
            DWORD exitCode = 0;
            GetExitCodeProcess(sei.hProcess, &exitCode);
            CloseHandle(sei.hProcess);
            return exitCode == 0;
        }
        return true;
    }
    return false;
}

bool UnregisterDll(const std::wstring& dllPath)
{
    wchar_t systemDir[MAX_PATH];
    GetSystemDirectoryW(systemDir, MAX_PATH);

    std::wstring regsvr32 = std::wstring(systemDir) + L"\\regsvr32.exe";
    std::wstring args = L"/u /s \"" + dllPath + L"\"";

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"open";
    sei.lpFile = regsvr32.c_str();
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 30000);
            CloseHandle(sei.hProcess);
        }
        return true;
    }
    return false;
}

bool ExtractEmbeddedDll(const std::wstring& installDir)
{
    // Create directory
    if (SHCreateDirectoryExW(nullptr, installDir.c_str(), nullptr) != ERROR_SUCCESS) {
        DWORD err = GetLastError();
        if (err != ERROR_SUCCESS && err != ERROR_ALREADY_EXISTS && err != ERROR_FILE_EXISTS) {
            return false;
        }
    }

    // Find the embedded DLL resource
    HMODULE hModule = GetModuleHandleW(nullptr);
    HRSRC hResource = FindResourceW(hModule, MAKEINTRESOURCEW(IDR_EMBEDDED_DLL), RT_DLL_RESOURCE);
    if (!hResource) {
        return false;
    }

    // Load the resource
    HGLOBAL hGlobal = LoadResource(hModule, hResource);
    if (!hGlobal) {
        return false;
    }

    // Get pointer to resource data
    void* pData = LockResource(hGlobal);
    if (!pData) {
        return false;
    }

    // Get resource size
    DWORD dwSize = SizeofResource(hModule, hResource);
    if (dwSize == 0) {
        return false;
    }

    // Write to target file
    std::wstring targetDll = installDir + L"\\" + DLL_NAME;
    HANDLE hFile = CreateFileW(targetDll.c_str(), GENERIC_WRITE, 0, nullptr, 
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD dwWritten = 0;
    BOOL bSuccess = WriteFile(hFile, pData, dwSize, &dwWritten, nullptr);
    CloseHandle(hFile);

    return bSuccess && (dwWritten == dwSize);
}

bool CreateUninstallEntry(const std::wstring& installDir, const std::wstring& setupExePath)
{
    HKEY hKey;
    DWORD disposition;

    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, &disposition) != ERROR_SUCCESS) {
        return false;
    }

    // Set values
    RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, 
        (BYTE*)PRODUCT_NAME, (DWORD)(wcslen(PRODUCT_NAME) + 1) * sizeof(wchar_t));

    RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ,
        (BYTE*)PRODUCT_VERSION, (DWORD)(wcslen(PRODUCT_VERSION) + 1) * sizeof(wchar_t));

    RegSetValueExW(hKey, L"Publisher", 0, REG_SZ,
        (BYTE*)PRODUCT_PUBLISHER, (DWORD)(wcslen(PRODUCT_PUBLISHER) + 1) * sizeof(wchar_t));

    RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ,
        (BYTE*)installDir.c_str(), (DWORD)(installDir.length() + 1) * sizeof(wchar_t));

    std::wstring uninstallCmd = L"\"" + setupExePath + L"\" /U";
    RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ,
        (BYTE*)uninstallCmd.c_str(), (DWORD)(uninstallCmd.length() + 1) * sizeof(wchar_t));

    std::wstring quietUninstall = L"\"" + setupExePath + L"\" /U /S";
    RegSetValueExW(hKey, L"QuietUninstallString", 0, REG_SZ,
        (BYTE*)quietUninstall.c_str(), (DWORD)(quietUninstall.length() + 1) * sizeof(wchar_t));

    DWORD noModify = 1;
    RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (BYTE*)&noModify, sizeof(noModify));
    RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (BYTE*)&noModify, sizeof(noModify));

    // Icon
    std::wstring icon = installDir + L"\\" + DLL_NAME + L",0";
    RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ,
        (BYTE*)icon.c_str(), (DWORD)(icon.length() + 1) * sizeof(wchar_t));

    // Estimated size (in KB)
    DWORD estimatedSize = 500; // ~500 KB
    RegSetValueExW(hKey, L"EstimatedSize", 0, REG_DWORD, (BYTE*)&estimatedSize, sizeof(estimatedSize));

    RegCloseKey(hKey);
    return true;
}

bool RemoveUninstallEntry()
{
    return RegDeleteKeyExW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY, KEY_WOW64_64KEY, 0) == ERROR_SUCCESS;
}

bool RemoveInstallDir(const std::wstring& installDir)
{
    // Delete files first
    std::wstring dllPath = installDir + L"\\" + DLL_NAME;
    DeleteFileW(dllPath.c_str());
    
    std::wstring pdbPath = installDir + L"\\SevenZipView.pdb";
    DeleteFileW(pdbPath.c_str());
    
    std::wstring setupPath = installDir + L"\\SevenZipViewSetup.exe";
    // Mark setup for deletion on reboot if in use
    if (!DeleteFileW(setupPath.c_str())) {
        MoveFileExW(setupPath.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
    
    // Remove directory
    RemoveDirectoryW(installDir.c_str());
    
    // Also try to remove parent Tootega folder if empty
    wchar_t programFiles[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, programFiles))) {
        std::wstring tootegaDir = std::wstring(programFiles) + L"\\Tootega";
        RemoveDirectoryW(tootegaDir.c_str()); // Will fail if not empty, which is OK
    }
    
    return true;
}

void NotifyShellChange()
{
    // Notify shell of changes
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

int ShowInstallDialog()
{
    return MessageBoxW(nullptr,
        L"SevenZipView is not installed.\n\n"
        L"Do you want to install SevenZipView?\n\n"
        L"This will add support for viewing 7-Zip archives\n"
        L"directly in Windows Explorer.",
        L"SevenZipView Setup",
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1);
}

int ShowUninstallDialog()
{
    return MessageBoxW(nullptr,
        L"SevenZipView is already installed.\n\n"
        L"Do you want to uninstall SevenZipView?\n\n"
        L"This will remove the shell extension and\n"
        L"all associated files.",
        L"SevenZipView Setup",
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
}

void ShowSuccessMessage(bool isInstall)
{
    if (isInstall) {
        MessageBoxW(nullptr,
            L"SevenZipView has been installed successfully!\n\n"
            L"You can now browse 7-Zip archives directly in\n"
            L"Windows Explorer.\n\n"
            L"Right-click on a .7z file to see additional options.",
            L"SevenZipView Setup",
            MB_OK | MB_ICONINFORMATION);
    }
    else {
        MessageBoxW(nullptr,
            L"SevenZipView has been uninstalled successfully.\n\n"
            L"Thank you for using SevenZipView!",
            L"SevenZipView Setup",
            MB_OK | MB_ICONINFORMATION);
    }
}

void ShowErrorMessage(const std::wstring& message)
{
    MessageBoxW(nullptr, message.c_str(), L"SevenZipView Setup - Error", MB_OK | MB_ICONERROR);
}
