/*
** SQLiteView - Windows Explorer Shell Extension
** DLL Entry Point and Registration
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovação. All rights reserved.
** Licensed under MIT License
**
** Based on Microsoft Windows-classic-samples explorerdataprovider pattern
** Uses HKEY_CURRENT_USER\Software\Classes for per-user registration without admin
*/

#include "Common.h"
#include "ShellFolder.h"
#include "PreviewHandler.h"
#include "ContextMenu.h"
#include "PropertyHandler.h"
#include "IconHandler.h"

// Definir CLSIDs - instâncias reais
// {E8A3C5B7-9D2F-4A1E-8B6C-3D5E7F9A2C4D}
const GUID CLSID_SQLiteViewFolder = 
    { 0xe8a3c5b7, 0x9d2f, 0x4a1e, { 0x8b, 0x6c, 0x3d, 0x5e, 0x7f, 0x9a, 0x2c, 0x4d } };

// {F9B4D6C8-AE3F-5B2F-9C7D-4E6F8A1B3D5E}
const GUID CLSID_SQLiteViewPreview =
    { 0xf9b4d6c8, 0xae3f, 0x5b2f, { 0x9c, 0x7d, 0x4e, 0x6f, 0x8a, 0x1b, 0x3d, 0x5e } };

// {A1C5E7F9-BF4A-6C3D-AE8F-5B7D9A2C4E6F}
const GUID CLSID_SQLiteViewProperty =
    { 0xa1c5e7f9, 0xbf4a, 0x6c3d, { 0xae, 0x8f, 0x5b, 0x7d, 0x9a, 0x2c, 0x4e, 0x6f } };

// {B2D6F8A1-CF5B-7D4E-BF9A-6C8E1B3D5F7A}
const GUID CLSID_SQLiteViewContextMenu =
    { 0xb2d6f8a1, 0xcf5b, 0x7d4e, { 0xbf, 0x9a, 0x6c, 0x8e, 0x1b, 0x3d, 0x5f, 0x7a } };

// Definir os PKEYs customizados
// {D9FFDCA0-1234-5678-9ABC-AABBCCDDEEFF}
const PROPERTYKEY PKEY_SQLite_TableCount = { { 0xD9FFDCA0, 0x1234, 0x5678, { 0x9A, 0xBC, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } }, 1 };
const PROPERTYKEY PKEY_SQLite_ViewCount = { { 0xD9FFDCA0, 0x1234, 0x5678, { 0x9A, 0xBC, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } }, 2 };
const PROPERTYKEY PKEY_SQLite_IndexCount = { { 0xD9FFDCA0, 0x1234, 0x5678, { 0x9A, 0xBC, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } }, 3 };
const PROPERTYKEY PKEY_SQLite_PageSize = { { 0xD9FFDCA0, 0x1234, 0x5678, { 0x9A, 0xBC, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } }, 4 };
const PROPERTYKEY PKEY_SQLite_Encoding = { { 0xD9FFDCA0, 0x1234, 0x5678, { 0x9A, 0xBC, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } }, 5 };

// Global variables
HMODULE g_hModule = nullptr;
LONG g_DllRefCount = 0;

// Supported file extensions
const wchar_t* g_SupportedExtensions[] = {
    L".db",
    L".sqlite",
    L".sqlite3",
    L".db3",
    L".sdb"
};

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            g_hModule = hModule;
            DisableThreadLibraryCalls(hModule);
            SQLITEVIEW_LOG(L"DLL_PROCESS_ATTACH - SQLiteView.dll loaded in process");
            break;
            
        case DLL_PROCESS_DETACH:
            SQLITEVIEW_LOG(L"DLL_PROCESS_DETACH - SQLiteView.dll unloading");
            SQLiteView::DatabasePool::Instance().Clear();
            break;
    }
    return TRUE;
}

// DllCanUnloadNow - Check if DLL can be unloaded
STDAPI DllCanUnloadNow() {
    return (g_DllRefCount == 0) ? S_OK : S_FALSE;
}

// DllGetClassObject - Get class factory
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    // Log the request
    wchar_t clsidStr[64], riidStr[64];
    StringFromGUID2(rclsid, clsidStr, 64);
    StringFromGUID2(riid, riidStr, 64);
    SQLITEVIEW_LOG(L"DllGetClassObject CLSID=%s riid=%s", clsidStr, riidStr);

    // Check for supported CLSIDs
    if (!IsEqualCLSID(rclsid, CLSID_SQLiteViewFolder) &&
        !IsEqualCLSID(rclsid, CLSID_SQLiteViewPreview) &&
        !IsEqualCLSID(rclsid, CLSID_SQLiteViewContextMenu) &&
        !IsEqualCLSID(rclsid, CLSID_SQLiteViewProperty)) {
        SQLITEVIEW_LOG(L"  -> CLASS_E_CLASSNOTAVAILABLE");
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    SQLiteView::ClassFactory* factory = new (std::nothrow) SQLiteView::ClassFactory(rclsid);
    if (!factory) return E_OUTOFMEMORY;

    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();

    SQLITEVIEW_LOG(L"  -> DllGetClassObject returning 0x%08X", hr);
    return hr;
}

// ============================================================================
// Registry helper functions with detailed error logging
// Based on Microsoft Windows-classic-samples pattern
// ============================================================================

// Structure to hold registry entry data
struct REGISTRY_ENTRY {
    HKEY    hkeyRoot;
    LPCWSTR pszKeyName;
    LPCWSTR pszValueName;   // NULL for default value
    DWORD   dwType;
    LPCWSTR pszData;        // For REG_SZ
    DWORD   dwData;         // For REG_DWORD
};

// Create registry key and set value with detailed error logging
static HRESULT CreateRegKeyAndSetValue(const REGISTRY_ENTRY* pEntry, LPCWSTR pszModulePath) {
    HKEY hKey = nullptr;
    
    // Build the full key path for logging
    wchar_t fullKeyPath[512];
    StringCchPrintfW(fullKeyPath, 512, L"%s\\%s", 
        (pEntry->hkeyRoot == HKEY_CURRENT_USER) ? L"HKCU" : 
        (pEntry->hkeyRoot == HKEY_LOCAL_MACHINE) ? L"HKLM" : L"HKCR",
        pEntry->pszKeyName);
    
    SQLITEVIEW_LOG(L"  Creating key: %s", fullKeyPath);
    
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
        SQLITEVIEW_LOG(L"    FAILED RegCreateKeyExW: error=%d (0x%08X)", result, result);
        return HRESULT_FROM_WIN32(result);
    }
    
    // Now set the value
    if (pEntry->dwType == REG_SZ && pEntry->pszData) {
        wchar_t szData[MAX_PATH * 2];
        
        // Check if data contains %s placeholder for module path
        if (wcsstr(pEntry->pszData, L"%s") != nullptr) {
            StringCchPrintfW(szData, ARRAYSIZE(szData), pEntry->pszData, pszModulePath);
        } else {
            StringCchCopyW(szData, ARRAYSIZE(szData), pEntry->pszData);
        }
        
        SQLITEVIEW_LOG(L"    Setting value '%s' = '%s'", 
            pEntry->pszValueName ? pEntry->pszValueName : L"(Default)",
            szData);
        
        result = RegSetValueExW(
            hKey,
            pEntry->pszValueName,
            0,
            REG_SZ,
            (const BYTE*)szData,
            (DWORD)(wcslen(szData) + 1) * sizeof(wchar_t));
    }
    else if (pEntry->dwType == REG_SZ && pEntry->pszData == nullptr) {
        // Just create the key, no value to set
        SQLITEVIEW_LOG(L"    Key created (no value)");
        result = ERROR_SUCCESS;
    }
    else if (pEntry->dwType == REG_EXPAND_SZ && pEntry->pszData) {
        wchar_t szData[MAX_PATH * 2];
        StringCchCopyW(szData, ARRAYSIZE(szData), pEntry->pszData);
        
        SQLITEVIEW_LOG(L"    Setting REG_EXPAND_SZ value '%s' = '%s'", 
            pEntry->pszValueName ? pEntry->pszValueName : L"(Default)",
            szData);
        
        result = RegSetValueExW(
            hKey,
            pEntry->pszValueName,
            0,
            REG_EXPAND_SZ,
            (const BYTE*)szData,
            (DWORD)(wcslen(szData) + 1) * sizeof(wchar_t));
    }
    else if (pEntry->dwType == REG_DWORD) {
        SQLITEVIEW_LOG(L"    Setting DWORD value '%s' = 0x%08X",
            pEntry->pszValueName ? pEntry->pszValueName : L"(Default)",
            pEntry->dwData);
        
        result = RegSetValueExW(
            hKey,
            pEntry->pszValueName,
            0,
            REG_DWORD,
            (const BYTE*)&pEntry->dwData,
            sizeof(DWORD));
    }
    
    RegCloseKey(hKey);
    
    if (result != ERROR_SUCCESS) {
        SQLITEVIEW_LOG(L"    FAILED RegSetValueExW: error=%d (0x%08X)", result, result);
        return HRESULT_FROM_WIN32(result);
    }
    
    SQLITEVIEW_LOG(L"    SUCCESS");
    return S_OK;
}

// Delete registry key recursively
static HRESULT DeleteRegistryKeyRecursive(HKEY hKeyRoot, LPCWSTR pszKeyName) {
    LONG result = RegDeleteTreeW(hKeyRoot, pszKeyName);
    if (result == ERROR_FILE_NOT_FOUND) {
        return S_OK;  // Key doesn't exist, that's fine
    }
    return HRESULT_FROM_WIN32(result);
}

// ============================================================================
// DllRegisterServer - Register the DLL
// Uses HKEY_CURRENT_USER\Software\Classes pattern from Microsoft samples
// This avoids requiring admin rights and works with UAC
// ============================================================================
STDAPI DllRegisterServer() {
    SQLITEVIEW_LOG(L"========================================");
    SQLITEVIEW_LOG(L"DllRegisterServer called");
    SQLITEVIEW_LOG(L"========================================");
    
    // Get module path
    wchar_t modulePath[MAX_PATH];
    if (!GetModuleFileNameW(g_hModule, modulePath, MAX_PATH)) {
        DWORD err = GetLastError();
        SQLITEVIEW_LOG(L"FATAL: GetModuleFileName failed, error=%d", err);
        return HRESULT_FROM_WIN32(err);
    }
    
    SQLITEVIEW_LOG(L"Module path: %s", modulePath);
    
    // SFGAO attributes - same as CompressedFolder (zipfldr.dll): 0x200001a0
    // SFGAO_FOLDER (0x20000000) | SFGAO_BROWSABLE (0x08000000) | 
    // SFGAO_HASSUBFOLDER (0x80000000) | SFGAO_NONENUMERATED (0x00100000) |
    // Other flags from CompressedFolder
    DWORD dwFolderAttributes = 0x200001a0;  // Exact same as zipfldr
    
    // Build CLSID strings
    wchar_t szFolderCLSID[64], szPreviewCLSID[64], szContextMenuCLSID[64], szPropertyCLSID[64];
    StringFromGUID2(CLSID_SQLiteViewFolder, szFolderCLSID, ARRAYSIZE(szFolderCLSID));
    StringFromGUID2(CLSID_SQLiteViewPreview, szPreviewCLSID, ARRAYSIZE(szPreviewCLSID));
    StringFromGUID2(CLSID_SQLiteViewContextMenu, szContextMenuCLSID, ARRAYSIZE(szContextMenuCLSID));
    StringFromGUID2(CLSID_SQLiteViewProperty, szPropertyCLSID, ARRAYSIZE(szPropertyCLSID));
    
    SQLITEVIEW_LOG(L"Shell Folder CLSID: %s", szFolderCLSID);
    SQLITEVIEW_LOG(L"Preview CLSID: %s", szPreviewCLSID);
    SQLITEVIEW_LOG(L"ContextMenu CLSID: %s", szContextMenuCLSID);
    SQLITEVIEW_LOG(L"Property CLSID: %s", szPropertyCLSID);
    
    // Build key paths
    wchar_t szFolderKey[256], szFolderInProcKey[256], szFolderShellFolderKey[256], szFolderDefaultIconKey[256];
    wchar_t szFolderProgIdKey[256], szFolderCategoryKey[256];
    wchar_t szPreviewKey[256], szPreviewInProcKey[256];
    wchar_t szContextMenuKey[256], szContextMenuInProcKey[256];
    wchar_t szPropertyKey[256], szPropertyInProcKey[256];
    
    StringCchPrintfW(szFolderKey, 256, L"Software\\Classes\\CLSID\\%s", szFolderCLSID);
    StringCchPrintfW(szFolderInProcKey, 256, L"Software\\Classes\\CLSID\\%s\\InProcServer32", szFolderCLSID);
    StringCchPrintfW(szFolderShellFolderKey, 256, L"Software\\Classes\\CLSID\\%s\\ShellFolder", szFolderCLSID);
    StringCchPrintfW(szFolderDefaultIconKey, 256, L"Software\\Classes\\CLSID\\%s\\DefaultIcon", szFolderCLSID);
    StringCchPrintfW(szFolderProgIdKey, 256, L"Software\\Classes\\CLSID\\%s\\ProgID", szFolderCLSID);
    // CATID_BrowsableShellExt = {00021490-0000-0000-C000-000000000046}
    StringCchPrintfW(szFolderCategoryKey, 256, L"Software\\Classes\\CLSID\\%s\\Implemented Categories\\{00021490-0000-0000-C000-000000000046}", szFolderCLSID);
    
    StringCchPrintfW(szPreviewKey, 256, L"Software\\Classes\\CLSID\\%s", szPreviewCLSID);
    StringCchPrintfW(szPreviewInProcKey, 256, L"Software\\Classes\\CLSID\\%s\\InProcServer32", szPreviewCLSID);
    
    StringCchPrintfW(szContextMenuKey, 256, L"Software\\Classes\\CLSID\\%s", szContextMenuCLSID);
    StringCchPrintfW(szContextMenuInProcKey, 256, L"Software\\Classes\\CLSID\\%s\\InProcServer32", szContextMenuCLSID);
    
    StringCchPrintfW(szPropertyKey, 256, L"Software\\Classes\\CLSID\\%s", szPropertyCLSID);
    StringCchPrintfW(szPropertyInProcKey, 256, L"Software\\Classes\\CLSID\\%s\\InProcServer32", szPropertyCLSID);
    
    // Icon path
    wchar_t szIconPath[MAX_PATH + 10];
    StringCchPrintfW(szIconPath, ARRAYSIZE(szIconPath), L"%s,0", modulePath);
    
    // ========================================================================
    // Registry entries array - based on Microsoft explorerdataprovider sample
    // Using HKEY_CURRENT_USER\Software\Classes to avoid admin requirements
    // ========================================================================
    REGISTRY_ENTRY rgRegEntries[] = {
        // ================================================================
        // Shell Folder CLSID registration (like CompressedFolder zipfldr.dll)
        // ================================================================
        { HKEY_CURRENT_USER, szFolderKey, nullptr, REG_SZ, L"SQLiteView Database Browser", 0 },
        { HKEY_CURRENT_USER, szFolderInProcKey, nullptr, REG_SZ, L"%s", 0 },  // %s = modulePath
        { HKEY_CURRENT_USER, szFolderInProcKey, L"ThreadingModel", REG_SZ, L"Apartment", 0 },
        { HKEY_CURRENT_USER, szFolderShellFolderKey, L"Attributes", REG_DWORD, nullptr, dwFolderAttributes },
        { HKEY_CURRENT_USER, szFolderShellFolderKey, L"UseDropHandler", REG_SZ, L"", 0 },  // Same as zipfldr
        { HKEY_CURRENT_USER, szFolderDefaultIconKey, nullptr, REG_SZ, szIconPath, 0 },
        { HKEY_CURRENT_USER, szFolderProgIdKey, nullptr, REG_SZ, L"SQLiteView.Database", 0 },
        // CATID_BrowsableShellExt - tells Explorer this is a browsable namespace extension
        { HKEY_CURRENT_USER, szFolderCategoryKey, nullptr, REG_SZ, nullptr, 0 },  // Just create the key (empty default value)
        
        // ================================================================
        // Preview Handler CLSID registration
        // ================================================================
        { HKEY_CURRENT_USER, szPreviewKey, nullptr, REG_SZ, L"SQLiteView Preview Handler", 0 },
        { HKEY_CURRENT_USER, szPreviewInProcKey, nullptr, REG_SZ, L"%s", 0 },
        { HKEY_CURRENT_USER, szPreviewInProcKey, L"ThreadingModel", REG_SZ, L"Apartment", 0 },
        // AppId for preview handler host (prevhost.exe)
        { HKEY_CURRENT_USER, szPreviewKey, L"AppId", REG_SZ, L"{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}", 0 },
        
        // ================================================================
        // Context Menu Handler CLSID registration
        // ================================================================
        { HKEY_CURRENT_USER, szContextMenuKey, nullptr, REG_SZ, L"SQLiteView Context Menu", 0 },
        { HKEY_CURRENT_USER, szContextMenuInProcKey, nullptr, REG_SZ, L"%s", 0 },
        { HKEY_CURRENT_USER, szContextMenuInProcKey, L"ThreadingModel", REG_SZ, L"Apartment", 0 },
        
        // ================================================================
        // Property Handler CLSID registration
        // ================================================================
        { HKEY_CURRENT_USER, szPropertyKey, nullptr, REG_SZ, L"SQLiteView Property Handler", 0 },
        { HKEY_CURRENT_USER, szPropertyInProcKey, nullptr, REG_SZ, L"%s", 0 },
        { HKEY_CURRENT_USER, szPropertyInProcKey, L"ThreadingModel", REG_SZ, L"Both", 0 },
        
        // ================================================================
        // ProgId registration - SQLiteView.Database (like CompressedFolder)
        // Uses DelegateExecute with CLSID_ExecuteFolder from explorerframe.dll
        // This tells Explorer to browse into our namespace extension
        // ================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database", nullptr, REG_SZ, L"SQLite Database", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database", L"FriendlyTypeName", REG_SZ, L"SQLite Database", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database\\CLSID", nullptr, REG_SZ, szFolderCLSID, 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database\\DefaultIcon", nullptr, REG_SZ, szIconPath, 0 },
        // Shell\Open\Command with DelegateExecute - same pattern as CompressedFolder
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database\\Shell\\Open", nullptr, REG_SZ, L"Open", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database\\Shell\\Open\\Command", nullptr, REG_EXPAND_SZ, L"%SystemRoot%\\Explorer.exe /idlist,%I,%L", 0 },
        // CLSID_ExecuteFolder - internal Windows handler for opening namespace extensions
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database\\Shell\\Open\\Command", L"DelegateExecute", REG_SZ, L"{11dbb47c-a525-400b-9e80-a54615a090c0}", 0 },
    };
    
    SQLITEVIEW_LOG(L"");
    SQLITEVIEW_LOG(L"--- Registering CLSID and ProgId entries ---");
    
    HRESULT hr = S_OK;
    for (int i = 0; SUCCEEDED(hr) && (i < ARRAYSIZE(rgRegEntries)); i++) {
        hr = CreateRegKeyAndSetValue(&rgRegEntries[i], modulePath);
        if (FAILED(hr)) {
            SQLITEVIEW_LOG(L"FATAL: Registration failed at entry %d, hr=0x%08X", i, hr);
            return hr;
        }
    }
    
    // ========================================================================
    // Register ShellEx handlers on ProgId
    // ========================================================================
    SQLITEVIEW_LOG(L"");
    SQLITEVIEW_LOG(L"--- Registering ShellEx handlers on ProgId ---");
    
    wchar_t szProgIdShellEx[256];
    
    // Preview handler
    StringCchPrintfW(szProgIdShellEx, 256, L"Software\\Classes\\SQLiteView.Database\\ShellEx\\{8895b1c6-b41f-4c1c-a562-0d564250836f}");
    REGISTRY_ENTRY previewEntry = { HKEY_CURRENT_USER, szProgIdShellEx, nullptr, REG_SZ, szPreviewCLSID, 0 };
    hr = CreateRegKeyAndSetValue(&previewEntry, modulePath);
    if (FAILED(hr)) return hr;
    
    // Context menu handler
    StringCchPrintfW(szProgIdShellEx, 256, L"Software\\Classes\\SQLiteView.Database\\ShellEx\\ContextMenuHandlers\\SQLiteView");
    REGISTRY_ENTRY ctxEntry = { HKEY_CURRENT_USER, szProgIdShellEx, nullptr, REG_SZ, szContextMenuCLSID, 0 };
    hr = CreateRegKeyAndSetValue(&ctxEntry, modulePath);
    if (FAILED(hr)) return hr;
    
    // Property handler
    StringCchPrintfW(szProgIdShellEx, 256, L"Software\\Classes\\SQLiteView.Database\\ShellEx\\{D5CDD505-2E9C-101B-9397-08002B2CF9AE}");
    REGISTRY_ENTRY propEntry = { HKEY_CURRENT_USER, szProgIdShellEx, nullptr, REG_SZ, szPropertyCLSID, 0 };
    hr = CreateRegKeyAndSetValue(&propEntry, modulePath);
    if (FAILED(hr)) return hr;
    
    // ========================================================================
    // Register file associations
    // ========================================================================
    SQLITEVIEW_LOG(L"");
    SQLITEVIEW_LOG(L"--- Registering file associations ---");
    
    for (const wchar_t* ext : g_SupportedExtensions) {
        SQLITEVIEW_LOG(L"Processing extension: %s", ext);
        
        wchar_t szExtKey[64], szExtShellExKey[256];
        StringCchPrintfW(szExtKey, 64, L"Software\\Classes\\%s", ext);
        
        // Associate extension with ProgId (like .zip uses CompressedFolder)
        REGISTRY_ENTRY extEntries[] = {
            { HKEY_CURRENT_USER, szExtKey, nullptr, REG_SZ, L"SQLiteView.Database", 0 },
            { HKEY_CURRENT_USER, szExtKey, L"Content Type", REG_SZ, L"application/x-sqlite3", 0 },
            { HKEY_CURRENT_USER, szExtKey, L"PerceivedType", REG_SZ, L"compressed", 0 },
        };
        
        for (int i = 0; SUCCEEDED(hr) && (i < ARRAYSIZE(extEntries)); i++) {
            hr = CreateRegKeyAndSetValue(&extEntries[i], modulePath);
            if (FAILED(hr)) return hr;
        }
        
        // Create the empty subkey like .zip\CompressedFolder
        wchar_t szExtProgIdKey[128];
        StringCchPrintfW(szExtProgIdKey, 128, L"Software\\Classes\\%s\\SQLiteView.Database", ext);
        REGISTRY_ENTRY extProgIdKeyEntry = { HKEY_CURRENT_USER, szExtProgIdKey, nullptr, REG_SZ, nullptr, 0 };
        hr = CreateRegKeyAndSetValue(&extProgIdKeyEntry, modulePath);
        if (FAILED(hr)) return hr;
        
        // OpenWithProgids entry (like .zip uses)
        wchar_t szOpenWithKey[128];
        StringCchPrintfW(szOpenWithKey, 128, L"Software\\Classes\\%s\\OpenWithProgids", ext);
        REGISTRY_ENTRY openWithEntry = { HKEY_CURRENT_USER, szOpenWithKey, L"SQLiteView.Database", REG_SZ, L"", 0 };
        hr = CreateRegKeyAndSetValue(&openWithEntry, modulePath);
        if (FAILED(hr)) return hr;
        
        // Also register ShellEx handlers directly on extension for compatibility
        StringCchPrintfW(szExtShellExKey, 256, L"Software\\Classes\\%s\\ShellEx\\{8895b1c6-b41f-4c1c-a562-0d564250836f}", ext);
        REGISTRY_ENTRY extPreviewEntry = { HKEY_CURRENT_USER, szExtShellExKey, nullptr, REG_SZ, szPreviewCLSID, 0 };
        hr = CreateRegKeyAndSetValue(&extPreviewEntry, modulePath);
        if (FAILED(hr)) return hr;
        
        StringCchPrintfW(szExtShellExKey, 256, L"Software\\Classes\\%s\\ShellEx\\ContextMenuHandlers\\SQLiteView", ext);
        REGISTRY_ENTRY extCtxEntry = { HKEY_CURRENT_USER, szExtShellExKey, nullptr, REG_SZ, szContextMenuCLSID, 0 };
        hr = CreateRegKeyAndSetValue(&extCtxEntry, modulePath);
        if (FAILED(hr)) return hr;
        
        StringCchPrintfW(szExtShellExKey, 256, L"Software\\Classes\\%s\\ShellEx\\{D5CDD505-2E9C-101B-9397-08002B2CF9AE}", ext);
        REGISTRY_ENTRY extPropEntry = { HKEY_CURRENT_USER, szExtShellExKey, nullptr, REG_SZ, szPropertyCLSID, 0 };
        hr = CreateRegKeyAndSetValue(&extPropEntry, modulePath);
        if (FAILED(hr)) return hr;
    }
    
    // ========================================================================
    // Register preview handler in approved list (requires admin/HKLM)
    // This is optional and may fail without admin rights
    // ========================================================================
    SQLITEVIEW_LOG(L"");
    SQLITEVIEW_LOG(L"--- Attempting HKLM registrations (optional, may fail without admin) ---");
    
    {
        HKEY hKey;
        LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
            0, KEY_WRITE, &hKey);
        
        if (result == ERROR_SUCCESS) {
            RegSetValueExW(hKey, szPreviewCLSID, 0, REG_SZ,
                (const BYTE*)L"SQLiteView Preview Handler",
                (DWORD)(wcslen(L"SQLiteView Preview Handler") + 1) * sizeof(wchar_t));
            RegCloseKey(hKey);
            SQLITEVIEW_LOG(L"  PreviewHandlers list: SUCCESS");
        } else {
            SQLITEVIEW_LOG(L"  PreviewHandlers list: SKIPPED (not admin) error=%d", result);
        }
    }
    
    // Shell Extensions Approved list
    {
        HKEY hKey;
        LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
            0, KEY_WRITE, &hKey);
        
        if (result == ERROR_SUCCESS) {
            RegSetValueExW(hKey, szContextMenuCLSID, 0, REG_SZ,
                (const BYTE*)L"SQLiteView Context Menu Handler",
                (DWORD)(wcslen(L"SQLiteView Context Menu Handler") + 1) * sizeof(wchar_t));
            
            RegSetValueExW(hKey, szFolderCLSID, 0, REG_SZ,
                (const BYTE*)L"SQLiteView Shell Folder",
                (DWORD)(wcslen(L"SQLiteView Shell Folder") + 1) * sizeof(wchar_t));
            
            RegSetValueExW(hKey, szPropertyCLSID, 0, REG_SZ,
                (const BYTE*)L"SQLiteView Property Handler",
                (DWORD)(wcslen(L"SQLiteView Property Handler") + 1) * sizeof(wchar_t));
            
            RegCloseKey(hKey);
            SQLITEVIEW_LOG(L"  Shell Extensions Approved: SUCCESS");
        } else {
            SQLITEVIEW_LOG(L"  Shell Extensions Approved: SKIPPED (not admin) error=%d", result);
        }
    }
    
    // Notify shell of changes
    SQLITEVIEW_LOG(L"");
    SQLITEVIEW_LOG(L"Notifying Shell of changes...");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    
    SQLITEVIEW_LOG(L"========================================");
    SQLITEVIEW_LOG(L"DllRegisterServer: COMPLETED SUCCESSFULLY");
    SQLITEVIEW_LOG(L"========================================");
    
    return S_OK;
}

// ============================================================================
// DllUnregisterServer - Unregister the DLL
// ============================================================================
STDAPI DllUnregisterServer() {
    SQLITEVIEW_LOG(L"DllUnregisterServer called");
    
    HRESULT hr = S_OK;
    
    // Build CLSID strings
    wchar_t szFolderCLSID[64], szPreviewCLSID[64], szContextMenuCLSID[64], szPropertyCLSID[64];
    StringFromGUID2(CLSID_SQLiteViewFolder, szFolderCLSID, ARRAYSIZE(szFolderCLSID));
    StringFromGUID2(CLSID_SQLiteViewPreview, szPreviewCLSID, ARRAYSIZE(szPreviewCLSID));
    StringFromGUID2(CLSID_SQLiteViewContextMenu, szContextMenuCLSID, ARRAYSIZE(szContextMenuCLSID));
    StringFromGUID2(CLSID_SQLiteViewProperty, szPropertyCLSID, ARRAYSIZE(szPropertyCLSID));
    
    wchar_t key[256];
    
    // Unregister CLSIDs from HKCU\Software\Classes
    StringCchPrintfW(key, 256, L"Software\\Classes\\CLSID\\%s", szFolderCLSID);
    DeleteRegistryKeyRecursive(HKEY_CURRENT_USER, key);
    
    StringCchPrintfW(key, 256, L"Software\\Classes\\CLSID\\%s", szPreviewCLSID);
    DeleteRegistryKeyRecursive(HKEY_CURRENT_USER, key);
    
    StringCchPrintfW(key, 256, L"Software\\Classes\\CLSID\\%s", szContextMenuCLSID);
    DeleteRegistryKeyRecursive(HKEY_CURRENT_USER, key);
    
    StringCchPrintfW(key, 256, L"Software\\Classes\\CLSID\\%s", szPropertyCLSID);
    DeleteRegistryKeyRecursive(HKEY_CURRENT_USER, key);
    
    // Also try HKEY_CLASSES_ROOT (for old installations)
    StringCchPrintfW(key, 256, L"CLSID\\%s", szFolderCLSID);
    DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, key);
    
    StringCchPrintfW(key, 256, L"CLSID\\%s", szPreviewCLSID);
    DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, key);
    
    StringCchPrintfW(key, 256, L"CLSID\\%s", szContextMenuCLSID);
    DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, key);
    
    StringCchPrintfW(key, 256, L"CLSID\\%s", szPropertyCLSID);
    DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, key);
    
    // Remove ProgId
    DeleteRegistryKeyRecursive(HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database");
    DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, L"SQLiteView.Database");
    
    // Unregister file extensions - remove ShellEx and clean up
    for (const wchar_t* ext : g_SupportedExtensions) {
        // Remove from HKCU
        StringCchPrintfW(key, 256, L"Software\\Classes\\%s\\ShellEx", ext);
        DeleteRegistryKeyRecursive(HKEY_CURRENT_USER, key);
        
        // Clean up default value if it points to our ProgId
        HKEY hKey;
        StringCchPrintfW(key, 256, L"Software\\Classes\\%s", ext);
        if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
            wchar_t value[256] = {0};
            DWORD size = sizeof(value);
            if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)value, &size) == ERROR_SUCCESS) {
                if (wcscmp(value, L"SQLiteView.Database") == 0) {
                    RegDeleteValueW(hKey, nullptr);
                }
            }
            RegDeleteValueW(hKey, L"Content Type");
            RegDeleteValueW(hKey, L"PerceivedType");
            RegCloseKey(hKey);
        }
        
        // Also try HKEY_CLASSES_ROOT (for old installations)
        StringCchPrintfW(key, 256, L"%s\\ShellEx", ext);
        DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, key);
        
        if (RegOpenKeyExW(HKEY_CLASSES_ROOT, ext, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
            wchar_t value[256] = {0};
            DWORD size = sizeof(value);
            if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)value, &size) == ERROR_SUCCESS) {
                if (wcscmp(value, L"SQLiteView.Database") == 0) {
                    RegDeleteValueW(hKey, nullptr);
                }
            }
            RegDeleteValueW(hKey, L"PerceivedType");
            RegCloseKey(hKey);
        }
    }
    
    // Remove from preview handlers list (HKLM)
    {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
            0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, szPreviewCLSID);
            RegCloseKey(hKey);
        }
    }
    
    // Remove from Shell Extensions Approved list (HKLM)
    {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
            0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, szContextMenuCLSID);
            RegDeleteValueW(hKey, szFolderCLSID);
            RegDeleteValueW(hKey, szPropertyCLSID);
            RegCloseKey(hKey);
        }
    }
    
    // Notify shell of changes
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    
    SQLITEVIEW_LOG(L"DllUnregisterServer: COMPLETED");
    return S_OK;
}

// DllInstall - Per-user registration
STDAPI DllInstall(BOOL bInstall, LPCWSTR pszCmdLine) {
    SQLITEVIEW_LOG(L"DllInstall called: bInstall=%d, cmdLine=%s", 
        bInstall, pszCmdLine ? pszCmdLine : L"(null)");
    
    if (bInstall) {
        return DllRegisterServer();
    } else {
        return DllUnregisterServer();
    }
}
