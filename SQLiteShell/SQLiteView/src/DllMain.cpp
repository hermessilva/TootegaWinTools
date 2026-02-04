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

// Registration helpers
static HRESULT SetRegValue(HKEY hkey, const wchar_t* subkey, const wchar_t* valueName, const wchar_t* value) {
    HKEY hk = nullptr;
    LONG result = RegCreateKeyExW(hkey, subkey, 0, nullptr, 0, KEY_WRITE, nullptr, &hk, nullptr);
    if (result != ERROR_SUCCESS) return HRESULT_FROM_WIN32(result);
    
    if (value) {
        result = RegSetValueExW(hk, valueName, 0, REG_SZ, 
            reinterpret_cast<const BYTE*>(value), 
            static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
    }
    
    RegCloseKey(hk);
    return HRESULT_FROM_WIN32(result);
}

static HRESULT DeleteRegTree(HKEY hkey, const wchar_t* subkey) {
    LSTATUS result = RegDeleteTreeW(hkey, subkey);
    if (result == ERROR_FILE_NOT_FOUND) return S_OK;
    return HRESULT_FROM_WIN32(result);
}

extern "C" HRESULT __stdcall DllRegisterServer() {
    SQLITEVIEW_LOG(L"DllRegisterServer");
    
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(g_hModule, modulePath, MAX_PATH);
    
    HRESULT hr = S_OK;
    
    // Register CLSID for ShellFolder
    hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_CLSID_STR, nullptr, L"SQLiteView Database Browser");
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_CLSID_STR L"\\InProcServer32", nullptr, modulePath);
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_CLSID_STR L"\\InProcServer32", L"ThreadingModel", L"Apartment");
    
    // Register CLSID for Preview Handler
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_PREVIEW_CLSID_STR, nullptr, L"SQLiteView Preview Handler");
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_PREVIEW_CLSID_STR L"\\InProcServer32", nullptr, modulePath);
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_PREVIEW_CLSID_STR L"\\InProcServer32", L"ThreadingModel", L"Apartment");
    
    // Register CLSID for Property Handler
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_PROPERTY_CLSID_STR, nullptr, L"SQLiteView Property Handler");
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_PROPERTY_CLSID_STR L"\\InProcServer32", nullptr, modulePath);
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_PROPERTY_CLSID_STR L"\\InProcServer32", L"ThreadingModel", L"Both");
    
    // Register CLSID for Context Menu Handler
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_CONTEXTMENU_CLSID_STR, nullptr, L"SQLiteView Context Menu");
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_CONTEXTMENU_CLSID_STR L"\\InProcServer32", nullptr, modulePath);
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_CONTEXTMENU_CLSID_STR L"\\InProcServer32", L"ThreadingModel", L"Apartment");
    
    // Register CLSID for Icon Handler
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_ICON_CLSID_STR, nullptr, L"SQLiteView Icon Handler");
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_ICON_CLSID_STR L"\\InProcServer32", nullptr, modulePath);
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_ICON_CLSID_STR L"\\InProcServer32", L"ThreadingModel", L"Apartment");
    
    // Register file extensions: .db, .sqlite, .sqlite3, .db3
    const wchar_t* extensions[] = { L".db", L".sqlite", L".sqlite3", L".db3" };
    for (const auto& ext : extensions) {
        std::wstring key = ext;
        if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, key.c_str(), nullptr, SQLITEVIEW_PROGID);
        
        // Shell folder handler
        key = std::wstring(ext) + L"\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}";
        if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, key.c_str(), nullptr, SQLITEVIEW_CLSID_STR);
        
        // Preview handler
        key = std::wstring(ext) + L"\\ShellEx\\{8895b1c6-b41f-4c1c-a562-0d564250836f}";
        if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, key.c_str(), nullptr, SQLITEVIEW_PREVIEW_CLSID_STR);
        
        // Property handler
        key = std::wstring(ext) + L"\\ShellEx\\{00021500-0000-0000-C000-000000000046}";
        if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, key.c_str(), nullptr, SQLITEVIEW_PROPERTY_CLSID_STR);
        
        // Context menu handler
        key = std::wstring(ext) + L"\\ShellEx\\ContextMenuHandlers\\SQLiteView";
        if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, key.c_str(), nullptr, SQLITEVIEW_CONTEXTMENU_CLSID_STR);
        
        // Icon handler
        key = std::wstring(ext) + L"\\ShellEx\\IconHandler";
        if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, key.c_str(), nullptr, SQLITEVIEW_ICON_CLSID_STR);
    }
    
    // Register ProgID
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, SQLITEVIEW_PROGID, nullptr, L"SQLite Database");
    if (SUCCEEDED(hr)) hr = SetRegValue(HKEY_CLASSES_ROOT, SQLITEVIEW_PROGID L"\\CLSID", nullptr, SQLITEVIEW_CLSID_STR);
    
    // Notify shell of changes
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    
    return hr;
}

extern "C" HRESULT __stdcall DllUnregisterServer() {
    SQLITEVIEW_LOG(L"DllUnregisterServer");
    
    // Remove CLSID registrations
    DeleteRegTree(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_PREVIEW_CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_PROPERTY_CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_CONTEXTMENU_CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, L"CLSID\\" SQLITEVIEW_ICON_CLSID_STR);
    
    // Remove file extension registrations
    const wchar_t* extensions[] = { L".db", L".sqlite", L".sqlite3", L".db3" };
    for (const auto& ext : extensions) {
        DeleteRegTree(HKEY_CLASSES_ROOT, (std::wstring(ext) + L"\\ShellEx").c_str());
    }
    
    // Remove ProgID
    DeleteRegTree(HKEY_CLASSES_ROOT, SQLITEVIEW_PROGID);
    
    // Notify shell of changes
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    
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
