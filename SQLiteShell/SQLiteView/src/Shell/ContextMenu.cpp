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
    , _FirstCmdID(0) {
}

ContextMenuHandler::~ContextMenuHandler() {
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
    
    if (uFlags & CMF_DEFAULTONLY) return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    
    _FirstCmdID = idCmdFirst;
    UINT cmdID = idCmdFirst;
    
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
        // String verb - not supported
        return E_INVALIDARG;
    }
    
    UINT cmd = LOWORD(pici->lpVerb);
    
    switch (cmd) {
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

} // namespace SQLiteView
