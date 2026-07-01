/*
** SQLLocalDBView - Windows Explorer Shell Extension
** DLL Entry Point and Registration
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. All rights reserved.
** Licensed under MIT License
**
** Uses HKEY_CURRENT_USER\Software\Classes for per-user registration without admin.
*/

#include "Common.h"
#include "ShellFolder.h"
#include "PreviewHandler.h"
#include "ContextMenu.h"
#include "PropertyHandler.h"
#include "IconHandler.h"

// Definir CLSIDs - instancias reais
// {3F1A9C2E-7B4D-4E6A-9C11-2A5E8D3F7B10}
const GUID CLSID_SQLLocalDBViewFolder =
    { 0x3f1a9c2e, 0x7b4d, 0x4e6a, { 0x9c, 0x11, 0x2a, 0x5e, 0x8d, 0x3f, 0x7b, 0x10 } };

// {3F1A9C2E-7B4D-4E6A-9C11-2A5E8D3F7B11}
const GUID CLSID_SQLLocalDBViewPreview =
    { 0x3f1a9c2e, 0x7b4d, 0x4e6a, { 0x9c, 0x11, 0x2a, 0x5e, 0x8d, 0x3f, 0x7b, 0x11 } };

// {3F1A9C2E-7B4D-4E6A-9C11-2A5E8D3F7B12}
const GUID CLSID_SQLLocalDBViewProperty =
    { 0x3f1a9c2e, 0x7b4d, 0x4e6a, { 0x9c, 0x11, 0x2a, 0x5e, 0x8d, 0x3f, 0x7b, 0x12 } };

// {3F1A9C2E-7B4D-4E6A-9C11-2A5E8D3F7B13}
const GUID CLSID_SQLLocalDBViewContextMenu =
    { 0x3f1a9c2e, 0x7b4d, 0x4e6a, { 0x9c, 0x11, 0x2a, 0x5e, 0x8d, 0x3f, 0x7b, 0x13 } };

// Definir os PKEYs customizados
// {3F1A9C2E-7B4D-4E6A-9C11-2A5E8D3F7B20}
const PROPERTYKEY PKEY_SQLLocalDB_TableCount = { { 0x3f1a9c2e, 0x7b4d, 0x4e6a, { 0x9c, 0x11, 0x2a, 0x5e, 0x8d, 0x3f, 0x7b, 0x20 } }, 1 };
const PROPERTYKEY PKEY_SQLLocalDB_ViewCount = { { 0x3f1a9c2e, 0x7b4d, 0x4e6a, { 0x9c, 0x11, 0x2a, 0x5e, 0x8d, 0x3f, 0x7b, 0x20 } }, 2 };
const PROPERTYKEY PKEY_SQLLocalDB_IndexCount = { { 0x3f1a9c2e, 0x7b4d, 0x4e6a, { 0x9c, 0x11, 0x2a, 0x5e, 0x8d, 0x3f, 0x7b, 0x20 } }, 3 };
const PROPERTYKEY PKEY_SQLLocalDB_PageSize = { { 0x3f1a9c2e, 0x7b4d, 0x4e6a, { 0x9c, 0x11, 0x2a, 0x5e, 0x8d, 0x3f, 0x7b, 0x20 } }, 4 };
const PROPERTYKEY PKEY_SQLLocalDB_Encoding = { { 0x3f1a9c2e, 0x7b4d, 0x4e6a, { 0x9c, 0x11, 0x2a, 0x5e, 0x8d, 0x3f, 0x7b, 0x20 } }, 5 };

// Global variables
HMODULE g_hModule = nullptr;
LONG g_DllRefCount = 0;

// Supported file extensions - arquivos de banco SQL Server LocalDB
const wchar_t* g_SupportedExtensions[] = {
    L".mdf"
};

// Processo elevado (admin)? Instaladores rodam elevados e devem registrar
// machine-wide (HKLM); regsvr32 comum (dev, sem admin) registra por-usuario (HKCU).
static bool IsProcessElevated() {
    bool elevated = false;
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION te;
        DWORD sz = sizeof(te);
        if (GetTokenInformation(hToken, TokenElevation, &te, sizeof(te), &sz))
            elevated = te.TokenIsElevated != 0;
        CloseHandle(hToken);
    }
    return elevated;
}

// Raiz de registro: HKLM (todos os usuarios) se elevado, senao HKCU (por-usuario).
// Ambos os caminhos usam a subarvore "Software\Classes\...".
static HKEY RegistrationRoot() {
    return IsProcessElevated() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            g_hModule = hModule;
            DisableThreadLibraryCalls(hModule);
            SQLLOCALDB_LOG(L"DLL_PROCESS_ATTACH - SQLLocalDBView.dll loaded in process");
            break;

        case DLL_PROCESS_DETACH:
            SQLLOCALDB_LOG(L"DLL_PROCESS_DETACH - SQLLocalDBView.dll unloading");
            SQLLocalDBView::DatabasePool::Instance().Clear();
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
    SQLLOCALDB_LOG(L"DllGetClassObject CLSID=%s riid=%s", clsidStr, riidStr);

    if (!IsEqualCLSID(rclsid, CLSID_SQLLocalDBViewFolder) &&
        !IsEqualCLSID(rclsid, CLSID_SQLLocalDBViewPreview) &&
        !IsEqualCLSID(rclsid, CLSID_SQLLocalDBViewContextMenu) &&
        !IsEqualCLSID(rclsid, CLSID_SQLLocalDBViewProperty)) {
        SQLLOCALDB_LOG(L"  -> CLASS_E_CLASSNOTAVAILABLE");
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    SQLLocalDBView::ClassFactory* factory = new (std::nothrow) SQLLocalDBView::ClassFactory(rclsid);
    if (!factory) return E_OUTOFMEMORY;

    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();

    SQLLOCALDB_LOG(L"  -> DllGetClassObject returning 0x%08X", hr);
    return hr;
}

// ============================================================================
// Registry helper functions
// ============================================================================

struct REGISTRY_ENTRY {
    HKEY    hkeyRoot;
    LPCWSTR pszKeyName;
    LPCWSTR pszValueName;   // NULL for default value
    DWORD   dwType;
    LPCWSTR pszData;        // For REG_SZ
    DWORD   dwData;         // For REG_DWORD
};

static HRESULT CreateRegKeyAndSetValue(const REGISTRY_ENTRY* pEntry, LPCWSTR pszModulePath) {
    HKEY hKey = nullptr;

    wchar_t fullKeyPath[512];
    StringCchPrintfW(fullKeyPath, 512, L"%s\\%s",
        (pEntry->hkeyRoot == HKEY_CURRENT_USER) ? L"HKCU" :
        (pEntry->hkeyRoot == HKEY_LOCAL_MACHINE) ? L"HKLM" : L"HKCR",
        pEntry->pszKeyName);

    SQLLOCALDB_LOG(L"  Creating key: %s", fullKeyPath);

    LONG result = RegCreateKeyExW(
        pEntry->hkeyRoot, pEntry->pszKeyName, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);

    if (result != ERROR_SUCCESS) {
        SQLLOCALDB_LOG(L"    FAILED RegCreateKeyExW: error=%d (0x%08X)", result, result);
        return HRESULT_FROM_WIN32(result);
    }

    if (pEntry->dwType == REG_SZ && pEntry->pszData) {
        wchar_t szData[MAX_PATH * 2];
        if (wcsstr(pEntry->pszData, L"%s") != nullptr) {
            StringCchPrintfW(szData, ARRAYSIZE(szData), pEntry->pszData, pszModulePath);
        } else {
            StringCchCopyW(szData, ARRAYSIZE(szData), pEntry->pszData);
        }

        SQLLOCALDB_LOG(L"    Setting value '%s' = '%s'",
            pEntry->pszValueName ? pEntry->pszValueName : L"(Default)", szData);

        result = RegSetValueExW(hKey, pEntry->pszValueName, 0, REG_SZ,
            (const BYTE*)szData, (DWORD)(wcslen(szData) + 1) * sizeof(wchar_t));
    }
    else if (pEntry->dwType == REG_SZ && pEntry->pszData == nullptr) {
        SQLLOCALDB_LOG(L"    Key created (no value)");
        result = ERROR_SUCCESS;
    }
    else if (pEntry->dwType == REG_EXPAND_SZ && pEntry->pszData) {
        wchar_t szData[MAX_PATH * 2];
        StringCchCopyW(szData, ARRAYSIZE(szData), pEntry->pszData);

        result = RegSetValueExW(hKey, pEntry->pszValueName, 0, REG_EXPAND_SZ,
            (const BYTE*)szData, (DWORD)(wcslen(szData) + 1) * sizeof(wchar_t));
    }
    else if (pEntry->dwType == REG_DWORD) {
        result = RegSetValueExW(hKey, pEntry->pszValueName, 0, REG_DWORD,
            (const BYTE*)&pEntry->dwData, sizeof(DWORD));
    }

    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        SQLLOCALDB_LOG(L"    FAILED RegSetValueExW: error=%d (0x%08X)", result, result);
        return HRESULT_FROM_WIN32(result);
    }

    SQLLOCALDB_LOG(L"    SUCCESS");
    return S_OK;
}

static HRESULT DeleteRegistryKeyRecursive(HKEY hKeyRoot, LPCWSTR pszKeyName) {
    LONG result = RegDeleteTreeW(hKeyRoot, pszKeyName);
    if (result == ERROR_FILE_NOT_FOUND) return S_OK;
    return HRESULT_FROM_WIN32(result);
}

// ============================================================================
// DllRegisterServer
// ============================================================================
STDAPI DllRegisterServer() {
    SQLLOCALDB_LOG(L"========================================");
    SQLLOCALDB_LOG(L"DllRegisterServer called");
    SQLLOCALDB_LOG(L"========================================");

    wchar_t modulePath[MAX_PATH];
    if (!GetModuleFileNameW(g_hModule, modulePath, MAX_PATH)) {
        DWORD err = GetLastError();
        SQLLOCALDB_LOG(L"FATAL: GetModuleFileName failed, error=%d", err);
        return HRESULT_FROM_WIN32(err);
    }

    SQLLOCALDB_LOG(L"Module path: %s", modulePath);

    // HKLM se elevado (instalador), senao HKCU (regsvr32 por-usuario).
    HKEY root = RegistrationRoot();
    SQLLOCALDB_LOG(L"Registration root: %s", (root == HKEY_LOCAL_MACHINE) ? L"HKLM" : L"HKCU");

    // Atributos SFGAO iguais ao CompressedFolder (zipfldr.dll): navegavel como pasta.
    DWORD dwFolderAttributes = 0x200001a0;

    wchar_t szFolderCLSID[64], szPreviewCLSID[64], szContextMenuCLSID[64], szPropertyCLSID[64];
    StringFromGUID2(CLSID_SQLLocalDBViewFolder, szFolderCLSID, ARRAYSIZE(szFolderCLSID));
    StringFromGUID2(CLSID_SQLLocalDBViewPreview, szPreviewCLSID, ARRAYSIZE(szPreviewCLSID));
    StringFromGUID2(CLSID_SQLLocalDBViewContextMenu, szContextMenuCLSID, ARRAYSIZE(szContextMenuCLSID));
    StringFromGUID2(CLSID_SQLLocalDBViewProperty, szPropertyCLSID, ARRAYSIZE(szPropertyCLSID));

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
    StringCchPrintfW(szFolderCategoryKey, 256, L"Software\\Classes\\CLSID\\%s\\Implemented Categories\\{00021490-0000-0000-C000-000000000046}", szFolderCLSID);

    StringCchPrintfW(szPreviewKey, 256, L"Software\\Classes\\CLSID\\%s", szPreviewCLSID);
    StringCchPrintfW(szPreviewInProcKey, 256, L"Software\\Classes\\CLSID\\%s\\InProcServer32", szPreviewCLSID);

    StringCchPrintfW(szContextMenuKey, 256, L"Software\\Classes\\CLSID\\%s", szContextMenuCLSID);
    StringCchPrintfW(szContextMenuInProcKey, 256, L"Software\\Classes\\CLSID\\%s\\InProcServer32", szContextMenuCLSID);

    StringCchPrintfW(szPropertyKey, 256, L"Software\\Classes\\CLSID\\%s", szPropertyCLSID);
    StringCchPrintfW(szPropertyInProcKey, 256, L"Software\\Classes\\CLSID\\%s\\InProcServer32", szPropertyCLSID);

    wchar_t szIconPath[MAX_PATH + 10];
    StringCchPrintfW(szIconPath, ARRAYSIZE(szIconPath), L"%s,0", modulePath);

    REGISTRY_ENTRY rgRegEntries[] = {
        // Shell Folder CLSID
        { root, szFolderKey, nullptr, REG_SZ, L"SQLLocalDBView Database Browser", 0 },
        { root, szFolderInProcKey, nullptr, REG_SZ, L"%s", 0 },
        { root, szFolderInProcKey, L"ThreadingModel", REG_SZ, L"Apartment", 0 },
        { root, szFolderShellFolderKey, L"Attributes", REG_DWORD, nullptr, dwFolderAttributes },
        { root, szFolderShellFolderKey, L"UseDropHandler", REG_SZ, L"", 0 },
        { root, szFolderDefaultIconKey, nullptr, REG_SZ, szIconPath, 0 },
        { root, szFolderProgIdKey, nullptr, REG_SZ, L"SQLLocalDBView.Database", 0 },
        { root, szFolderCategoryKey, nullptr, REG_SZ, nullptr, 0 },

        // Preview Handler CLSID
        { root, szPreviewKey, nullptr, REG_SZ, L"SQLLocalDBView Preview Handler", 0 },
        { root, szPreviewInProcKey, nullptr, REG_SZ, L"%s", 0 },
        { root, szPreviewInProcKey, L"ThreadingModel", REG_SZ, L"Apartment", 0 },
        { root, szPreviewKey, L"AppId", REG_SZ, L"{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}", 0 },

        // Context Menu Handler CLSID
        { root, szContextMenuKey, nullptr, REG_SZ, L"SQLLocalDBView Context Menu", 0 },
        { root, szContextMenuInProcKey, nullptr, REG_SZ, L"%s", 0 },
        { root, szContextMenuInProcKey, L"ThreadingModel", REG_SZ, L"Apartment", 0 },

        // Property Handler CLSID
        { root, szPropertyKey, nullptr, REG_SZ, L"SQLLocalDBView Property Handler", 0 },
        { root, szPropertyInProcKey, nullptr, REG_SZ, L"%s", 0 },
        { root, szPropertyInProcKey, L"ThreadingModel", REG_SZ, L"Both", 0 },

        // ProgId registration
        { root, L"Software\\Classes\\SQLLocalDBView.Database", nullptr, REG_SZ, L"SQL Server LocalDB Database", 0 },
        { root, L"Software\\Classes\\SQLLocalDBView.Database", L"FriendlyTypeName", REG_SZ, L"SQL Server LocalDB Database", 0 },
        { root, L"Software\\Classes\\SQLLocalDBView.Database\\CLSID", nullptr, REG_SZ, szFolderCLSID, 0 },
        { root, L"Software\\Classes\\SQLLocalDBView.Database\\DefaultIcon", nullptr, REG_SZ, szIconPath, 0 },
        { root, L"Software\\Classes\\SQLLocalDBView.Database\\Shell\\Open", nullptr, REG_SZ, L"Open", 0 },
        { root, L"Software\\Classes\\SQLLocalDBView.Database\\Shell\\Open\\Command", nullptr, REG_EXPAND_SZ, L"%SystemRoot%\\Explorer.exe /idlist,%I,%L", 0 },
        { root, L"Software\\Classes\\SQLLocalDBView.Database\\Shell\\Open\\Command", L"DelegateExecute", REG_SZ, L"{11dbb47c-a525-400b-9e80-a54615a090c0}", 0 },
    };

    SQLLOCALDB_LOG(L"--- Registering CLSID and ProgId entries ---");

    HRESULT hr = S_OK;
    for (int i = 0; SUCCEEDED(hr) && (i < ARRAYSIZE(rgRegEntries)); i++) {
        hr = CreateRegKeyAndSetValue(&rgRegEntries[i], modulePath);
        if (FAILED(hr)) {
            SQLLOCALDB_LOG(L"FATAL: Registration failed at entry %d, hr=0x%08X", i, hr);
            return hr;
        }
    }

    // ShellEx handlers no ProgId
    SQLLOCALDB_LOG(L"--- Registering ShellEx handlers on ProgId ---");

    wchar_t szProgIdShellEx[256];

    StringCchPrintfW(szProgIdShellEx, 256, L"Software\\Classes\\SQLLocalDBView.Database\\ShellEx\\{8895b1c6-b41f-4c1c-a562-0d564250836f}");
    REGISTRY_ENTRY previewEntry = { root, szProgIdShellEx, nullptr, REG_SZ, szPreviewCLSID, 0 };
    hr = CreateRegKeyAndSetValue(&previewEntry, modulePath);
    if (FAILED(hr)) return hr;

    StringCchPrintfW(szProgIdShellEx, 256, L"Software\\Classes\\SQLLocalDBView.Database\\ShellEx\\ContextMenuHandlers\\SQLLocalDBView");
    REGISTRY_ENTRY ctxEntry = { root, szProgIdShellEx, nullptr, REG_SZ, szContextMenuCLSID, 0 };
    hr = CreateRegKeyAndSetValue(&ctxEntry, modulePath);
    if (FAILED(hr)) return hr;

    StringCchPrintfW(szProgIdShellEx, 256, L"Software\\Classes\\SQLLocalDBView.Database\\ShellEx\\{D5CDD505-2E9C-101B-9397-08002B2CF9AE}");
    REGISTRY_ENTRY propEntry = { root, szProgIdShellEx, nullptr, REG_SZ, szPropertyCLSID, 0 };
    hr = CreateRegKeyAndSetValue(&propEntry, modulePath);
    if (FAILED(hr)) return hr;

    // Associacoes de arquivo
    SQLLOCALDB_LOG(L"--- Registering file associations ---");

    for (const wchar_t* ext : g_SupportedExtensions) {
        SQLLOCALDB_LOG(L"Processing extension: %s", ext);

        wchar_t szExtKey[64], szExtShellExKey[256];
        StringCchPrintfW(szExtKey, 64, L"Software\\Classes\\%s", ext);

        REGISTRY_ENTRY extEntries[] = {
            { root, szExtKey, nullptr, REG_SZ, L"SQLLocalDBView.Database", 0 },
            { root, szExtKey, L"Content Type", REG_SZ, L"application/x-sqlserver-mdf", 0 },
            { root, szExtKey, L"PerceivedType", REG_SZ, L"compressed", 0 },
        };

        for (int i = 0; SUCCEEDED(hr) && (i < ARRAYSIZE(extEntries)); i++) {
            hr = CreateRegKeyAndSetValue(&extEntries[i], modulePath);
            if (FAILED(hr)) return hr;
        }

        wchar_t szExtProgIdKey[128];
        StringCchPrintfW(szExtProgIdKey, 128, L"Software\\Classes\\%s\\SQLLocalDBView.Database", ext);
        REGISTRY_ENTRY extProgIdKeyEntry = { root, szExtProgIdKey, nullptr, REG_SZ, nullptr, 0 };
        hr = CreateRegKeyAndSetValue(&extProgIdKeyEntry, modulePath);
        if (FAILED(hr)) return hr;

        wchar_t szOpenWithKey[128];
        StringCchPrintfW(szOpenWithKey, 128, L"Software\\Classes\\%s\\OpenWithProgids", ext);
        REGISTRY_ENTRY openWithEntry = { root, szOpenWithKey, L"SQLLocalDBView.Database", REG_SZ, L"", 0 };
        hr = CreateRegKeyAndSetValue(&openWithEntry, modulePath);
        if (FAILED(hr)) return hr;

        StringCchPrintfW(szExtShellExKey, 256, L"Software\\Classes\\%s\\ShellEx\\{8895b1c6-b41f-4c1c-a562-0d564250836f}", ext);
        REGISTRY_ENTRY extPreviewEntry = { root, szExtShellExKey, nullptr, REG_SZ, szPreviewCLSID, 0 };
        hr = CreateRegKeyAndSetValue(&extPreviewEntry, modulePath);
        if (FAILED(hr)) return hr;

        StringCchPrintfW(szExtShellExKey, 256, L"Software\\Classes\\%s\\ShellEx\\ContextMenuHandlers\\SQLLocalDBView", ext);
        REGISTRY_ENTRY extCtxEntry = { root, szExtShellExKey, nullptr, REG_SZ, szContextMenuCLSID, 0 };
        hr = CreateRegKeyAndSetValue(&extCtxEntry, modulePath);
        if (FAILED(hr)) return hr;

        StringCchPrintfW(szExtShellExKey, 256, L"Software\\Classes\\%s\\ShellEx\\{D5CDD505-2E9C-101B-9397-08002B2CF9AE}", ext);
        REGISTRY_ENTRY extPropEntry = { root, szExtShellExKey, nullptr, REG_SZ, szPropertyCLSID, 0 };
        hr = CreateRegKeyAndSetValue(&extPropEntry, modulePath);
        if (FAILED(hr)) return hr;
    }

    // Registros HKLM (opcionais, podem falhar sem admin)
    SQLLOCALDB_LOG(L"--- Attempting HKLM registrations (optional) ---");

    {
        HKEY hKey;
        LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
            0, KEY_WRITE, &hKey);
        if (result == ERROR_SUCCESS) {
            RegSetValueExW(hKey, szPreviewCLSID, 0, REG_SZ,
                (const BYTE*)L"SQLLocalDBView Preview Handler",
                (DWORD)(wcslen(L"SQLLocalDBView Preview Handler") + 1) * sizeof(wchar_t));
            RegCloseKey(hKey);
            SQLLOCALDB_LOG(L"  PreviewHandlers list: SUCCESS");
        } else {
            SQLLOCALDB_LOG(L"  PreviewHandlers list: SKIPPED (not admin) error=%d", result);
        }
    }

    {
        HKEY hKey;
        LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
            0, KEY_WRITE, &hKey);
        if (result == ERROR_SUCCESS) {
            RegSetValueExW(hKey, szContextMenuCLSID, 0, REG_SZ,
                (const BYTE*)L"SQLLocalDBView Context Menu Handler",
                (DWORD)(wcslen(L"SQLLocalDBView Context Menu Handler") + 1) * sizeof(wchar_t));
            RegSetValueExW(hKey, szFolderCLSID, 0, REG_SZ,
                (const BYTE*)L"SQLLocalDBView Shell Folder",
                (DWORD)(wcslen(L"SQLLocalDBView Shell Folder") + 1) * sizeof(wchar_t));
            RegSetValueExW(hKey, szPropertyCLSID, 0, REG_SZ,
                (const BYTE*)L"SQLLocalDBView Property Handler",
                (DWORD)(wcslen(L"SQLLocalDBView Property Handler") + 1) * sizeof(wchar_t));
            RegCloseKey(hKey);
            SQLLOCALDB_LOG(L"  Shell Extensions Approved: SUCCESS");
        } else {
            SQLLOCALDB_LOG(L"  Shell Extensions Approved: SKIPPED (not admin) error=%d", result);
        }
    }

    SQLLOCALDB_LOG(L"Notifying Shell of changes...");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    SQLLOCALDB_LOG(L"DllRegisterServer: COMPLETED SUCCESSFULLY");
    return S_OK;
}

// ============================================================================
// DllUnregisterServer
// ============================================================================
STDAPI DllUnregisterServer() {
    SQLLOCALDB_LOG(L"DllUnregisterServer called");

    wchar_t szFolderCLSID[64], szPreviewCLSID[64], szContextMenuCLSID[64], szPropertyCLSID[64];
    StringFromGUID2(CLSID_SQLLocalDBViewFolder, szFolderCLSID, ARRAYSIZE(szFolderCLSID));
    StringFromGUID2(CLSID_SQLLocalDBViewPreview, szPreviewCLSID, ARRAYSIZE(szPreviewCLSID));
    StringFromGUID2(CLSID_SQLLocalDBViewContextMenu, szContextMenuCLSID, ARRAYSIZE(szContextMenuCLSID));
    StringFromGUID2(CLSID_SQLLocalDBViewProperty, szPropertyCLSID, ARRAYSIZE(szPropertyCLSID));

    wchar_t key[256];

    // Limpar em AMBOS os hives (por-usuario e machine-wide), pois o registro pode
    // ter sido feito por regsvr32 (HKCU) ou pelo instalador elevado (HKLM).
    HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    for (HKEY r : roots) {
        StringCchPrintfW(key, 256, L"Software\\Classes\\CLSID\\%s", szFolderCLSID);
        DeleteRegistryKeyRecursive(r, key);
        StringCchPrintfW(key, 256, L"Software\\Classes\\CLSID\\%s", szPreviewCLSID);
        DeleteRegistryKeyRecursive(r, key);
        StringCchPrintfW(key, 256, L"Software\\Classes\\CLSID\\%s", szContextMenuCLSID);
        DeleteRegistryKeyRecursive(r, key);
        StringCchPrintfW(key, 256, L"Software\\Classes\\CLSID\\%s", szPropertyCLSID);
        DeleteRegistryKeyRecursive(r, key);

        DeleteRegistryKeyRecursive(r, L"Software\\Classes\\SQLLocalDBView.Database");

        for (const wchar_t* ext : g_SupportedExtensions) {
            StringCchPrintfW(key, 256, L"Software\\Classes\\%s\\ShellEx", ext);
            DeleteRegistryKeyRecursive(r, key);

            HKEY hKey;
            StringCchPrintfW(key, 256, L"Software\\Classes\\%s", ext);
            if (RegOpenKeyExW(r, key, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
                wchar_t value[256] = {0};
                DWORD size = sizeof(value);
                if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)value, &size) == ERROR_SUCCESS) {
                    if (wcscmp(value, L"SQLLocalDBView.Database") == 0)
                        RegDeleteValueW(hKey, nullptr);
                }
                RegDeleteValueW(hKey, L"Content Type");
                RegDeleteValueW(hKey, L"PerceivedType");
                RegCloseKey(hKey);
            }
        }
    }

    // Fallback HKEY_CLASSES_ROOT (instalacoes antigas).
    StringCchPrintfW(key, 256, L"CLSID\\%s", szFolderCLSID);
    DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, key);
    StringCchPrintfW(key, 256, L"CLSID\\%s", szPreviewCLSID);
    DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, key);
    StringCchPrintfW(key, 256, L"CLSID\\%s", szContextMenuCLSID);
    DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, key);
    StringCchPrintfW(key, 256, L"CLSID\\%s", szPropertyCLSID);
    DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, key);
    DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, L"SQLLocalDBView.Database");
    for (const wchar_t* ext : g_SupportedExtensions) {
        StringCchPrintfW(key, 256, L"%s\\ShellEx", ext);
        DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, key);
    }

    {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
            0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, szPreviewCLSID);
            RegCloseKey(hKey);
        }
    }

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

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    SQLLOCALDB_LOG(L"DllUnregisterServer: COMPLETED");
    return S_OK;
}

// DllInstall - Per-user registration
STDAPI DllInstall(BOOL bInstall, LPCWSTR pszCmdLine) {
    SQLLOCALDB_LOG(L"DllInstall called: bInstall=%d, cmdLine=%s",
        bInstall, pszCmdLine ? pszCmdLine : L"(null)");
    return bInstall ? DllRegisterServer() : DllUnregisterServer();
}
