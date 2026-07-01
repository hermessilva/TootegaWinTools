/*
** SQLLocalDBView - Windows Explorer Shell Extension
** Context Menu Handler Implementation
*/

#include "ContextMenu.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <commdlg.h>

namespace SQLLocalDBView {

// ============================================================================
// ContextMenuHandler Implementation
// ============================================================================

ContextMenuHandler::ContextMenuHandler() : refCount_(1), cmdFirst_(0), pidlItem_(nullptr) {
}

ContextMenuHandler::~ContextMenuHandler() {
    if (pidlItem_) {
        CoTaskMemFree(pidlItem_);
        pidlItem_ = nullptr;
    }
}

// IObjectWithSite
STDMETHODIMP ContextMenuHandler::SetSite(IUnknown* pUnkSite) {
    site_ = pUnkSite;
    SQLLOCALDB_LOG(L"ContextMenuHandler::SetSite called, site=%p", pUnkSite);
    return S_OK;
}

STDMETHODIMP ContextMenuHandler::GetSite(REFIID riid, void** ppvSite) {
    if (!ppvSite) return E_POINTER;
    if (site_) {
        return site_->QueryInterface(riid, ppvSite);
    }
    *ppvSite = nullptr;
    return E_FAIL;
}

void ContextMenuHandler::SetItemPidl(PCUITEMID_CHILD pidl) {
    if (pidlItem_) {
        CoTaskMemFree(pidlItem_);
        pidlItem_ = nullptr;
    }
    if (pidl) {
        pidlItem_ = ILClone(pidl);
    }
}

STDMETHODIMP ContextMenuHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_IContextMenu) ||
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
    return InterlockedIncrement(&refCount_);
}

STDMETHODIMP_(ULONG) ContextMenuHandler::Release() {
    LONG count = InterlockedDecrement(&refCount_);
    if (count == 0) {
        delete this;
    }
    return count;
}

STDMETHODIMP ContextMenuHandler::Initialize(PCIDLIST_ABSOLUTE pidlFolder, 
    IDataObject* pdtobj, HKEY hkeyProgID) {
    
    SQLLOCALDB_LOG(L"ContextMenuHandler::Initialize called");
    
    // Se temos IDataObject, extrair arquivo
    if (pdtobj) {
        FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stg = {};
        
        if (SUCCEEDED(pdtobj->GetData(&fmt, &stg))) {
            HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
            if (hDrop) {
                wchar_t path[MAX_PATH];
                if (DragQueryFileW(hDrop, 0, path, MAX_PATH)) {
                    dbPath_ = path;
                    SQLLOCALDB_LOG(L"ContextMenu: File path = %s", path);
                }
                GlobalUnlock(stg.hGlobal);
            }
            ReleaseStgMedium(&stg);
        }
    }
    
    // Fallback: extrair de pidlFolder
    if (dbPath_.empty() && pidlFolder) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidlFolder, path)) {
            dbPath_ = path;
            SQLLOCALDB_LOG(L"ContextMenu: Folder path = %s", path);
        }
    }
    
    return S_OK;
}

// Constante para comando Open (navegação)
#define CMD_OPEN 100

STDMETHODIMP ContextMenuHandler::QueryContextMenu(HMENU hmenu, UINT indexMenu, 
    UINT idCmdFirst, UINT idCmdLast, UINT uFlags) {
    
    SQLLOCALDB_LOG(L"QueryContextMenu: uFlags=0x%X, idCmdFirst=%u, tableName='%s'", 
        uFlags, idCmdFirst, tableName_.c_str());
    
    cmdFirst_ = idCmdFirst;
    
    // Tabelas/Views sao "pastas" virtuais: o Explorer NAO entra sozinho nelas.
    // Fornecemos o verbo "Open" (default) que navega via IShellBrowser::BrowseObject
    // (ver InvokeCommand/CMD_OPEN). Igual ao SQLiteShell, que navega corretamente.
    bool isFolder = (itemType_ == ItemType::Table || itemType_ == ItemType::View);

    // Duplo-clique / verbo default.
    if (uFlags & CMF_DEFAULTONLY) {
        if (isFolder) {
            InsertMenuW(hmenu, indexMenu, MF_BYPOSITION | MF_STRING, idCmdFirst + CMD_OPEN, L"Open");
            SetMenuDefaultItem(hmenu, idCmdFirst + CMD_OPEN, FALSE);
            SQLLOCALDB_LOG(L"QueryContextMenu: CMF_DEFAULTONLY - Open added for '%s'", tableName_.c_str());
        }
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, CMD_OPEN + 1);
    }

    // Menu completo (clique-direito): "Open" como item default para pastas.
    UINT insertPos = indexMenu;
    if (isFolder) {
        InsertMenuW(hmenu, insertPos++, MF_BYPOSITION | MF_STRING, idCmdFirst + CMD_OPEN, L"&Open");
        SetMenuDefaultItem(hmenu, idCmdFirst + CMD_OPEN, FALSE);
    }

    // Criar submenu SQLLocalDBView
    HMENU hSubmenu = CreatePopupMenu();
    
    // Comandos de cópia
    InsertMenuW(hSubmenu, 0, MF_BYPOSITION | MF_STRING, 
        idCmdFirst + CMD_COPY_CREATE_TABLE, L"Copy CREATE TABLE...");
    InsertMenuW(hSubmenu, 1, MF_BYPOSITION | MF_STRING, 
        idCmdFirst + CMD_COPY_AS_CSV, L"Copy as CSV");
    InsertMenuW(hSubmenu, 2, MF_BYPOSITION | MF_STRING, 
        idCmdFirst + CMD_COPY_AS_JSON, L"Copy as JSON");
    InsertMenuW(hSubmenu, 3, MF_BYPOSITION | MF_STRING, 
        idCmdFirst + CMD_COPY_AS_INSERT, L"Copy as INSERT statements");
    
    InsertMenuW(hSubmenu, 4, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    
    // Comandos de exportação
    InsertMenuW(hSubmenu, 5, MF_BYPOSITION | MF_STRING, 
        idCmdFirst + CMD_EXPORT_CSV, L"Export to CSV...");
    InsertMenuW(hSubmenu, 6, MF_BYPOSITION | MF_STRING, 
        idCmdFirst + CMD_EXPORT_JSON, L"Export to JSON...");
    InsertMenuW(hSubmenu, 7, MF_BYPOSITION | MF_STRING, 
        idCmdFirst + CMD_EXPORT_SQL, L"Export to SQL...");
    
    InsertMenuW(hSubmenu, 8, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    
    // Comandos de visualização
    InsertMenuW(hSubmenu, 9, MF_BYPOSITION | MF_STRING, 
        idCmdFirst + CMD_VIEW_SCHEMA, L"View Schema...");
    InsertMenuW(hSubmenu, 10, MF_BYPOSITION | MF_STRING, 
        idCmdFirst + CMD_VIEW_INDEXES, L"View Indexes...");
    
    // Menu principal
    MENUITEMINFOW mii = {};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_SUBMENU | MIIM_STRING | MIIM_ID;
    mii.wID = idCmdFirst + CMD_LAST;
    mii.hSubMenu = hSubmenu;
    mii.dwTypeData = (LPWSTR)L"SQLLocalDBView";

    InsertMenuItemW(hmenu, insertPos, TRUE, &mii);

    // Retornar o maior ID usado (CMD_OPEN=100)
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, CMD_OPEN + 1);
}

STDMETHODIMP ContextMenuHandler::InvokeCommand(CMINVOKECOMMANDINFO* pici) {
    if (!pici) return E_POINTER;
    
    // Verificar se é por offset ou por verbo
    UINT cmd;
    if (HIWORD(pici->lpVerb)) {
        // Verbo de string - verificar se é "open"
        const char* verb = pici->lpVerb;
        if (_stricmp(verb, "open") == 0) {
            cmd = CMD_OPEN;
        } else {
            SQLLOCALDB_LOG(L"InvokeCommand: Unknown verb '%S'", verb);
            return E_INVALIDARG;
        }
    } else {
        cmd = LOWORD(pici->lpVerb);
    }
    
    SQLLOCALDB_LOG(L"InvokeCommand: cmd=%u, itemType=%d, dbPath='%s', tableName='%s'", 
        cmd, (int)itemType_, dbPath_.c_str(), tableName_.c_str());
    
    // CMD_OPEN (100) - Navegação para dentro da tabela
    // Só navegar se for uma tabela ou view (pastas)
    if (cmd == CMD_OPEN && (itemType_ == ItemType::Table || itemType_ == ItemType::View)) {
        SQLLOCALDB_LOG(L"InvokeCommand: CMD_OPEN - Attempting navigation, pidlItem_=%p, site_=%p", 
            pidlItem_, site_);
        
        // Método 1: Usar IShellBrowser via site_ (IObjectWithSite)
        if (site_) {
            IShellBrowser* pShellBrowser = nullptr;
            // Tentar obter via IServiceProvider
            IServiceProvider* pServiceProvider = nullptr;
            if (SUCCEEDED(site_->QueryInterface(IID_IServiceProvider, (void**)&pServiceProvider))) {
                pServiceProvider->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&pShellBrowser);
                pServiceProvider->Release();
            }
            // Fallback: tentar QueryInterface direto
            if (!pShellBrowser) {
                site_->QueryInterface(IID_IShellBrowser, (void**)&pShellBrowser);
            }
            
            if (pShellBrowser && pidlItem_) {
                SQLLOCALDB_LOG(L"InvokeCommand: CMD_OPEN - Using IShellBrowser::BrowseObject with pidlItem_");
                // BrowseObject com PIDL relativo navega para dentro do item
                HRESULT hr = pShellBrowser->BrowseObject(pidlItem_, SBSP_RELATIVE | SBSP_SAMEBROWSER);
                if (SUCCEEDED(hr)) {
                    SQLLOCALDB_LOG(L"InvokeCommand: CMD_OPEN - BrowseObject succeeded");
                    pShellBrowser->Release();
                    return S_OK;
                }
                SQLLOCALDB_LOG(L"InvokeCommand: CMD_OPEN - BrowseObject failed: 0x%08X", hr);
                pShellBrowser->Release();
            }
            if (pShellBrowser) pShellBrowser->Release();
        }
        
        // Método 2: Construir caminho virtual e usar SHParseDisplayName
        std::wstring virtualPath = dbPath_;
        if (!tableName_.empty()) {
            virtualPath += L"\\" + tableName_;
        }
        
        SQLLOCALDB_LOG(L"InvokeCommand: CMD_OPEN - Navigating to '%s'", virtualPath.c_str());
        
        PIDLIST_ABSOLUTE pidl = nullptr;
        HRESULT hr = SHParseDisplayName(virtualPath.c_str(), nullptr, &pidl, 0, nullptr);
        
        if (SUCCEEDED(hr) && pidl) {
            // Tentar navegar no mesmo browser via site
            if (site_) {
                IShellBrowser* pShellBrowser = nullptr;
                IServiceProvider* pServiceProvider = nullptr;
                if (SUCCEEDED(site_->QueryInterface(IID_IServiceProvider, (void**)&pServiceProvider))) {
                    pServiceProvider->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&pShellBrowser);
                    pServiceProvider->Release();
                }
                if (pShellBrowser) {
                    hr = pShellBrowser->BrowseObject(pidl, SBSP_ABSOLUTE | SBSP_SAMEBROWSER);
                    if (SUCCEEDED(hr)) {
                        SQLLOCALDB_LOG(L"InvokeCommand: CMD_OPEN - BrowseObject (absolute) succeeded");
                        pShellBrowser->Release();
                        CoTaskMemFree(pidl);
                        return S_OK;
                    }
                    SQLLOCALDB_LOG(L"InvokeCommand: CMD_OPEN - BrowseObject (absolute) failed: 0x%08X", hr);
                    pShellBrowser->Release();
                }
            }
            
            // Fallback: usar SHOpenFolderAndSelectItems  
            hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
            if (SUCCEEDED(hr)) {
                SQLLOCALDB_LOG(L"InvokeCommand: CMD_OPEN - SHOpenFolderAndSelectItems succeeded");
            } else {
                SQLLOCALDB_LOG(L"InvokeCommand: CMD_OPEN - SHOpenFolderAndSelectItems failed: 0x%08X", hr);
            }
            
            CoTaskMemFree(pidl);
            return hr;
        } else {
            SQLLOCALDB_LOG(L"InvokeCommand: CMD_OPEN - SHParseDisplayName failed: 0x%08X", hr);
            
            // Último fallback: ShellExecute "explore"
            HINSTANCE result = ShellExecuteW(
                pici->hwnd,
                L"explore",
                virtualPath.c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL
            );
            
            if ((INT_PTR)result > 32) {
                SQLLOCALDB_LOG(L"InvokeCommand: CMD_OPEN - ShellExecute succeeded");
                return S_OK;
            } else {
                SQLLOCALDB_LOG(L"InvokeCommand: CMD_OPEN - ShellExecute failed: %d", (int)(INT_PTR)result);
            }
        }
        
        return S_OK;
    }
    
    // Para CMD_OPEN em itens que não são pastas (Row), ignorar
    if (cmd == CMD_OPEN) {
        SQLLOCALDB_LOG(L"InvokeCommand: CMD_OPEN ignored for non-folder item type %d", (int)itemType_);
        return S_OK;
    }
    
    // Verificar se temos caminho para outros comandos
    if (dbPath_.empty()) {
        MessageBoxW(pici->hwnd, L"No database file selected", L"SQLLocalDBView Error", MB_ICONERROR);
        return E_FAIL;
    }
    
    // Abrir banco se necessário
    if (!database_) {
        database_ = DatabasePool::Instance().GetDatabase(dbPath_);
    }
    
    if (!database_ || !database_->IsOpen()) {
        std::wstring msg = L"Failed to open database:\n" + dbPath_;
        MessageBoxW(pici->hwnd, msg.c_str(), L"SQLLocalDBView Error", MB_ICONERROR);
        return E_FAIL;
    }
    
    try {
        switch (cmd) {
            case CMD_COPY_CREATE_TABLE:
                CopyCreateTable(pici->hwnd);
                break;
            case CMD_COPY_AS_CSV:
                CopyAsCSV(pici->hwnd);
                break;
            case CMD_COPY_AS_JSON:
                CopyAsJSON(pici->hwnd);
                break;
            case CMD_COPY_AS_INSERT:
                CopyAsInsert(pici->hwnd);
                break;
            case CMD_EXPORT_CSV:
                ExportToCSV(pici->hwnd);
                break;
            case CMD_EXPORT_JSON:
                ExportToJSON(pici->hwnd);
                break;
            case CMD_EXPORT_SQL:
                ExportToSQL(pici->hwnd);
                break;
            case CMD_VIEW_SCHEMA:
                ViewSchema(pici->hwnd);
                break;
            case CMD_VIEW_INDEXES:
                ViewIndexes(pici->hwnd);
                break;
            default:
                MessageBoxW(pici->hwnd, L"Unknown command", L"SQLLocalDBView", MB_ICONWARNING);
                return E_INVALIDARG;
        }
    }
    catch (const std::exception& e) {
        std::wstring msg = L"Error: " + StringUtil::Utf8ToWide(e.what());
        MessageBoxW(pici->hwnd, msg.c_str(), L"SQLLocalDBView Error", MB_ICONERROR);
        return E_FAIL;
    }
    catch (...) {
        MessageBoxW(pici->hwnd, L"Unknown error occurred", L"SQLLocalDBView Error", MB_ICONERROR);
        return E_FAIL;
    }
    
    return S_OK;
}

STDMETHODIMP ContextMenuHandler::GetCommandString(UINT_PTR idCmd, UINT uType, 
    UINT* pReserved, CHAR* pszName, UINT cchMax) {
    
    if (uType != GCS_HELPTEXT && uType != GCS_HELPTEXTW) {
        return E_NOTIMPL;
    }
    
    const wchar_t* help = nullptr;
    
    switch (idCmd) {
        case CMD_COPY_CREATE_TABLE:
            help = L"Copy the CREATE TABLE statement to clipboard";
            break;
        case CMD_COPY_AS_CSV:
            help = L"Copy table data as CSV to clipboard";
            break;
        case CMD_COPY_AS_JSON:
            help = L"Copy table data as JSON to clipboard";
            break;
        case CMD_COPY_AS_INSERT:
            help = L"Copy table data as INSERT statements";
            break;
        case CMD_EXPORT_CSV:
            help = L"Export table data to a CSV file";
            break;
        case CMD_EXPORT_JSON:
            help = L"Export table data to a JSON file";
            break;
        case CMD_EXPORT_SQL:
            help = L"Export table data to a SQL file";
            break;
        case CMD_VIEW_SCHEMA:
            help = L"View the table schema";
            break;
        case CMD_VIEW_INDEXES:
            help = L"View table indexes";
            break;
        default:
            return E_INVALIDARG;
    }
    
    if (uType == GCS_HELPTEXTW) {
        StringCchCopyW(reinterpret_cast<LPWSTR>(pszName), cchMax, help);
    } else {
        StringCchCopyA(pszName, cchMax, StringUtil::WideToUtf8(help).c_str());
    }
    
    return S_OK;
}

STDMETHODIMP ContextMenuHandler::HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return S_OK;
}

STDMETHODIMP ContextMenuHandler::HandleMenuMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, 
    LRESULT* plResult) {
    if (plResult) *plResult = 0;
    return S_OK;
}

void ContextMenuHandler::CopyToClipboard(HWND hwnd, const std::wstring& text) {
    if (!OpenClipboard(hwnd)) return;
    
    EmptyClipboard();
    
    // Unicode text
    size_t size = (text.length() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (hMem) {
        void* ptr = GlobalLock(hMem);
        if (ptr) {
            memcpy(ptr, text.c_str(), size);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    
    CloseClipboard();
}

void ContextMenuHandler::CopyCreateTable(HWND hwnd) {
    if (tableName_.empty()) return;
    
    std::wstring sql = database_->GetCreateStatement(tableName_);
    if (!sql.empty()) {
        CopyToClipboard(hwnd, sql);
    }
}

void ContextMenuHandler::CopyAsCSV(HWND hwnd) {
    std::wstring csv = GenerateCSV(1000);
    CopyToClipboard(hwnd, csv);
}

void ContextMenuHandler::CopyAsJSON(HWND hwnd) {
    std::wstring json = GenerateJSON(1000);
    CopyToClipboard(hwnd, json);
}

void ContextMenuHandler::CopyAsInsert(HWND hwnd) {
    std::wstring sql = GenerateInsertStatements(100);
    CopyToClipboard(hwnd, sql);
}

std::wstring ContextMenuHandler::GenerateCSV(int maxRows) {
    if (tableName_.empty() || !database_) return L"";
    
    QueryResult result = database_->GetTablePreview(tableName_, maxRows);
    if (!result.error.empty()) return L"";
    
    std::wostringstream oss;
    
    // Cabeçalho
    for (size_t i = 0; i < result.columnNames.size(); i++) {
        if (i > 0) oss << L",";
        oss << L"\"" << result.columnNames[i] << L"\"";
    }
    oss << L"\n";
    
    // Dados
    for (const auto& row : result.rows) {
        for (size_t i = 0; i < row.size(); i++) {
            if (i > 0) oss << L",";
            
            const auto& cell = row[i];
            if (cell.type == CellValue::Type::Null) {
                // Vazio para NULL
            }
            else if (cell.type == CellValue::Type::Text) {
                // Escapar aspas e quebras de linha
                std::wstring text = cell.textValue;
                size_t pos = 0;
                while ((pos = text.find(L"\"", pos)) != std::wstring::npos) {
                    text.replace(pos, 1, L"\"\"");
                    pos += 2;
                }
                oss << L"\"" << text << L"\"";
            }
            else {
                oss << cell.ToString();
            }
        }
        oss << L"\n";
    }
    
    return oss.str();
}

std::wstring ContextMenuHandler::GenerateJSON(int maxRows) {
    if (tableName_.empty() || !database_) return L"";
    
    QueryResult result = database_->GetTablePreview(tableName_, maxRows);
    if (!result.error.empty()) return L"";
    
    std::wostringstream oss;
    oss << L"[\n";
    
    for (size_t rowIdx = 0; rowIdx < result.rows.size(); rowIdx++) {
        const auto& row = result.rows[rowIdx];
        
        if (rowIdx > 0) oss << L",\n";
        oss << L"  {";
        
        for (size_t i = 0; i < row.size() && i < result.columnNames.size(); i++) {
            if (i > 0) oss << L", ";
            
            oss << L"\"" << result.columnNames[i] << L"\": ";
            
            const auto& cell = row[i];
            switch (cell.type) {
                case CellValue::Type::Null:
                    oss << L"null";
                    break;
                case CellValue::Type::Integer:
                    oss << cell.intValue;
                    break;
                case CellValue::Type::Real:
                    oss << std::setprecision(15) << cell.realValue;
                    break;
                case CellValue::Type::Text: {
                    // Escapar string JSON
                    std::wstring text = cell.textValue;
                    std::wostringstream escaped;
                    for (wchar_t c : text) {
                        switch (c) {
                            case L'\\': escaped << L"\\\\"; break;
                            case L'"': escaped << L"\\\""; break;
                            case L'\n': escaped << L"\\n"; break;
                            case L'\r': escaped << L"\\r"; break;
                            case L'\t': escaped << L"\\t"; break;
                            default: escaped << c; break;
                        }
                    }
                    oss << L"\"" << escaped.str() << L"\"";
                    break;
                }
                case CellValue::Type::Blob:
                    oss << L"\"(BLOB " << cell.blobValue.size() << L" bytes)\"";
                    break;
            }
        }
        
        oss << L"}";
    }
    
    oss << L"\n]";
    return oss.str();
}

std::wstring ContextMenuHandler::GenerateInsertStatements(int maxRows) {
    if (tableName_.empty() || !database_) return L"";
    
    QueryResult result = database_->GetTablePreview(tableName_, maxRows);
    if (!result.error.empty()) return L"";
    
    std::wostringstream oss;
    
    for (const auto& row : result.rows) {
        oss << L"INSERT INTO " << StringUtil::QualifyBracketed(tableName_) << L" (";

        // Nomes das colunas
        for (size_t i = 0; i < result.columnNames.size(); i++) {
            if (i > 0) oss << L", ";
            oss << StringUtil::BracketQuote(result.columnNames[i]);
        }
        
        oss << L") VALUES (";
        
        // Valores
        for (size_t i = 0; i < row.size(); i++) {
            if (i > 0) oss << L", ";
            
            const auto& cell = row[i];
            switch (cell.type) {
                case CellValue::Type::Null:
                    oss << L"NULL";
                    break;
                case CellValue::Type::Integer:
                    oss << cell.intValue;
                    break;
                case CellValue::Type::Real:
                    oss << std::setprecision(15) << cell.realValue;
                    break;
                case CellValue::Type::Text: {
                    // Escapar aspas simples
                    std::wstring text = cell.textValue;
                    size_t pos = 0;
                    while ((pos = text.find(L"'", pos)) != std::wstring::npos) {
                        text.replace(pos, 1, L"''");
                        pos += 2;
                    }
                    oss << L"'" << text << L"'";
                    break;
                }
                case CellValue::Type::Blob:
                    oss << L"0x";
                    for (uint8_t b : cell.blobValue) {
                        oss << std::hex << std::setw(2) << std::setfill(L'0') << (int)b;
                    }
                    oss << std::dec;
                    break;
            }
        }
        
        oss << L");\n";
    }
    
    return oss.str();
}

bool ContextMenuHandler::SaveToFile(HWND hwnd, const std::wstring& content, 
    const std::wstring& defaultName, const std::wstring& filter) {
    
    wchar_t filename[MAX_PATH] = {};
    StringCchCopyW(filename, MAX_PATH, defaultName.c_str());
    
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    
    if (GetSaveFileNameW(&ofn)) {
        // Converter wstring para UTF-8
        int size = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size > 0) {
            std::string utf8(size - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, &utf8[0], size, nullptr, nullptr);
            
            std::ofstream file(filename, std::ios::binary);
            if (file) {
                // BOM UTF-8
                file.write("\xEF\xBB\xBF", 3);
                file.write(utf8.c_str(), utf8.size());
                file.close();
                return true;
            }
        }
    }
    
    return false;
}

void ContextMenuHandler::ExportToCSV(HWND hwnd) {
    std::wstring csv = GenerateCSV(-1);
    std::wstring defaultName = tableName_ + L".csv";
    SaveToFile(hwnd, csv, defaultName, 
        L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0");
}

void ContextMenuHandler::ExportToJSON(HWND hwnd) {
    std::wstring json = GenerateJSON(-1);
    std::wstring defaultName = tableName_ + L".json";
    SaveToFile(hwnd, json, defaultName, 
        L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0");
}

void ContextMenuHandler::ExportToSQL(HWND hwnd) {
    std::wostringstream oss;
    
    // CREATE TABLE
    oss << L"-- Table: " << tableName_ << L"\n";
    oss << database_->GetCreateStatement(tableName_) << L";\n\n";
    
    // INSERT statements
    oss << L"-- Data\n";
    oss << GenerateInsertStatements(-1);
    
    std::wstring defaultName = tableName_ + L".sql";
    SaveToFile(hwnd, oss.str(), defaultName, 
        L"SQL Files (*.sql)\0*.sql\0All Files (*.*)\0*.*\0");
}

void ContextMenuHandler::ViewSchema(HWND hwnd) {
    if (tableName_.empty() || !database_) return;
    
    std::wstring schema = database_->GetTableSchema(tableName_);
    
    // Mostrar em dialog simples
    MessageBoxW(hwnd, schema.c_str(), 
        (L"Schema: " + tableName_).c_str(), MB_OK | MB_ICONINFORMATION);
}

void ContextMenuHandler::ViewIndexes(HWND hwnd) {
    if (tableName_.empty() || !database_) return;
    
    TableInfo info = database_->GetTableInfo(tableName_);
    
    std::wostringstream oss;
    oss << L"Indexes for table: " << tableName_ << L"\n\n";
    
    if (info.indexes.empty()) {
        oss << L"No indexes defined.";
    } else {
        for (const auto& idx : info.indexes) {
            oss << L"• " << idx.name;
            if (idx.unique) oss << L" (UNIQUE)";
            oss << L"\n  Columns: ";
            for (size_t i = 0; i < idx.columns.size(); i++) {
                if (i > 0) oss << L", ";
                oss << idx.columns[i];
            }
            oss << L"\n\n";
        }
    }
    
    MessageBoxW(hwnd, oss.str().c_str(), L"Indexes", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// DatabaseContextMenuHandler Implementation
// ============================================================================

DatabaseContextMenuHandler::DatabaseContextMenuHandler() : refCount_(1), cmdFirst_(0) {
}

DatabaseContextMenuHandler::~DatabaseContextMenuHandler() {
}

STDMETHODIMP DatabaseContextMenuHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_IContextMenu) ||
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

STDMETHODIMP_(ULONG) DatabaseContextMenuHandler::AddRef() {
    return InterlockedIncrement(&refCount_);
}

STDMETHODIMP_(ULONG) DatabaseContextMenuHandler::Release() {
    LONG count = InterlockedDecrement(&refCount_);
    if (count == 0) {
        delete this;
    }
    return count;
}

STDMETHODIMP DatabaseContextMenuHandler::Initialize(PCIDLIST_ABSOLUTE pidlFolder, 
    IDataObject* pdtobj, HKEY hkeyProgID) {
    
    // DEBUG: Show MessageBox immediately to confirm handler is being called
    // MessageBoxW(nullptr, L"DatabaseContextMenuHandler::Initialize called!", L"SQLLocalDBView DEBUG", MB_OK | MB_ICONINFORMATION);
    
    SQLLOCALDB_LOG(L"DatabaseContextMenuHandler::Initialize called");
    OutputDebugStringW(L"[SQLLocalDBView] DatabaseContextMenuHandler::Initialize called\n");
    selectedFiles_.clear();
    
    if (!pdtobj) {
        SQLLOCALDB_LOG(L"DatabaseContextMenuHandler: pdtobj is null");
        return E_INVALIDARG;
    }
    
    FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = {};
    
    HRESULT hr = pdtobj->GetData(&fmt, &stg);
    if (FAILED(hr)) {
        SQLLOCALDB_LOG(L"DatabaseContextMenuHandler: GetData failed hr=0x%08X", hr);
        return hr;
    }
    
    HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
    if (hDrop) {
        UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        SQLLOCALDB_LOG(L"DatabaseContextMenuHandler: Found %u files", count);
        for (UINT i = 0; i < count; i++) {
            wchar_t path[MAX_PATH];
            if (DragQueryFileW(hDrop, i, path, MAX_PATH)) {
                selectedFiles_.push_back(path);
                SQLLOCALDB_LOG(L"DatabaseContextMenuHandler: File[%u] = %s", i, path);
            }
        }
        GlobalUnlock(stg.hGlobal);
    }
    
    ReleaseStgMedium(&stg);
    
    return S_OK;
}

STDMETHODIMP DatabaseContextMenuHandler::QueryContextMenu(HMENU hmenu, UINT indexMenu, 
    UINT idCmdFirst, UINT idCmdLast, UINT uFlags) {
    
    if (uFlags & CMF_DEFAULTONLY) {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    }
    
    cmdFirst_ = idCmdFirst;
    
    // Menu principal para arquivos .db
    InsertMenuW(hmenu, indexMenu++, MF_BYPOSITION | MF_STRING, 
        idCmdFirst + 0, L"Browse with SQLLocalDBView");
    InsertMenuW(hmenu, indexMenu++, MF_BYPOSITION | MF_STRING, 
        idCmdFirst + 1, L"Export all tables...");
    InsertMenuW(hmenu, indexMenu++, MF_BYPOSITION | MF_STRING, 
        idCmdFirst + 2, L"Database information...");
    InsertMenuW(hmenu, indexMenu++, MF_BYPOSITION | MF_STRING, 
        idCmdFirst + 3, L"Validate database");
    
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 4);
}

STDMETHODIMP DatabaseContextMenuHandler::InvokeCommand(CMINVOKECOMMANDINFO* pici) {
    if (!pici) return E_POINTER;
    
    SQLLOCALDB_LOG(L"DatabaseContextMenuHandler::InvokeCommand called");
    
    if (selectedFiles_.empty()) {
        MessageBoxW(pici->hwnd, L"No file selected", L"SQLLocalDBView Error", MB_ICONERROR);
        return E_FAIL;
    }
    
    UINT cmd = LOWORD(pici->lpVerb);
    const std::wstring& path = selectedFiles_[0];
    
    SQLLOCALDB_LOG(L"DatabaseContextMenuHandler: cmd=%u, path=%s", cmd, path.c_str());
    
    try {
        switch (cmd) {
            case 0:
                OpenAsFolder(pici->hwnd, path);
                break;
            case 1:
                ExportAllTables(pici->hwnd, path);
                break;
            case 2:
                ShowDatabaseInfo(pici->hwnd, path);
                break;
            case 3:
                ValidateDatabase(pici->hwnd, path);
                break;
            default:
                MessageBoxW(pici->hwnd, L"Unknown command", L"SQLLocalDBView", MB_ICONWARNING);
                return E_INVALIDARG;
        }
    }
    catch (const std::exception& e) {
        std::wstring msg = L"Error: " + StringUtil::Utf8ToWide(e.what());
        MessageBoxW(pici->hwnd, msg.c_str(), L"SQLLocalDBView Error", MB_ICONERROR);
        return E_FAIL;
    }
    catch (...) {
        MessageBoxW(pici->hwnd, L"Unknown error occurred", L"SQLLocalDBView Error", MB_ICONERROR);
        return E_FAIL;
    }
    
    return S_OK;
}

STDMETHODIMP DatabaseContextMenuHandler::GetCommandString(UINT_PTR idCmd, UINT uType, 
    UINT* pReserved, CHAR* pszName, UINT cchMax) {
    return E_NOTIMPL;
}

STDMETHODIMP DatabaseContextMenuHandler::HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return S_OK;
}

STDMETHODIMP DatabaseContextMenuHandler::HandleMenuMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, 
    LRESULT* plResult) {
    if (plResult) *plResult = 0;
    return S_OK;
}

void DatabaseContextMenuHandler::OpenAsFolder(HWND hwnd, const std::wstring& path) {
    // Abrir arquivo no Explorer como pasta virtual
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.hwnd = hwnd;
    sei.lpVerb = L"explore";
    sei.lpFile = path.c_str();
    sei.nShow = SW_SHOWNORMAL;
    
    ShellExecuteExW(&sei);
}

void DatabaseContextMenuHandler::ExportAllTables(HWND hwnd, const std::wstring& path) {
    auto db = DatabasePool::Instance().GetDatabase(path);
    if (!db || !db->IsOpen()) {
        MessageBoxW(hwnd, L"Failed to open database", L"SQLLocalDBView", MB_ICONERROR);
        return;
    }
    
    // Escolher pasta de destino
    BROWSEINFOW bi = {};
    bi.hwndOwner = hwnd;
    bi.lpszTitle = L"Select folder to export tables:";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    
    wchar_t folderPath[MAX_PATH];
    SHGetPathFromIDListW(pidl, folderPath);
    CoTaskMemFree(pidl);
    
    auto tables = db->GetTables();
    int exported = 0;
    
    for (const auto& table : tables) {
        QueryResult result = db->ExecuteQuery(
            L"SELECT * FROM " + StringUtil::QualifyBracketed(table.name), -1);

        if (result.error.empty()) {
            std::wstring filePath = std::wstring(folderPath) + L"\\" + table.name + L".csv";
            
            std::wofstream file(filePath);
            if (file) {
                // Cabeçalho
                for (size_t i = 0; i < result.columnNames.size(); i++) {
                    if (i > 0) file << L",";
                    file << L"\"" << result.columnNames[i] << L"\"";
                }
                file << L"\n";
                
                // Dados
                for (const auto& row : result.rows) {
                    for (size_t i = 0; i < row.size(); i++) {
                        if (i > 0) file << L",";
                        if (row[i].type == CellValue::Type::Text) {
                            file << L"\"" << row[i].textValue << L"\"";
                        } else if (row[i].type != CellValue::Type::Null) {
                            file << row[i].ToString();
                        }
                    }
                    file << L"\n";
                }
                
                exported++;
            }
        }
    }
    
    std::wstring msg = StringUtil::Format(L"Exported %d tables to %s", exported, folderPath);
    MessageBoxW(hwnd, msg.c_str(), L"SQLLocalDBView", MB_ICONINFORMATION);
}

void DatabaseContextMenuHandler::ShowDatabaseInfo(HWND hwnd, const std::wstring& path) {
    auto db = DatabasePool::Instance().GetDatabase(path);
    if (!db || !db->IsOpen()) {
        MessageBoxW(hwnd, L"Failed to open database", L"SQLLocalDBView", MB_ICONERROR);
        return;
    }
    
    DatabaseInfo info = db->GetDatabaseInfo();
    
    std::wostringstream oss;
    oss << L"File: " << info.fileName << L"\n";
    oss << L"Database: " << info.databaseName << L"\n\n";
    oss << L"File Size: " << (info.fileSize / 1024) << L" KB\n";
    oss << L"Data Pages (8 KB): " << info.pageCount << L"\n";
    oss << L"Collation: " << info.encoding << L"\n";
    oss << L"Product Version: " << info.productVersion << L"\n";
    oss << L"Compatibility Level: " << info.compatibilityLevel << L"\n\n";
    oss << L"Tables: " << info.tables.size() << L"\n";
    oss << L"Views: " << info.views.size() << L"\n";
    oss << L"Indexes: " << info.indexes.size() << L"\n";
    
    MessageBoxW(hwnd, oss.str().c_str(), L"Database Information", MB_ICONINFORMATION);
}

void DatabaseContextMenuHandler::ValidateDatabase(HWND hwnd, const std::wstring& path) {
    auto db = DatabasePool::Instance().GetDatabase(path);
    if (!db || !db->IsOpen()) {
        MessageBoxW(hwnd, L"Failed to open database", L"SQLLocalDBView", MB_ICONERROR);
        return;
    }
    
    // SQL Server: DBCC CHECKDB. Com NO_INFOMSGS, uma base integra nao retorna
    // mensagens; corrupcao gera erros que aparecem em result.error.
    QueryResult result = db->ExecuteQuery(L"DBCC CHECKDB WITH NO_INFOMSGS", 0);

    if (result.error.empty()) {
        MessageBoxW(hwnd, L"Database integrity check passed!\n\nNo errors found.",
            L"Database Valid", MB_ICONINFORMATION);
    } else {
        MessageBoxW(hwnd, (L"Database integrity check found issues:\n\n" + result.error).c_str(),
            L"Database Issues", MB_ICONWARNING);
    }
}

} // namespace SQLLocalDBView
