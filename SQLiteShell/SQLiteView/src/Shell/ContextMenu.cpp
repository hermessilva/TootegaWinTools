/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** Context Menu Handler Implementation
*/

#include "ContextMenu.h"
#include <shlobj.h>
#include <commdlg.h>

namespace SQLiteView {

ContextMenuHandler::ContextMenuHandler()
    : _RefCount(1)
    , _FirstCmdID(0)
    , _ItemType(ItemType::Unknown)
    , _Site(nullptr)
    , _FolderPIDL(nullptr) {
}

ContextMenuHandler::~ContextMenuHandler() {
    if (_FolderPIDL) {
        CoTaskMemFree(_FolderPIDL);
        _FolderPIDL = nullptr;
    }
}

STDMETHODIMP ContextMenuHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown)) {
        *ppv = static_cast<IContextMenu3*>(this);
    }
    else if (IsEqualIID(riid, IID_IContextMenu) ||
             IsEqualIID(riid, IID_IContextMenu2) ||
             IsEqualIID(riid, IID_IContextMenu3)) {
        *ppv = static_cast<IContextMenu3*>(this);
    }
    else if (IsEqualIID(riid, IID_IShellExtInit)) {
        *ppv = static_cast<IShellExtInit*>(this);
    }
    else if (IsEqualIID(riid, IID_IObjectWithSite)) {
        *ppv = static_cast<IObjectWithSite*>(this);
    }
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) ContextMenuHandler::AddRef() {
    return InterlockedIncrement(&_RefCount);
}

STDMETHODIMP_(ULONG) ContextMenuHandler::Release() {
    LONG count = InterlockedDecrement(&_RefCount);
    if (count == 0) delete this;
    return count;
}

// IContextMenu
STDMETHODIMP ContextMenuHandler::QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst,
    UINT idCmdLast, UINT uFlags) {
    
    _FirstCmdID = idCmdFirst;
    UINT cmdID = idCmdFirst;
    
    // For CMF_DEFAULTONLY (double-click), provide "Open" as default command
    // This enables navigation for tables
    if (uFlags & CMF_DEFAULTONLY) {
        // If this is a table/folder, add Open command for navigation
        if (_ItemType == ItemType::Table || _ItemType == ItemType::View || _ItemType == ItemType::SystemTable) {
            SQLITEVIEW_LOG(L"QueryContextMenu: CMF_DEFAULTONLY - adding Open for table '%s'", _ItemName.c_str());
            InsertMenuW(hmenu, indexMenu, MF_BYPOSITION | MF_STRING, idCmdFirst + CMD_OPEN, L"Open");
            SetMenuDefaultItem(hmenu, idCmdFirst + CMD_OPEN, FALSE);
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, CMD_MAX);
        }
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    }
    
    // Create submenu
    HMENU hSubMenu = CreatePopupMenu();
    
    if (_TableName.empty()) {
        // Database-level context menu
        InsertMenuW(hSubMenu, 0, MF_BYPOSITION | MF_STRING, cmdID + CMD_EXPORT_CSV, L"Export All to CSV...");
        InsertMenuW(hSubMenu, 1, MF_BYPOSITION | MF_STRING, cmdID + CMD_EXPORT_SQL, L"Export Schema to SQL...");
        InsertMenuW(hSubMenu, 2, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        InsertMenuW(hSubMenu, 3, MF_BYPOSITION | MF_STRING, cmdID + CMD_VIEW_SCHEMA, L"View Schema");
        InsertMenuW(hSubMenu, 4, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        InsertMenuW(hSubMenu, 5, MF_BYPOSITION | MF_STRING, cmdID + CMD_INTEGRITY_CHECK, L"Integrity Check");
        InsertMenuW(hSubMenu, 6, MF_BYPOSITION | MF_STRING, cmdID + CMD_ANALYZE, L"Analyze Database");
        InsertMenuW(hSubMenu, 7, MF_BYPOSITION | MF_STRING, cmdID + CMD_VACUUM, L"Vacuum (Optimize)");
    } else if (_SelectedRowIDs.empty()) {
        // Table-level context menu
        InsertMenuW(hSubMenu, 0, MF_BYPOSITION | MF_STRING, cmdID + CMD_EXPORT_CSV, L"Export Table to CSV...");
        InsertMenuW(hSubMenu, 1, MF_BYPOSITION | MF_STRING, cmdID + CMD_EXPORT_JSON, L"Export Table to JSON...");
        InsertMenuW(hSubMenu, 2, MF_BYPOSITION | MF_STRING, cmdID + CMD_EXPORT_SQL, L"Export Table to SQL...");
        InsertMenuW(hSubMenu, 3, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        InsertMenuW(hSubMenu, 4, MF_BYPOSITION | MF_STRING, cmdID + CMD_VIEW_SCHEMA, L"View Table Schema");
    } else {
        // Record-level context menu
        InsertMenuW(hSubMenu, 0, MF_BYPOSITION | MF_STRING, cmdID + CMD_COPY_RECORD, L"Copy as CSV");
        InsertMenuW(hSubMenu, 1, MF_BYPOSITION | MF_STRING, cmdID + CMD_EXPORT_JSON, L"Copy as JSON");
        InsertMenuW(hSubMenu, 2, MF_BYPOSITION | MF_STRING, cmdID + CMD_EXPORT_SQL, L"Copy as INSERT SQL");
    }
    
    // Insert submenu
    MENUITEMINFOW mii = {};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_SUBMENU | MIIM_STRING | MIIM_ID;
    mii.wID = cmdID + CMD_OPEN;
    mii.hSubMenu = hSubMenu;
    mii.dwTypeData = const_cast<LPWSTR>(L"SQLite Database");
    
    InsertMenuItemW(hmenu, indexMenu, TRUE, &mii);
    
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, CMD_MAX);
}

STDMETHODIMP ContextMenuHandler::InvokeCommand(CMINVOKECOMMANDINFO* pici) {
    if (!pici) return E_POINTER;
    
    // Check if command is specified by offset or verb string
    if (HIWORD(pici->lpVerb) != 0) {
        // String verb - check for "open"
        const char* verb = pici->lpVerb;
        SQLITEVIEW_LOG(L"InvokeCommand: String verb='%S'", verb);
        if (_stricmp(verb, "open") == 0) {
            // For tables, navigate
            if (_ItemType == ItemType::Table || _ItemType == ItemType::View || _ItemType == ItemType::SystemTable) {
                return NavigateToTable(pici->hwnd) ? S_OK : E_FAIL;
            }
        }
        return E_INVALIDARG;
    }
    
    UINT cmd = LOWORD(pici->lpVerb);
    SQLITEVIEW_LOG(L"InvokeCommand: cmd=%u itemType=%d", cmd, (int)_ItemType);
    
    switch (cmd) {
        case CMD_OPEN:
            // For tables, navigate into them
            if (_ItemType == ItemType::Table || _ItemType == ItemType::View || _ItemType == ItemType::SystemTable) {
                SQLITEVIEW_LOG(L"InvokeCommand: CMD_OPEN for table '%s'", _ItemName.c_str());
                return NavigateToTable(pici->hwnd) ? S_OK : E_FAIL;
            }
            break;
        case CMD_EXPORT_CSV:
            DoExportCSV(pici->hwnd);
            break;
        case CMD_EXPORT_JSON:
            DoExportJSON(pici->hwnd);
            break;
        case CMD_EXPORT_SQL:
            DoExportSQL(pici->hwnd);
            break;
        case CMD_COPY_RECORD:
            DoCopyRecord(pici->hwnd);
            break;
        case CMD_VIEW_SCHEMA:
            DoViewSchema(pici->hwnd);
            break;
        case CMD_VACUUM:
            DoVacuum(pici->hwnd);
            break;
        case CMD_INTEGRITY_CHECK:
            DoIntegrityCheck(pici->hwnd);
            break;
        case CMD_ANALYZE:
            DoAnalyze(pici->hwnd);
            break;
        case CMD_PROPERTIES:
            DoProperties(pici->hwnd);
            break;
        default:
            return E_INVALIDARG;
    }
    
    return S_OK;
}

STDMETHODIMP ContextMenuHandler::GetCommandString(UINT_PTR idCmd, UINT uType, UINT* pReserved,
    LPSTR pszName, UINT cchMax) {
    
    if (uType != GCS_HELPTEXT && uType != GCS_HELPTEXTW)
        return E_NOTIMPL;
    
    const wchar_t* helpText = nullptr;
    
    switch (idCmd) {
        case CMD_EXPORT_CSV: helpText = L"Export data to CSV file"; break;
        case CMD_EXPORT_JSON: helpText = L"Export data to JSON file"; break;
        case CMD_EXPORT_SQL: helpText = L"Export data as SQL statements"; break;
        case CMD_COPY_RECORD: helpText = L"Copy record data to clipboard"; break;
        case CMD_VIEW_SCHEMA: helpText = L"View database schema"; break;
        case CMD_VACUUM: helpText = L"Optimize database file size"; break;
        case CMD_INTEGRITY_CHECK: helpText = L"Check database integrity"; break;
        case CMD_ANALYZE: helpText = L"Analyze database statistics"; break;
        default: return E_INVALIDARG;
    }
    
    if (uType == GCS_HELPTEXTW) {
        StringCchCopyW(reinterpret_cast<LPWSTR>(pszName), cchMax, helpText);
    } else {
        StringCchCopyA(pszName, cchMax, WideToUtf8(helpText).c_str());
    }
    
    return S_OK;
}

// IContextMenu2
STDMETHODIMP ContextMenuHandler::HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return S_OK;
}

// IContextMenu3
STDMETHODIMP ContextMenuHandler::HandleMenuMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) {
    if (plResult) *plResult = 0;
    return S_OK;
}

// IShellExtInit
STDMETHODIMP ContextMenuHandler::Initialize(PCIDLIST_ABSOLUTE pidlFolder, IDataObject* pdtobj, HKEY hkeyProgID) {
    if (!pdtobj) return E_INVALIDARG;
    
    // Get file path from data object
    FORMATETC fe = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stm = {};
    
    if (SUCCEEDED(pdtobj->GetData(&fe, &stm))) {
        HDROP hDrop = static_cast<HDROP>(GlobalLock(stm.hGlobal));
        if (hDrop) {
            wchar_t path[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, path, MAX_PATH) > 0) {
                _FilePath = path;
                _Database = DatabasePool::Instance().GetDatabase(_FilePath);
            }
            GlobalUnlock(stm.hGlobal);
        }
        ReleaseStgMedium(&stm);
    }
    
    return S_OK;
}

// IObjectWithSite
STDMETHODIMP ContextMenuHandler::SetSite(IUnknown* pUnkSite) {
    SQLITEVIEW_LOG(L"ContextMenuHandler::SetSite called with site=%p", pUnkSite);
    _Site = pUnkSite;  // Weak reference - do not AddRef (prevent circular ref)
    return S_OK;
}

STDMETHODIMP ContextMenuHandler::GetSite(REFIID riid, void** ppvSite) {
    if (!ppvSite) return E_POINTER;
    if (!_Site) {
        *ppvSite = nullptr;
        return E_FAIL;
    }
    return _Site->QueryInterface(riid, ppvSite);
}

void ContextMenuHandler::DoExportCSV(HWND hwnd) {
    if (!_Database) return;
    
    // Show save dialog
    wchar_t fileName[MAX_PATH] = L"export.csv";
    
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"csv";
    
    if (!GetSaveFileNameW(&ofn)) return;
    
    if (_TableName.empty()) {
        // Export all tables
        auto tables = _Database->GetTables(false);
        for (const auto& table : tables) {
            std::wstring tablePath = std::wstring(fileName);
            size_t dotPos = tablePath.rfind(L'.');
            if (dotPos != std::wstring::npos) {
                tablePath = tablePath.substr(0, dotPos) + L"_" + table.Name + L".csv";
            }
            _Database->ExportTableToCSV(table.Name, tablePath);
        }
    } else {
        // Export single table
        _Database->ExportTableToCSV(_TableName, fileName);
    }
    
    MessageBoxW(hwnd, L"Export completed successfully.", L"SQLite Export", MB_OK | MB_ICONINFORMATION);
}

void ContextMenuHandler::DoExportJSON(HWND hwnd) {
    if (!_Database || _TableName.empty()) return;
    
    // For records, copy to clipboard
    if (!_SelectedRowIDs.empty()) {
        std::wstring json = L"[\n";
        bool first = true;
        
        for (INT64 rowid : _SelectedRowIDs) {
            std::wstring recordJson;
            if (_Database->ExportRecordToJSON(_TableName, rowid, recordJson)) {
                if (!first) json += L",\n";
                first = false;
                json += recordJson;
            }
        }
        
        json += L"\n]";
        
        // Copy to clipboard
        if (OpenClipboard(hwnd)) {
            EmptyClipboard();
            
            SIZE_T size = (json.length() + 1) * sizeof(wchar_t);
            HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
            if (hGlobal) {
                void* pData = GlobalLock(hGlobal);
                if (pData) {
                    memcpy(pData, json.c_str(), size);
                    GlobalUnlock(hGlobal);
                    SetClipboardData(CF_UNICODETEXT, hGlobal);
                }
            }
            
            CloseClipboard();
        }
        
        MessageBoxW(hwnd, L"JSON copied to clipboard.", L"SQLite Export", MB_OK | MB_ICONINFORMATION);
    }
}

void ContextMenuHandler::DoExportSQL(HWND hwnd) {
    if (!_Database) return;
    
    std::wstring sql;
    
    if (_TableName.empty()) {
        // Export schema
        auto tables = _Database->GetTables(false);
        for (const auto& table : tables) {
            sql += _Database->GetCreateStatement(table.Name) + L";\n\n";
        }
        
        auto views = _Database->GetViews();
        for (const auto& view : views) {
            sql += _Database->GetCreateStatement(view.Name) + L";\n\n";
        }
    } else if (!_SelectedRowIDs.empty()) {
        // Export selected records as INSERT statements
        auto columns = _Database->GetColumns(_TableName);
        
        for (INT64 rowid : _SelectedRowIDs) {
            DatabaseEntry entry = _Database->GetRecordByRowID(_TableName, rowid);
            
            sql += L"INSERT INTO \"" + _TableName + L"\" (";
            
            bool first = true;
            for (const auto& col : columns) {
                if (!first) sql += L", ";
                first = false;
                sql += L"\"" + col.Name + L"\"";
            }
            
            sql += L") VALUES (";
            
            first = true;
            for (const auto& col : columns) {
                if (!first) sql += L", ";
                first = false;
                
                auto it = entry.RecordData.find(col.Name);
                if (it != entry.RecordData.end() && it->second != L"NULL") {
                    std::wstring value = it->second;
                    size_t pos = 0;
                    while ((pos = value.find(L'\'', pos)) != std::wstring::npos) {
                        value.replace(pos, 1, L"''");
                        pos += 2;
                    }
                    sql += L"'" + value + L"'";
                } else {
                    sql += L"NULL";
                }
            }
            
            sql += L");\n";
        }
    }
    
    // Copy to clipboard
    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        
        SIZE_T size = (sql.length() + 1) * sizeof(wchar_t);
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
        if (hGlobal) {
            void* pData = GlobalLock(hGlobal);
            if (pData) {
                memcpy(pData, sql.c_str(), size);
                GlobalUnlock(hGlobal);
                SetClipboardData(CF_UNICODETEXT, hGlobal);
            }
        }
        
        CloseClipboard();
    }
    
    MessageBoxW(hwnd, L"SQL copied to clipboard.", L"SQLite Export", MB_OK | MB_ICONINFORMATION);
}

void ContextMenuHandler::DoCopyRecord(HWND hwnd) {
    if (!_Database || _TableName.empty() || _SelectedRowIDs.empty()) return;
    
    auto columns = _Database->GetColumns(_TableName);
    std::wstring csv;
    
    // Header
    for (size_t i = 0; i < columns.size(); i++) {
        if (i > 0) csv += L",";
        csv += L"\"" + columns[i].Name + L"\"";
    }
    csv += L"\n";
    
    // Data
    for (INT64 rowid : _SelectedRowIDs) {
        DatabaseEntry entry = _Database->GetRecordByRowID(_TableName, rowid);
        
        for (size_t i = 0; i < columns.size(); i++) {
            if (i > 0) csv += L",";
            
            auto it = entry.RecordData.find(columns[i].Name);
            if (it != entry.RecordData.end()) {
                std::wstring value = it->second;
                size_t pos = 0;
                while ((pos = value.find(L'"', pos)) != std::wstring::npos) {
                    value.replace(pos, 1, L"\"\"");
                    pos += 2;
                }
                csv += L"\"" + value + L"\"";
            }
        }
        csv += L"\n";
    }
    
    // Copy to clipboard
    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        
        SIZE_T size = (csv.length() + 1) * sizeof(wchar_t);
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
        if (hGlobal) {
            void* pData = GlobalLock(hGlobal);
            if (pData) {
                memcpy(pData, csv.c_str(), size);
                GlobalUnlock(hGlobal);
                SetClipboardData(CF_UNICODETEXT, hGlobal);
            }
        }
        
        CloseClipboard();
    }
}

void ContextMenuHandler::DoViewSchema(HWND hwnd) {
    if (!_Database) return;
    
    std::wstring schema;
    
    if (_TableName.empty()) {
        // Show all schemas
        auto tables = _Database->GetTables(true);
        for (const auto& table : tables) {
            schema += _Database->GetCreateStatement(table.Name) + L";\n\n";
        }
        
        auto views = _Database->GetViews();
        for (const auto& view : views) {
            schema += _Database->GetCreateStatement(view.Name) + L";\n\n";
        }
    } else {
        schema = _Database->GetCreateStatement(_TableName);
    }
    
    // Show in message box (for now - could open a dialog)
    MessageBoxW(hwnd, schema.c_str(), L"Database Schema", MB_OK);
}

void ContextMenuHandler::DoVacuum(HWND hwnd) {
    MessageBoxW(hwnd, 
                L"VACUUM operation requires write access.\nThe database is currently open in read-only mode.",
                L"SQLite Vacuum", 
                MB_OK | MB_ICONINFORMATION);
}

void ContextMenuHandler::DoIntegrityCheck(HWND hwnd) {
    if (!_Database) return;
    
    std::vector<std::wstring> colNames;
    std::vector<std::vector<std::wstring>> rows;
    
    if (_Database->ExecuteQuery(L"PRAGMA integrity_check", colNames, rows, 100)) {
        std::wstring result;
        for (const auto& row : rows) {
            if (!row.empty()) {
                result += row[0] + L"\n";
            }
        }
        
        MessageBoxW(hwnd, result.c_str(), L"Integrity Check Result", MB_OK | MB_ICONINFORMATION);
    }
}

void ContextMenuHandler::DoAnalyze(HWND hwnd) {
    MessageBoxW(hwnd,
                L"ANALYZE operation requires write access.\nThe database is currently open in read-only mode.",
                L"SQLite Analyze",
                MB_OK | MB_ICONINFORMATION);
}

void ContextMenuHandler::DoProperties(HWND hwnd) {
    // Open properties dialog
    if (_FilePath.empty()) return;
    
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_INVOKEIDLIST;
    sei.hwnd = hwnd;
    sei.lpVerb = L"properties";
    sei.lpFile = _FilePath.c_str();
    sei.nShow = SW_SHOW;
    
    ShellExecuteExW(&sei);
}

bool ContextMenuHandler::NavigateToTable(HWND /*hwnd*/) {
    SQLITEVIEW_LOG(L"NavigateToTable: START - table='%s' FolderPIDL=%p Site=%p", 
        _ItemName.c_str(), _FolderPIDL, _Site);
    
    if (!_FolderPIDL) {
        SQLITEVIEW_LOG(L"NavigateToTable: FAIL - no folder PIDL");
        return false;
    }
    
    if (_ItemName.empty()) {
        SQLITEVIEW_LOG(L"NavigateToTable: FAIL - no item name");
        return false;
    }
    
    // Create a PIDL for the table item (same structure as ShellFolder::CreateItemID)
    size_t itemSize = sizeof(ItemData);
    size_t totalSize = itemSize + sizeof(USHORT); // Terminator
    
    PITEMID_CHILD itemPidl = static_cast<PITEMID_CHILD>(CoTaskMemAlloc(totalSize));
    if (!itemPidl) {
        SQLITEVIEW_LOG(L"NavigateToTable: FAIL - CoTaskMemAlloc failed");
        return false;
    }
    
    ZeroMemory(itemPidl, totalSize);
    
    ItemData* data = reinterpret_cast<ItemData*>(&itemPidl->mkid);
    data->cb = static_cast<USHORT>(itemSize);
    data->signature = ItemData::SIGNATURE;
    data->type = _ItemType;
    data->rowid = 0;
    data->recordCount = 0;
    data->columnCount = 0;
    GetSystemTimeAsFileTime(&data->modifiedTime);
    
    StringCchCopyW(data->name, ARRAYSIZE(data->name), _ItemName.c_str());
    StringCchCopyW(data->path, ARRAYSIZE(data->path), _ItemName.c_str());
    
    // Method 1: Use IShellBrowser::BrowseObject for in-place navigation (preferred)
    if (_Site) {
        IShellBrowser* pShellBrowser = nullptr;
        
        // Try to get IShellBrowser via IServiceProvider
        IServiceProvider* pServiceProvider = nullptr;
        if (SUCCEEDED(_Site->QueryInterface(IID_IServiceProvider, (void**)&pServiceProvider))) {
            pServiceProvider->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&pShellBrowser);
            pServiceProvider->Release();
        }
        
        // Fallback: try direct QueryInterface
        if (!pShellBrowser) {
            _Site->QueryInterface(IID_IShellBrowser, (void**)&pShellBrowser);
        }
        
        if (pShellBrowser) {
            SQLITEVIEW_LOG(L"NavigateToTable: Using IShellBrowser::BrowseObject with relative PIDL");
            
            // BrowseObject with relative PIDL navigates into the subfolder
            HRESULT hr = pShellBrowser->BrowseObject(itemPidl, SBSP_RELATIVE | SBSP_SAMEBROWSER);
            pShellBrowser->Release();
            CoTaskMemFree(itemPidl);
            
            if (SUCCEEDED(hr)) {
                SQLITEVIEW_LOG(L"NavigateToTable: BrowseObject SUCCESS");
                return true;
            }
            SQLITEVIEW_LOG(L"NavigateToTable: BrowseObject FAILED hr=0x%08X", hr);
        }
    }
    
    // Method 2: Fallback to SHOpenFolderAndSelectItems
    SQLITEVIEW_LOG(L"NavigateToTable: Falling back to SHOpenFolderAndSelectItems");
    
    // Combine with folder PIDL to get absolute PIDL
    PIDLIST_ABSOLUTE targetPidl = ILCombine(_FolderPIDL, itemPidl);
    CoTaskMemFree(itemPidl);
    
    if (!targetPidl) {
        SQLITEVIEW_LOG(L"NavigateToTable: FAIL - ILCombine failed");
        return false;
    }
    
    HRESULT hr = SHOpenFolderAndSelectItems(targetPidl, 0, nullptr, 0);
    CoTaskMemFree(targetPidl);
    
    if (FAILED(hr)) {
        SQLITEVIEW_LOG(L"NavigateToTable: SHOpenFolderAndSelectItems FAILED hr=0x%08X", hr);
        return false;
    }
    
    SQLITEVIEW_LOG(L"NavigateToTable: SUCCESS (via SHOpenFolderAndSelectItems)");
    return true;
}

} // namespace SQLiteView
