/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** DLL Entry Point and COM Registration
*/

#include "Common.h"
#include "ShellFolder.h"
#include "PreviewHandler.h"
#include "PropertyHandler.h"
#include "ContextMenu.h"
#include "IconHandler.h"
#include <shlobj.h>

// Global variables
HMODULE g_hModule = nullptr;
LONG g_DllRefCount = 0;

// CLSID definitions
// {A1B2C3D4-E5F6-7A8B-9C0D-1E2F3A4B5C6D}
extern "C" const GUID CLSID_SQLiteViewFolder = 
    { 0xA1B2C3D4, 0xE5F6, 0x7A8B, { 0x9C, 0x0D, 0x1E, 0x2F, 0x3A, 0x4B, 0x5C, 0x6D } };

// {B2C3D4E5-F6A7-8B9C-0D1E-2F3A4B5C6D7E}
extern "C" const GUID CLSID_SQLiteViewPreview = 
    { 0xB2C3D4E5, 0xF6A7, 0x8B9C, { 0x0D, 0x1E, 0x2F, 0x3A, 0x4B, 0x5C, 0x6D, 0x7E } };

// {C3D4E5F6-A7B8-9C0D-1E2F-3A4B5C6D7E8F}
extern "C" const GUID CLSID_SQLiteViewProperty = 
    { 0xC3D4E5F6, 0xA7B8, 0x9C0D, { 0x1E, 0x2F, 0x3A, 0x4B, 0x5C, 0x6D, 0x7E, 0x8F } };

// {D4E5F6A7-B8C9-0D1E-2F3A-4B5C6D7E8F9A}
extern "C" const GUID CLSID_SQLiteViewContextMenu = 
    { 0xD4E5F6A7, 0xB8C9, 0x0D1E, { 0x2F, 0x3A, 0x4B, 0x5C, 0x6D, 0x7E, 0x8F, 0x9A } };

// {E5F6A7B8-C9D0-1E2F-3A4B-5C6D7E8F9A0B}
extern "C" const GUID CLSID_SQLiteViewIcon = 
    { 0xE5F6A7B8, 0xC9D0, 0x1E2F, { 0x3A, 0x4B, 0x5C, 0x6D, 0x7E, 0x8F, 0x9A, 0x0B } };

// Custom property keys
namespace SQLiteView {
    // Define custom property keys using FMTID_Storage-based GUIDs
    // {D5CDD505-2E9C-101B-9397-08002B2CF9AE} - FMTID_SummaryInformation base
    static const GUID FMTID_SQLiteProperties = 
        { 0xD5CDD505, 0x2E9C, 0x101B, { 0x93, 0x97, 0x08, 0x00, 0x2B, 0x2C, 0xF9, 0xAE } };
    
    const PROPERTYKEY PKEY_SQLite_TableCount = { FMTID_SQLiteProperties, 100 };
    const PROPERTYKEY PKEY_SQLite_ViewCount = { FMTID_SQLiteProperties, 101 };
    const PROPERTYKEY PKEY_SQLite_IndexCount = { FMTID_SQLiteProperties, 102 };
    const PROPERTYKEY PKEY_SQLite_TriggerCount = { FMTID_SQLiteProperties, 103 };
    const PROPERTYKEY PKEY_SQLite_RecordCount = { FMTID_SQLiteProperties, 104 };
    const PROPERTYKEY PKEY_SQLite_PageSize = { FMTID_SQLiteProperties, 105 };
    const PROPERTYKEY PKEY_SQLite_Encoding = { FMTID_SQLiteProperties, 106 };
    const PROPERTYKEY PKEY_SQLite_SQLiteVersion = { FMTID_SQLiteProperties, 107 };
    const PROPERTYKEY PKEY_SQLite_SchemaVersion = { FMTID_SQLiteProperties, 108 };
    const PROPERTYKEY PKEY_SQLite_ColumnType = { FMTID_SQLiteProperties, 109 };
    const PROPERTYKEY PKEY_SQLite_PrimaryKey = { FMTID_SQLiteProperties, 110 };
    const PROPERTYKEY PKEY_SQLite_NotNull = { FMTID_SQLiteProperties, 111 };
    const PROPERTYKEY PKEY_SQLite_DefaultValue = { FMTID_SQLiteProperties, 112 };
}

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            g_hModule = hModule;
            DisableThreadLibraryCalls(hModule);
            SQLITEVIEW_LOG(L"DllMain: DLL_PROCESS_ATTACH");
            break;
        case DLL_PROCESS_DETACH:
            SQLITEVIEW_LOG(L"DllMain: DLL_PROCESS_DETACH");
            SQLiteView::DatabasePool::Instance().Clear();
            break;
    }
    return TRUE;
}

// COM exports
extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    wchar_t clsidStr[64];
    StringFromGUID2(rclsid, clsidStr, 64);
    SQLITEVIEW_LOG(L"DllGetClassObject: CLSID=%s", clsidStr);

    // Check if requested CLSID is one of ours
    bool isOurs = IsEqualCLSID(rclsid, CLSID_SQLiteViewFolder) ||
                  IsEqualCLSID(rclsid, CLSID_SQLiteViewPreview) ||
                  IsEqualCLSID(rclsid, CLSID_SQLiteViewProperty) ||
                  IsEqualCLSID(rclsid, CLSID_SQLiteViewContextMenu) ||
                  IsEqualCLSID(rclsid, CLSID_SQLiteViewIcon);

    if (!isOurs) {
        SQLITEVIEW_LOG(L"  -> CLASS_E_CLASSNOTAVAILABLE (not our CLSID)");
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    SQLiteView::ClassFactory* factory = new (std::nothrow) SQLiteView::ClassFactory(rclsid);
    if (!factory) return E_OUTOFMEMORY;

    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();

    return hr;
}

extern "C" HRESULT __stdcall DllCanUnloadNow() {
    SQLITEVIEW_LOG(L"DllCanUnloadNow: refcount=%d", g_DllRefCount);
    return (g_DllRefCount == 0) ? S_OK : S_FALSE;
}

// Registration helpers - using HKEY_CURRENT_USER for per-user registration (no admin required)
struct REGISTRY_ENTRY {
    HKEY hkeyRoot;
    const wchar_t* pszKeyName;
    const wchar_t* pszValueName;
    DWORD dwType;
    const wchar_t* pszData;
    DWORD dwData;
};

static HRESULT CreateRegKeyAndSetValue(const REGISTRY_ENTRY* pEntry, const wchar_t* pszModulePath) {
    HKEY hKey = nullptr;
    LONG result = RegCreateKeyExW(pEntry->hkeyRoot, pEntry->pszKeyName, 0, nullptr,
                                   REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (result != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(result);

    if (pEntry->pszData || pEntry->dwType == REG_DWORD) {
        if (pEntry->dwType == REG_DWORD) {
            result = RegSetValueExW(hKey, pEntry->pszValueName, 0, REG_DWORD,
                                     reinterpret_cast<const BYTE*>(&pEntry->dwData), sizeof(DWORD));
        } else if (pEntry->dwType == REG_SZ || pEntry->dwType == REG_EXPAND_SZ) {
            wchar_t szValue[MAX_PATH * 2];
            if (pEntry->pszData && wcschr(pEntry->pszData, L'%') && pszModulePath)
                StringCchPrintfW(szValue, ARRAYSIZE(szValue), pEntry->pszData, pszModulePath);
            else if (pEntry->pszData)
                StringCchCopyW(szValue, ARRAYSIZE(szValue), pEntry->pszData);
            else
                szValue[0] = L'\0';

            result = RegSetValueExW(hKey, pEntry->pszValueName, 0, pEntry->dwType,
                                     reinterpret_cast<const BYTE*>(szValue),
                                     static_cast<DWORD>((wcslen(szValue) + 1) * sizeof(wchar_t)));
        }
    }

    RegCloseKey(hKey);
    return HRESULT_FROM_WIN32(result);
}

static HRESULT DeleteRegKey(HKEY hkeyRoot, const wchar_t* pszKeyName) {
    LONG result = RegDeleteTreeW(hkeyRoot, pszKeyName);
    if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    return HRESULT_FROM_WIN32(result);
}

// Supported file extensions for SQLite databases
static const wchar_t* g_SQLiteExtensions[] = { L".db", L".sqlite", L".sqlite3", L".db3" };

extern "C" HRESULT __stdcall DllRegisterServer() {
    SQLITEVIEW_LOG(L"DllRegisterServer called");
    
    wchar_t szModulePath[MAX_PATH];
    if (!GetModuleFileNameW(g_hModule, szModulePath, MAX_PATH))
        return HRESULT_FROM_WIN32(GetLastError());
    
    SQLITEVIEW_LOG(L"Module path: %s", szModulePath);
    
    // Registry entries - mirroring Windows native structure for proper file association
    const REGISTRY_ENTRY rgRegEntries[] = {
        // ============================================================================
        // ProgID: SQLiteView.Database
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database", nullptr, REG_SZ, L"SQLite Database (SQLiteView)", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database", L"FriendlyTypeName", REG_SZ, L"SQLite Database", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database\\DefaultIcon", nullptr, REG_SZ, L"%s,0", 0 },
        
        // Link ProgID to our CLSID
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database\\CLSID", nullptr, REG_SZ, SQLITEVIEW_CLSID_STR, 0 },
        
        // ============================================================================
        // Shell\Open command - Use DelegateExecute for folder browsing
        // The ExecuteFolder CLSID {11dbb47c-a525-400b-9e80-a54615a090c0} handles folder browsing
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database\\Shell", nullptr, REG_SZ, L"Open", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database\\Shell\\Open", L"MultiSelectModel", REG_SZ, L"Document", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database\\Shell\\Open\\Command", nullptr, REG_EXPAND_SZ, L"%SystemRoot%\\Explorer.exe /idlist,%I,%L", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database\\Shell\\Open\\Command", L"DelegateExecute", REG_SZ, L"{11dbb47c-a525-400b-9e80-a54615a090c0}", 0 },
        
        // ============================================================================
        // ShellEx handlers on ProgID
        // ============================================================================
        // StorageHandler - CRITICAL for browsing as folder
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database\\ShellEx\\StorageHandler", nullptr, REG_SZ, SQLITEVIEW_CLSID_STR, 0 },
        
        // ContextMenuHandler
        { HKEY_CURRENT_USER, L"Software\\Classes\\SQLiteView.Database\\ShellEx\\ContextMenuHandlers\\SQLiteView", nullptr, REG_SZ, SQLITEVIEW_CONTEXTMENU_CLSID_STR, 0 },
        
        // ============================================================================
        // Main ShellFolder CLSID registration
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_CLSID_STR, nullptr, REG_SZ, L"SQLiteView.Database", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_CLSID_STR L"\\InProcServer32", nullptr, REG_SZ, L"%s", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_CLSID_STR L"\\InProcServer32", L"ThreadingModel", REG_SZ, L"Apartment", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_CLSID_STR L"\\ProgID", nullptr, REG_SZ, L"SQLiteView.Database", 0 },
        
        // ShellFolder attributes - SFGAO_FOLDER | SFGAO_BROWSABLE | SFGAO_DROPTARGET | SFGAO_STREAM
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_CLSID_STR L"\\ShellFolder", L"Attributes", REG_DWORD, nullptr, 0x200001A0 },
        
        // ============================================================================
        // CRITICAL: Implemented Categories - "Browsable Shell Extension"
        // Category {00021490-0000-0000-C000-000000000046} tells Explorer this CLSID is browsable
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_CLSID_STR L"\\Implemented Categories", nullptr, REG_SZ, L"", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_CLSID_STR L"\\Implemented Categories\\{00021490-0000-0000-C000-000000000046}", nullptr, REG_SZ, L"", 0 },
        
        // ============================================================================
        // Preview Handler CLSID
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_PREVIEW_CLSID_STR, nullptr, REG_SZ, L"SQLiteView Preview Handler", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_PREVIEW_CLSID_STR L"\\InProcServer32", nullptr, REG_SZ, L"%s", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_PREVIEW_CLSID_STR L"\\InProcServer32", L"ThreadingModel", REG_SZ, L"Apartment", 0 },
        
        // ============================================================================
        // ContextMenu CLSID
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_CONTEXTMENU_CLSID_STR, nullptr, REG_SZ, L"SQLiteView Context Menu", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_CONTEXTMENU_CLSID_STR L"\\InProcServer32", nullptr, REG_SZ, L"%s", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_CONTEXTMENU_CLSID_STR L"\\InProcServer32", L"ThreadingModel", REG_SZ, L"Apartment", 0 },
        
        // ============================================================================
        // Property handler CLSID
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_PROPERTY_CLSID_STR, nullptr, REG_SZ, L"SQLiteView Property Handler", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_PROPERTY_CLSID_STR L"\\InProcServer32", nullptr, REG_SZ, L"%s", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_PROPERTY_CLSID_STR L"\\InProcServer32", L"ThreadingModel", REG_SZ, L"Both", 0 },
        
        // ============================================================================
        // Icon handler CLSID
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_ICON_CLSID_STR, nullptr, REG_SZ, L"SQLiteView Icon Handler", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_ICON_CLSID_STR L"\\InProcServer32", nullptr, REG_SZ, L"%s", 0 },
        { HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SQLITEVIEW_ICON_CLSID_STR L"\\InProcServer32", L"ThreadingModel", REG_SZ, L"Apartment", 0 },
        
        // ============================================================================
        // Approved shell extensions (helps with security)
        // ============================================================================
        { HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", SQLITEVIEW_CLSID_STR, REG_SZ, L"SQLiteView Shell Folder", 0 },
        { HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", SQLITEVIEW_CONTEXTMENU_CLSID_STR, REG_SZ, L"SQLiteView Context Menu", 0 },
        { HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", SQLITEVIEW_PROPERTY_CLSID_STR, REG_SZ, L"SQLiteView Property Handler", 0 },
        { HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", SQLITEVIEW_PREVIEW_CLSID_STR, REG_SZ, L"SQLiteView Preview Handler", 0 },
    };
    
    HRESULT hr = S_OK;
    
    // Register base entries
    for (const auto& entry : rgRegEntries) {
        HRESULT hrTemp = CreateRegKeyAndSetValue(&entry, szModulePath);
        if (FAILED(hrTemp)) {
            SQLITEVIEW_LOG(L"Failed to create registry key: %s", entry.pszKeyName);
            hr = hrTemp;
        }
    }
    
    // Register file extensions dynamically
    for (const auto& ext : g_SQLiteExtensions) {
        wchar_t keyBuf[256];
        
        // Default association to our ProgID
        StringCchPrintfW(keyBuf, ARRAYSIZE(keyBuf), L"Software\\Classes\\%s", ext);
        REGISTRY_ENTRY extEntry = { HKEY_CURRENT_USER, keyBuf, nullptr, REG_SZ, L"SQLiteView.Database", 0 };
        CreateRegKeyAndSetValue(&extEntry, szModulePath);
        
        // PerceivedType
        extEntry.pszValueName = L"PerceivedType";
        extEntry.pszData = L"document";
        CreateRegKeyAndSetValue(&extEntry, szModulePath);
        
        // Content Type
        extEntry.pszValueName = L"Content Type";
        extEntry.pszData = L"application/x-sqlite3";
        CreateRegKeyAndSetValue(&extEntry, szModulePath);
        
        // SubKey for our ProgID under extension
        StringCchPrintfW(keyBuf, ARRAYSIZE(keyBuf), L"Software\\Classes\\%s\\SQLiteView.Database", ext);
        REGISTRY_ENTRY subEntry = { HKEY_CURRENT_USER, keyBuf, nullptr, REG_SZ, L"", 0 };
        CreateRegKeyAndSetValue(&subEntry, szModulePath);
        
        // OpenWithProgids
        StringCchPrintfW(keyBuf, ARRAYSIZE(keyBuf), L"Software\\Classes\\%s\\OpenWithProgids", ext);
        REGISTRY_ENTRY progIdEntry = { HKEY_CURRENT_USER, keyBuf, L"SQLiteView.Database", REG_SZ, L"", 0 };
        CreateRegKeyAndSetValue(&progIdEntry, szModulePath);
        
        // SystemFileAssociations - CRITICAL for Explorer to recognize the CLSID
        StringCchPrintfW(keyBuf, ARRAYSIZE(keyBuf), L"Software\\Classes\\SystemFileAssociations\\%s\\CLSID", ext);
        REGISTRY_ENTRY sysEntry = { HKEY_CURRENT_USER, keyBuf, nullptr, REG_SZ, SQLITEVIEW_CLSID_STR, 0 };
        CreateRegKeyAndSetValue(&sysEntry, szModulePath);
    }
    
    // Notify shell of changes
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    
    SQLITEVIEW_LOG(L"DllRegisterServer completed with hr=0x%08X", hr);
    return hr;
}

extern "C" HRESULT __stdcall DllUnregisterServer() {
    SQLITEVIEW_LOG(L"DllUnregisterServer called");
    
    // Keys to delete
    const wchar_t* rgKeysToDelete[] = {
        L"Software\\Classes\\SQLiteView.Database",
        L"Software\\Classes\\CLSID\\" SQLITEVIEW_CLSID_STR L"\\Implemented Categories\\{00021490-0000-0000-C000-000000000046}",
        L"Software\\Classes\\CLSID\\" SQLITEVIEW_CLSID_STR L"\\Implemented Categories",
        L"Software\\Classes\\CLSID\\" SQLITEVIEW_CLSID_STR,
        L"Software\\Classes\\CLSID\\" SQLITEVIEW_PREVIEW_CLSID_STR,
        L"Software\\Classes\\CLSID\\" SQLITEVIEW_CONTEXTMENU_CLSID_STR,
        L"Software\\Classes\\CLSID\\" SQLITEVIEW_PROPERTY_CLSID_STR,
        L"Software\\Classes\\CLSID\\" SQLITEVIEW_ICON_CLSID_STR,
    };
    
    for (const auto& key : rgKeysToDelete)
        DeleteRegKey(HKEY_CURRENT_USER, key);
    
    // Remove file extension registrations
    for (const auto& ext : g_SQLiteExtensions) {
        wchar_t keyBuf[256];
        
        StringCchPrintfW(keyBuf, ARRAYSIZE(keyBuf), L"Software\\Classes\\%s\\SQLiteView.Database", ext);
        DeleteRegKey(HKEY_CURRENT_USER, keyBuf);
        
        StringCchPrintfW(keyBuf, ARRAYSIZE(keyBuf), L"Software\\Classes\\%s\\OpenWithProgids", ext);
        DeleteRegKey(HKEY_CURRENT_USER, keyBuf);
        
        StringCchPrintfW(keyBuf, ARRAYSIZE(keyBuf), L"Software\\Classes\\%s\\ShellEx", ext);
        DeleteRegKey(HKEY_CURRENT_USER, keyBuf);
        
        StringCchPrintfW(keyBuf, ARRAYSIZE(keyBuf), L"Software\\Classes\\SystemFileAssociations\\%s", ext);
        DeleteRegKey(HKEY_CURRENT_USER, keyBuf);
        
        // Reset extension to default (no ProgId)
        StringCchPrintfW(keyBuf, ARRAYSIZE(keyBuf), L"Software\\Classes\\%s", ext);
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, keyBuf, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, nullptr);
            RegDeleteValueW(hKey, L"PerceivedType");
            RegDeleteValueW(hKey, L"Content Type");
            RegCloseKey(hKey);
        }
    }
    
    // Remove approved extensions
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
                      0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, SQLITEVIEW_CLSID_STR);
        RegDeleteValueW(hKey, SQLITEVIEW_CONTEXTMENU_CLSID_STR);
        RegDeleteValueW(hKey, SQLITEVIEW_PROPERTY_CLSID_STR);
        RegDeleteValueW(hKey, SQLITEVIEW_PREVIEW_CLSID_STR);
        RegCloseKey(hKey);
    }
    
    // Notify shell of changes
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    
    SQLITEVIEW_LOG(L"DllUnregisterServer completed");
    return S_OK;
}

// DllInstall (for regsvr32 /i)
extern "C" HRESULT __stdcall DllInstall(BOOL bInstall, LPCWSTR pszCmdLine) {
    UNREFERENCED_PARAMETER(pszCmdLine);
    
    if (bInstall)
        return DllRegisterServer();
    else
        return DllUnregisterServer();
}
