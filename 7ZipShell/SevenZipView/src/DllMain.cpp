/*
** SevenZipView - Windows Explorer Shell Extension
** DLL Entry Point and Registration
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovação. All rights reserved.
** Licensed under MIT License
*/

#include "Common.h"
#include "ShellFolder.h"
#include "PreviewHandler.h"
#include "ContextMenu.h"
#include "PropertyHandler.h"
#include "IconHandler.h"
#include <cstdio>

// Define CLSIDs
// {7A8B9C0D-1E2F-3A4B-5C6D-7E8F9A0B1C2D}
const GUID CLSID_SevenZipViewFolder = 
    { 0x7a8b9c0d, 0x1e2f, 0x3a4b, { 0x5c, 0x6d, 0x7e, 0x8f, 0x9a, 0x0b, 0x1c, 0x2d } };

// {8B9C0D1E-2F3A-4B5C-6D7E-8F9A0B1C2D3E}
const GUID CLSID_SevenZipViewPreview =
    { 0x8b9c0d1e, 0x2f3a, 0x4b5c, { 0x6d, 0x7e, 0x8f, 0x9a, 0x0b, 0x1c, 0x2d, 0x3e } };

// {9C0D1E2F-3A4B-5C6D-7E8F-9A0B1C2D3E4F}
const GUID CLSID_SevenZipViewProperty =
    { 0x9c0d1e2f, 0x3a4b, 0x5c6d, { 0x7e, 0x8f, 0x9a, 0x0b, 0x1c, 0x2d, 0x3e, 0x4f } };

// {0D1E2F3A-4B5C-6D7E-8F9A-0B1C2D3E4F5A}
const GUID CLSID_SevenZipViewContextMenu =
    { 0x0d1e2f3a, 0x4b5c, 0x6d7e, { 0x8f, 0x9a, 0x0b, 0x1c, 0x2d, 0x3e, 0x4f, 0x5a } };

// {1E2F3A4B-5C6D-7E8F-9A0B-1C2D3E4F5A6B}
const GUID CLSID_SevenZipViewIcon =
    { 0x1e2f3a4b, 0x5c6d, 0x7e8f, { 0x9a, 0x0b, 0x1c, 0x2d, 0x3e, 0x4f, 0x5a, 0x6b } };

// Custom property keys
// {E8B4D6C8-AE3F-5B2F-9C7D-4E6F8A1B3D5E}
const PROPERTYKEY SevenZipView::PKEY_7z_FileCount = { { 0xe8b4d6c8, 0xae3f, 0x5b2f, { 0x9c, 0x7d, 0x4e, 0x6f, 0x8a, 0x1b, 0x3d, 0x5e } }, 1 };
const PROPERTYKEY SevenZipView::PKEY_7z_FolderCount = { { 0xe8b4d6c8, 0xae3f, 0x5b2f, { 0x9c, 0x7d, 0x4e, 0x6f, 0x8a, 0x1b, 0x3d, 0x5e } }, 2 };
const PROPERTYKEY SevenZipView::PKEY_7z_CompressionRatio = { { 0xe8b4d6c8, 0xae3f, 0x5b2f, { 0x9c, 0x7d, 0x4e, 0x6f, 0x8a, 0x1b, 0x3d, 0x5e } }, 3 };
const PROPERTYKEY SevenZipView::PKEY_7z_IsEncrypted = { { 0xe8b4d6c8, 0xae3f, 0x5b2f, { 0x9c, 0x7d, 0x4e, 0x6f, 0x8a, 0x1b, 0x3d, 0x5e } }, 4 };
const PROPERTYKEY SevenZipView::PKEY_7z_Method = { { 0xe8b4d6c8, 0xae3f, 0x5b2f, { 0x9c, 0x7d, 0x4e, 0x6f, 0x8a, 0x1b, 0x3d, 0x5e } }, 5 };

// Global variables
HMODULE g_hModule = nullptr;
LONG g_DllRefCount = 0;

// Supported file extensions
const wchar_t* g_SupportedExtensions[] = {
    L".7z",
};

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            g_hModule = hModule;
            DisableThreadLibraryCalls(hModule);
            SEVENZIPVIEW_LOG(L"DLL_PROCESS_ATTACH - SevenZipView.dll loaded");
            break;
            
        case DLL_PROCESS_DETACH:
            SEVENZIPVIEW_LOG(L"DLL_PROCESS_DETACH - SevenZipView.dll unloading");
            SevenZipView::ArchivePool::Instance().Clear();
            break;
    }
    return TRUE;
}

// DllCanUnloadNow
STDAPI DllCanUnloadNow() {
    return (g_DllRefCount == 0) ? S_OK : S_FALSE;
}

// DllGetClassObject
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    wchar_t clsidStr[64], riidStr[64];
    StringFromGUID2(rclsid, clsidStr, 64);
    StringFromGUID2(riid, riidStr, 64);
    SEVENZIPVIEW_LOG(L"DllGetClassObject CLSID=%s riid=%s", clsidStr, riidStr);

    // Check for supported CLSIDs
    if (!IsEqualCLSID(rclsid, CLSID_SevenZipViewFolder) &&
        !IsEqualCLSID(rclsid, CLSID_SevenZipViewPreview) &&
        !IsEqualCLSID(rclsid, CLSID_SevenZipViewContextMenu) &&
        !IsEqualCLSID(rclsid, CLSID_SevenZipViewProperty) &&
        !IsEqualCLSID(rclsid, CLSID_SevenZipViewIcon)) {
        SEVENZIPVIEW_LOG(L"  -> CLASS_E_CLASSNOTAVAILABLE");
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    SevenZipView::ClassFactory* factory = new (std::nothrow) SevenZipView::ClassFactory(rclsid);
    if (!factory) return E_OUTOFMEMORY;

    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();

    SEVENZIPVIEW_LOG(L"  -> DllGetClassObject returning 0x%08X", hr);
    return hr;
}

// ============================================================================
// Registry helper functions
// ============================================================================

struct REGISTRY_ENTRY {
    HKEY    hkeyRoot;
    LPCWSTR pszKeyName;
    LPCWSTR pszValueName;
    DWORD   dwType;
    LPCWSTR pszData;
    DWORD   dwData;
};

static HRESULT CreateRegKeyAndSetValue(const REGISTRY_ENTRY* pEntry, LPCWSTR pszModulePath) {
    HKEY hKey = nullptr;
    
    LONG result = RegCreateKeyExW(
        pEntry->hkeyRoot,
        pEntry->pszKeyName,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        nullptr,
        &hKey,
        nullptr);
    
    if (result != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(result);
    }
    
    if (pEntry->dwType == REG_SZ && pEntry->pszData) {
        wchar_t szData[MAX_PATH * 2];
        
        if (wcsstr(pEntry->pszData, L"%s") != nullptr) {
            StringCchPrintfW(szData, ARRAYSIZE(szData), pEntry->pszData, pszModulePath);
        } else {
            StringCchCopyW(szData, ARRAYSIZE(szData), pEntry->pszData);
        }
        
        result = RegSetValueExW(
            hKey,
            pEntry->pszValueName,
            0,
            REG_SZ,
            (const BYTE*)szData,
            (DWORD)(wcslen(szData) + 1) * sizeof(wchar_t));
    }
    else if (pEntry->dwType == REG_EXPAND_SZ && pEntry->pszData) {
        wchar_t szData[MAX_PATH * 2];
        StringCchCopyW(szData, ARRAYSIZE(szData), pEntry->pszData);
        
        result = RegSetValueExW(
            hKey,
            pEntry->pszValueName,
            0,
            REG_EXPAND_SZ,
            (const BYTE*)szData,
            (DWORD)(wcslen(szData) + 1) * sizeof(wchar_t));
    }
    else if (pEntry->dwType == REG_DWORD) {
        result = RegSetValueExW(
            hKey,
            pEntry->pszValueName,
            0,
            REG_DWORD,
            (const BYTE*)&pEntry->dwData,
            sizeof(DWORD));
    }
    
    RegCloseKey(hKey);
    return HRESULT_FROM_WIN32(result);
}

static HRESULT DeleteRegKey(HKEY hkeyRoot, LPCWSTR pszKeyName) {
    LONG result = RegDeleteTreeW(hkeyRoot, pszKeyName);
    if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    return HRESULT_FROM_WIN32(result);
}

// DllRegisterServer
STDAPI DllRegisterServer() {
    SEVENZIPVIEW_LOG(L"DllRegisterServer called");
    
    // Get DLL path
    wchar_t szModulePath[MAX_PATH];
    if (!GetModuleFileNameW(g_hModule, szModulePath, MAX_PATH)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    
    SEVENZIPVIEW_LOG(L"Module path: %s", szModulePath);
    
    // Registry entries - EXACTLY mirroring Windows native CompressedFolder/.zip structure
    // Reference: HKCR\.zip, HKCR\CompressedFolder, HKCR\CLSID\{E88DCCE0-B7B3-11d1-A9F0-00AA0060FA31}
    const REGISTRY_ENTRY rgRegEntries[] = {
        // ============================================================================
        // ProgID: SevenZipView.Archive (like CompressedFolder)
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\SevenZipView.Archive", nullptr, REG_SZ, L"7-Zip Archive (SevenZipView)", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SevenZipView.Archive", L"FriendlyTypeName", REG_SZ, L"7-Zip Archive", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SevenZipView.Archive\\DefaultIcon", nullptr, REG_SZ, L"%s,0", 0 },
        
        // CRITICAL: Link ProgID to our CLSID (like CompressedFolder\CLSID)
        { HKEY_CURRENT_USER, L"Software\\Classes\\SevenZipView.Archive\\CLSID", nullptr, REG_SZ, SEVENZIPVIEW_CLSID_STR, 0 },
        
        // ============================================================================
        // Shell\Open command - Use DelegateExecute like Windows native CompressedFolder
        // The ExecuteFolder CLSID {11dbb47c-a525-400b-9e80-a54615a090c0} handles folder browsing
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\SevenZipView.Archive\\Shell", nullptr, REG_SZ, L"Open", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SevenZipView.Archive\\Shell\\Open", L"MultiSelectModel", REG_SZ, L"Document", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SevenZipView.Archive\\Shell\\Open\\Command", nullptr, REG_EXPAND_SZ, L"%SystemRoot%\\Explorer.exe /idlist,%I,%L", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SevenZipView.Archive\\Shell\\Open\\Command", L"DelegateExecute", REG_SZ, L"{11dbb47c-a525-400b-9e80-a54615a090c0}", 0 },
        
        // ============================================================================
        // ShellEx handlers on ProgID (like CompressedFolder\ShellEx)
        // ============================================================================
        // StorageHandler - CRITICAL for browsing as folder
        { HKEY_CURRENT_USER, L"Software\\Classes\\SevenZipView.Archive\\ShellEx\\StorageHandler", nullptr, REG_SZ, SEVENZIPVIEW_CLSID_STR, 0 },
        
        // ContextMenuHandler
        { HKEY_CURRENT_USER, L"Software\\Classes\\SevenZipView.Archive\\ShellEx\\ContextMenuHandlers\\SevenZipView", nullptr, REG_SZ, SEVENZIPVIEW_CONTEXTMENU_CLSID_STR, 0 },
        
        // ============================================================================
        // Main ShellFolder CLSID registration
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CLSID_STR, nullptr, REG_SZ, L"SevenZipView.Archive", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CLSID_STR L"\\InProcServer32", nullptr, REG_SZ, L"%s", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CLSID_STR L"\\InProcServer32", L"ThreadingModel", REG_SZ, L"Apartment", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CLSID_STR L"\\ProgID", nullptr, REG_SZ, L"SevenZipView.Archive", 0 },
        
        // ShellFolder attributes - MUST match Windows CompressedFolder: 0x200001A0
        // SFGAO_FOLDER(0x20000000) | SFGAO_BROWSABLE(0x8000000) | SFGAO_DROPTARGET(0x100) | SFGAO_STREAM(0x400000) etc.
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CLSID_STR L"\\ShellFolder", L"Attributes", REG_DWORD, nullptr, 0x200001A0 },
        
        // ============================================================================
        // CRITICAL: Implemented Categories - "Browsable Shell Extension" 
        // Category {00021490-0000-0000-C000-000000000046} tells Explorer this CLSID is browsable
        // This is the KEY that allows double-click to browse into the archive like a folder
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CLSID_STR L"\\Implemented Categories", nullptr, REG_SZ, L"", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CLSID_STR L"\\Implemented Categories\\{00021490-0000-0000-C000-000000000046}", nullptr, REG_SZ, L"", 0 },
        
        // ============================================================================
        // .7z file extension association
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\.7z", nullptr, REG_SZ, L"SevenZipView.Archive", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\.7z", L"PerceivedType", REG_SZ, L"compressed", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\.7z", L"Content Type", REG_SZ, L"application/x-7z-compressed", 0 },
        
        // SubKey for our ProgID under extension (like .zip\CompressedFolder)
        { HKEY_CURRENT_USER, L"Software\\Classes\\.7z\\SevenZipView.Archive", nullptr, REG_SZ, L"", 0 },
        
        // OpenWithProgids
        { HKEY_CURRENT_USER, L"Software\\Classes\\.7z\\OpenWithProgids", L"SevenZipView.Archive", REG_SZ, L"", 0 },
        
        // ============================================================================
        // SystemFileAssociations - CRITICAL for Explorer to recognize the CLSID
        // This is how Windows native .zip works: SystemFileAssociations\.zip\CLSID
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\SystemFileAssociations\\.7z\\CLSID", nullptr, REG_SZ, SEVENZIPVIEW_CLSID_STR, 0 },
        
        // ============================================================================
        // ContextMenu CLSID
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CONTEXTMENU_CLSID_STR, nullptr, REG_SZ, L"SevenZipView Context Menu", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CONTEXTMENU_CLSID_STR L"\\InProcServer32", nullptr, REG_SZ, L"%s", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CONTEXTMENU_CLSID_STR L"\\InProcServer32", L"ThreadingModel", REG_SZ, L"Apartment", 0 },
        
        // ============================================================================
        // Property handler CLSID
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_PROPERTY_CLSID_STR, nullptr, REG_SZ, L"SevenZipView Property Handler", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_PROPERTY_CLSID_STR L"\\InProcServer32", nullptr, REG_SZ, L"%s", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_PROPERTY_CLSID_STR L"\\InProcServer32", L"ThreadingModel", REG_SZ, L"Both", 0 },
        
        // ============================================================================
        // Icon handler CLSID
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_ICON_CLSID_STR, nullptr, REG_SZ, L"SevenZipView Icon Handler", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_ICON_CLSID_STR L"\\InProcServer32", nullptr, REG_SZ, L"%s", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_ICON_CLSID_STR L"\\InProcServer32", L"ThreadingModel", REG_SZ, L"Apartment", 0 },
        
        // ============================================================================
        // Approved shell extensions (helps with security)
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", SEVENZIPVIEW_CLSID_STR, REG_SZ, L"SevenZipView Shell Folder", 0 },
        { HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", SEVENZIPVIEW_CONTEXTMENU_CLSID_STR, REG_SZ, L"SevenZipView Context Menu", 0 },
        { HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", SEVENZIPVIEW_PROPERTY_CLSID_STR, REG_SZ, L"SevenZipView Property Handler", 0 },
    };
    
    HRESULT hr = S_OK;
    
    for (const auto& entry : rgRegEntries) {
        HRESULT hrTemp = CreateRegKeyAndSetValue(&entry, szModulePath);
        if (FAILED(hrTemp)) {
            SEVENZIPVIEW_LOG(L"Failed to create registry key: %s", entry.pszKeyName);
            hr = hrTemp;
        }
    }
    
    // Notify shell of changes
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    
    SEVENZIPVIEW_LOG(L"DllRegisterServer completed with hr=0x%08X", hr);
    return hr;
}

// DllUnregisterServer
STDAPI DllUnregisterServer() {
    SEVENZIPVIEW_LOG(L"DllUnregisterServer called");
    
    // Keys to delete
    const wchar_t* rgKeysToDelete[] = {
        L"Software\\Classes\\SevenZipView.Archive",
        L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CLSID_STR L"\\Implemented Categories\\{00021490-0000-0000-C000-000000000046}",
        L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CLSID_STR L"\\Implemented Categories",
        L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CLSID_STR,
        L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_CONTEXTMENU_CLSID_STR,
        L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_PROPERTY_CLSID_STR,
        L"Software\\Classes\\CLSID\\" SEVENZIPVIEW_ICON_CLSID_STR,
        L"Software\\Classes\\.7z\\SevenZipView.Archive",
        L"Software\\Classes\\.7z\\OpenWithProgids",
        L"Software\\Classes\\.7z\\ShellEx",
        L"Software\\Classes\\SystemFileAssociations\\.7z",
    };
    
    for (const auto& key : rgKeysToDelete) {
        DeleteRegKey(HKEY_CURRENT_USER, key);
    }
    
    // Reset .7z to default (no ProgId)
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.7z",
                      0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, nullptr);  // Delete default value
        RegDeleteValueW(hKey, L"PerceivedType");
        RegDeleteValueW(hKey, L"Content Type");
        RegCloseKey(hKey);
    }
    
    // Remove approved extensions
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
                      0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, SEVENZIPVIEW_CLSID_STR);
        RegDeleteValueW(hKey, SEVENZIPVIEW_CONTEXTMENU_CLSID_STR);
        RegDeleteValueW(hKey, SEVENZIPVIEW_PROPERTY_CLSID_STR);
        RegCloseKey(hKey);
    }
    
    // Notify shell of changes
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    
    SEVENZIPVIEW_LOG(L"DllUnregisterServer completed");
    return S_OK;
}

// DllInstall (for regsvr32 /i)
STDAPI DllInstall(BOOL bInstall, LPCWSTR pszCmdLine) {
    if (bInstall) {
        return DllRegisterServer();
    } else {
        return DllUnregisterServer();
    }
}
