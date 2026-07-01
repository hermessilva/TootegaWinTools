/*
** SQLLocalDBView - Windows Explorer Shell Extension
** Shell Folder Implementation
*/

#include "ShellFolder.h"
#include "PreviewHandler.h"
#include "ContextMenu.h"
#include "PropertyHandler.h"
#include "IconHandler.h"
#include <algorithm>

// Definições de constantes do Shell que podem estar faltando
#ifndef PID_STG_NAME
#define PID_STG_NAME 10
#endif

#ifndef PID_STG_STORAGETYPE
#define PID_STG_STORAGETYPE 4
#endif

#ifndef PID_STG_SIZE
#define PID_STG_SIZE 12
#endif

// SFVM messages (Shell Folder View Callback)
#ifndef SFVM_DEFVIEWMODE
#define SFVM_DEFVIEWMODE 27
#endif

#ifndef SFVM_WINDOWCREATED
#define SFVM_WINDOWCREATED 15
#endif

// FMTID_Storage GUID
static const GUID LOCAL_FMTID_Storage = 
    { 0xB725F130, 0x47EF, 0x101A, { 0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC } };

// Helper para preencher STRRET
static HRESULT SetStrRet(STRRET* psr, const WCHAR* psz) {
    if (!psr || !psz) return E_POINTER;
    psr->uType = STRRET_WSTR;
    return SHStrDupW(psz, &psr->pOleStr);
}

namespace SQLLocalDBView {

// ============================================================================
// ClassFactory Implementation
// ============================================================================

ClassFactory::ClassFactory(REFCLSID clsid) : refCount_(1), clsid_(clsid) {
}

ClassFactory::~ClassFactory() {
}

STDMETHODIMP ClassFactory::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }

    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ClassFactory::AddRef() {
    return InterlockedIncrement(&refCount_);
}

STDMETHODIMP_(ULONG) ClassFactory::Release() {
    LONG count = InterlockedDecrement(&refCount_);
    if (count == 0) {
        delete this;
    }
    return count;
}

STDMETHODIMP ClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (pUnkOuter) return CLASS_E_NOAGGREGATION;

    // Log interface requested
    wchar_t clsidStr[64], riidStr[64];
    StringFromGUID2(clsid_, clsidStr, 64);
    StringFromGUID2(riid, riidStr, 64);
    SQLLOCALDB_LOG(L"ClassFactory::CreateInstance CLSID=%s riid=%s", clsidStr, riidStr);

    HRESULT hr = E_OUTOFMEMORY;

    if (IsEqualCLSID(clsid_, CLSID_SQLLocalDBViewFolder)) {
        SQLLOCALDB_LOG(L"  -> Creating ShellFolder instance");
        ShellFolder* folder = new (std::nothrow) ShellFolder();
        if (folder) {
            hr = folder->QueryInterface(riid, ppv);
            folder->Release();
            SQLLOCALDB_LOG(L"  -> ShellFolder QueryInterface returned 0x%08X", hr);
        }
    }
    else if (IsEqualCLSID(clsid_, CLSID_SQLLocalDBViewPreview)) {
        SQLLOCALDB_LOG(L"  -> Creating PreviewHandler instance");
        // PreviewHandler criado aqui
        PreviewHandler* preview = new (std::nothrow) PreviewHandler();
        if (preview) {
            hr = preview->QueryInterface(riid, ppv);
            preview->Release();
        }
    }
    else if (IsEqualCLSID(clsid_, CLSID_SQLLocalDBViewContextMenu)) {
        SQLLOCALDB_LOG(L"  -> Creating ContextMenu instance");
        // Use DatabaseContextMenuHandler for .db files context menu
        DatabaseContextMenuHandler* menu = new (std::nothrow) DatabaseContextMenuHandler();
        if (menu) {
            hr = menu->QueryInterface(riid, ppv);
            menu->Release();
        }
    }
    else if (IsEqualCLSID(clsid_, CLSID_SQLLocalDBViewProperty)) {
        PropertyHandler* prop = new (std::nothrow) PropertyHandler();
        if (prop) {
            hr = prop->QueryInterface(riid, ppv);
            prop->Release();
        }
    }
    else {
        hr = CLASS_E_CLASSNOTAVAILABLE;
    }

    return hr;
}

STDMETHODIMP ClassFactory::LockServer(BOOL fLock) {
    extern LONG g_DllRefCount;
    if (fLock) {
        InterlockedIncrement(&g_DllRefCount);
    } else {
        InterlockedDecrement(&g_DllRefCount);
    }
    return S_OK;
}

// ============================================================================
// ShellFolder Implementation
// ============================================================================

ShellFolder::ShellFolder() : refCount_(1), pidlRoot_(nullptr) {
    SQLLOCALDB_LOG(L"ShellFolder created");
}

ShellFolder::~ShellFolder() {
    if (pidlRoot_) {
        CoTaskMemFree(pidlRoot_);
    }
    SQLLOCALDB_LOG(L"ShellFolder destroyed");
}

STDMETHODIMP ShellFolder::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    // Log all interface requests for debugging
    wchar_t iidStr[64];
    StringFromGUID2(riid, iidStr, 64);
    SQLLOCALDB_LOG(L"ShellFolder::QueryInterface requested: %s", iidStr);

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_IShellFolder) ||
        IsEqualIID(riid, IID_IShellFolder2)) {
        SQLLOCALDB_LOG(L"  -> Returning IShellFolder2");
        *ppv = static_cast<IShellFolder2*>(this);
    }
    else if (IsEqualIID(riid, IID_IPersistFile)) {
        // CRÍTICO: Windows chama IPersistFile::Load() para passar o caminho do arquivo!
        SQLLOCALDB_LOG(L"  -> Returning IPersistFile (CRITICAL for file-based navigation!)");
        *ppv = static_cast<IPersistFile*>(this);
    }
    else if (IsEqualIID(riid, IID_IPersist) ||
             IsEqualIID(riid, IID_IPersistFolder) ||
             IsEqualIID(riid, IID_IPersistFolder2) ||
             IsEqualIID(riid, IID_IPersistFolder3)) {
        SQLLOCALDB_LOG(L"  -> Returning IPersistFolder3");
        *ppv = static_cast<IPersistFolder3*>(this);
    }
    else if (IsEqualIID(riid, IID_IShellFolderViewCB)) {
        // Deliberadamente NAO expor via QueryInterface (interfere na navegacao).
        // O callback ainda e passado em CreateViewObject (psfvcb). Igual SQLiteShell.
        SQLLOCALDB_LOG(L"  -> IShellFolderViewCB: E_NOINTERFACE (by design)");
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    else if (IsEqualIID(riid, IID_IObjectWithSite)) {
        SQLLOCALDB_LOG(L"  -> Returning IObjectWithSite");
        *ppv = static_cast<IObjectWithSite*>(this);
    }
    else {
        SQLLOCALDB_LOG(L"  -> E_NOINTERFACE");
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) ShellFolder::AddRef() {
    return InterlockedIncrement(&refCount_);
}

STDMETHODIMP_(ULONG) ShellFolder::Release() {
    LONG count = InterlockedDecrement(&refCount_);
    if (count == 0) {
        delete this;
    }
    return count;
}

// IPersist
STDMETHODIMP ShellFolder::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    *pClassID = CLSID_SQLLocalDBViewFolder;
    return S_OK;
}

// IPersistFolder
STDMETHODIMP ShellFolder::Initialize(PCIDLIST_ABSOLUTE pidl) {
    if (pidlRoot_) {
        CoTaskMemFree(pidlRoot_);
        pidlRoot_ = nullptr;
    }

    if (pidl) {
        pidlRoot_ = ILClone(pidl);
        if (!pidlRoot_) return E_OUTOFMEMORY;
    }

    // Extrair caminho do arquivo DB do PIDL (apenas se ainda nao definido, para
    // nao apagar um path ja setado por SetDatabasePath em subpastas).
    if (dbPath_.empty()) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            dbPath_ = path;
            SQLLOCALDB_LOG(L"ShellFolder::Initialize path=%s", path);
        }
    }

    // IMPORTANTE: Verificar se o PIDL contém um item de tabela após o caminho do arquivo
    // Quando navegamos para dentro de uma tabela, o PIDL tem a forma:
    // [Desktop] -> [Drives] -> [Path to .db file] -> [Table ItemData]
    // Precisamos percorrer o PIDL para encontrar nosso ItemData de tabela
    if (pidl) {
        PCUIDLIST_RELATIVE child = pidl;
        const ItemData* tableItem = nullptr;
        int itemCount = 0;
        
        // Percorrer todos os itens do PIDL procurando por um ItemData nosso
        while (child && child->mkid.cb > 0) {
            itemCount++;
            SQLLOCALDB_LOG(L"ShellFolder::Initialize PIDL item %d: cb=%u", itemCount, child->mkid.cb);
            
            // Verificar se este item é um ItemData nosso (pela assinatura)
            if (child->mkid.cb >= sizeof(ItemData)) {
                const ItemData* item = reinterpret_cast<const ItemData*>(&child->mkid);
                SQLLOCALDB_LOG(L"ShellFolder::Initialize PIDL item %d: signature=0x%04X (expected 0x%04X)", 
                    itemCount, item->signature, ItemData::SIGNATURE);
                if (item->signature == ItemData::SIGNATURE) {
                    // Encontramos um item nosso
                    SQLLOCALDB_LOG(L"ShellFolder::Initialize found ItemData: type=%d name='%s'", 
                        (int)item->type, item->name);
                    if (item->type == ItemType::Table || item->type == ItemType::View) {
                        tableItem = item;
                        SQLLOCALDB_LOG(L"ShellFolder::Initialize found table in PIDL: '%s'", item->name);
                    }
                }
            }
            // Avançar para o próximo item do PIDL
            child = reinterpret_cast<PCUIDLIST_RELATIVE>(
                reinterpret_cast<const BYTE*>(child) + child->mkid.cb);
        }
        
        SQLLOCALDB_LOG(L"ShellFolder::Initialize processed %d PIDL items, tableItem=%p", itemCount, tableItem);
        
        // Se encontramos uma tabela, configurar para mostrar seus registros
        if (tableItem) {
            tableName_ = tableItem->name;
            SQLLOCALDB_LOG(L"ShellFolder::Initialize configured as TableView for '%s'", tableName_.c_str());
        }
    }

    return S_OK;
}

// IPersistFolder2
STDMETHODIMP ShellFolder::GetCurFolder(PIDLIST_ABSOLUTE* ppidl) {
    if (!ppidl) return E_POINTER;
    
    if (pidlRoot_) {
        *ppidl = ILClone(pidlRoot_);
        return *ppidl ? S_OK : E_OUTOFMEMORY;
    }
    
    *ppidl = nullptr;
    return S_FALSE;
}

// IPersistFolder3
STDMETHODIMP ShellFolder::InitializeEx(IBindCtx* pbc, PCIDLIST_ABSOLUTE pidlRoot,
    const PERSIST_FOLDER_TARGET_INFO* ppfti) {
    
    HRESULT hr = Initialize(pidlRoot);
    if (FAILED(hr)) return hr;

    if (ppfti && ppfti->szTargetParsingName[0]) {
        dbPath_ = ppfti->szTargetParsingName;
    }

    return S_OK;
}

STDMETHODIMP ShellFolder::GetFolderTargetInfo(PERSIST_FOLDER_TARGET_INFO* ppfti) {
    if (!ppfti) return E_POINTER;
    
    ZeroMemory(ppfti, sizeof(*ppfti));
    
    if (!dbPath_.empty()) {
        StringCchCopyW(ppfti->szTargetParsingName, MAX_PATH, dbPath_.c_str());
        ppfti->dwAttributes = FILE_ATTRIBUTE_NORMAL;
        ppfti->csidl = -1;
    }
    
    return S_OK;
}

bool ShellFolder::OpenDatabase() {
    if (database_ && database_->IsOpen()) {
        return true;
    }
    
    if (dbPath_.empty()) return false;
    
    database_ = DatabasePool::Instance().GetDatabase(dbPath_);
    return database_ && database_->IsOpen();
}

// IShellFolder
STDMETHODIMP ShellFolder::ParseDisplayName(HWND hwnd, IBindCtx* pbc, LPWSTR pszDisplayName,
    ULONG* pchEaten, PIDLIST_RELATIVE* ppidl, ULONG* pdwAttributes) {
    
    if (!ppidl) return E_POINTER;
    *ppidl = nullptr;
    
    if (!pszDisplayName || !*pszDisplayName) return E_INVALIDARG;
    
    if (!OpenDatabase()) return E_FAIL;

    // Procurar tabela com esse nome
    auto tables = database_->GetTables();
    for (const auto& table : tables) {
        if (_wcsicmp(table.name.c_str(), pszDisplayName) == 0) {
            *ppidl = CreateItemID(table);
            if (pchEaten) *pchEaten = (ULONG)wcslen(pszDisplayName);
            if (pdwAttributes) {
                *pdwAttributes &= SFGAO_STREAM | SFGAO_CANCOPY;
            }
            return S_OK;
        }
    }

    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}

STDMETHODIMP ShellFolder::EnumObjects(HWND hwnd, SHCONTF grfFlags, IEnumIDList** ppenumIDList) {
    if (!ppenumIDList) return E_POINTER;
    *ppenumIDList = nullptr;

    // Log detalhado dos flags para depuração
    SQLLOCALDB_LOG(L"EnumObjects called: hwnd=%p flags=0x%08X (FOLDERS=%d NONFOLDERS=%d) IsTableView=%d tableName='%s'",
        hwnd, grfFlags, 
        (grfFlags & SHCONTF_FOLDERS) ? 1 : 0,
        (grfFlags & SHCONTF_NONFOLDERS) ? 1 : 0,
        IsTableView() ? 1 : 0,
        tableName_.c_str());

    EnumIDList* enumList = new (std::nothrow) EnumIDList(this, grfFlags);
    if (!enumList) return E_OUTOFMEMORY;

    *ppenumIDList = enumList;
    return S_OK;
}

STDMETHODIMP ShellFolder::BindToObject(PCUIDLIST_RELATIVE pidl, IBindCtx* pbc, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    
    const ItemData* item = GetItemData(pidl);
    if (!item) return E_INVALIDARG;
    
    // Log what interface is being requested
    WCHAR guidStr[64];
    StringFromGUID2(riid, guidStr, 64);
    SQLLOCALDB_LOG(L"BindToObject: item='%s' type=%d IID=%s", item->name, (int)item->type, guidStr);
    
    // Tabelas e Views são navegáveis como subpastas. Criar o subfolder apenas para
    // as interfaces de navegação/persistência que ele realmente implementa. Isso evita
    // alocar um ShellFolder para cada IID desconhecido que o shell sonda (milhares),
    // reduzindo drasticamente o overhead.
    if (item->type == ItemType::Table || item->type == ItemType::View) {
        if (IsEqualIID(riid, IID_IShellFolder) || IsEqualIID(riid, IID_IShellFolder2) ||
            IsEqualIID(riid, IID_IPersist) || IsEqualIID(riid, IID_IPersistFolder) ||
            IsEqualIID(riid, IID_IPersistFolder2) || IsEqualIID(riid, IID_IPersistFolder3)) {

            ShellFolder* subfolder = new (std::nothrow) ShellFolder();
            if (!subfolder) return E_OUTOFMEMORY;

            // Configurar identidade ANTES do Initialize (que respeita path ja setado).
            subfolder->SetDatabasePath(dbPath_);
            subfolder->SetTableName(item->name);

            // CRITICO: dar pidlRoot_ absoluto ao subfolder. Sem ele,
            // IPersistFolder2::GetCurFolder retorna nulo e o browser cancela a
            // navegacao (a subpasta nunca chega a enumerar as linhas).
            if (pidlRoot_) {
                PIDLIST_ABSOLUTE absPidl = ILCombine(pidlRoot_, pidl);
                if (absPidl) {
                    subfolder->Initialize(absPidl);
                    CoTaskMemFree(absPidl);
                }
            }

            SQLLOCALDB_LOG(L"BindToObject: Creating subfolder for table '%s' in db '%s'",
                item->name, dbPath_.c_str());

            HRESULT hr = subfolder->QueryInterface(riid, ppv);
            subfolder->Release();
            return hr;
        }
    }

    // Support IPreviewHandler for Preview Pane
    if (IsEqualIID(riid, IID_IPreviewHandler)) {
        PreviewHandler* preview = new (std::nothrow) PreviewHandler();
        if (!preview) return E_OUTOFMEMORY;
        
        preview->SetDatabasePath(dbPath_);
        preview->SetTableName(item->name);
        
        SQLLOCALDB_LOG(L"BindToObject: Creating PreviewHandler for table '%s'", item->name);
        
        HRESULT hr = preview->QueryInterface(riid, ppv);
        preview->Release();
        return hr;
    }
    
    return E_NOTIMPL;
}

STDMETHODIMP ShellFolder::BindToStorage(PCUIDLIST_RELATIVE pidl, IBindCtx* pbc, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    
    const ItemData* item = GetItemData(pidl);
    if (!item) return E_INVALIDARG;
    
    // Log what interface is being requested
    WCHAR guidStr[64];
    StringFromGUID2(riid, guidStr, 64);
    SQLLOCALDB_LOG(L"BindToStorage: item='%s' IID=%s", item->name, guidStr);
    
    // Support IPreviewHandler via BindToStorage too
    if (IsEqualIID(riid, IID_IPreviewHandler)) {
        PreviewHandler* preview = new (std::nothrow) PreviewHandler();
        if (!preview) return E_OUTOFMEMORY;
        
        preview->SetDatabasePath(dbPath_);
        preview->SetTableName(item->name);
        
        SQLLOCALDB_LOG(L"BindToStorage: Creating PreviewHandler for table '%s'", item->name);
        
        HRESULT hr = preview->QueryInterface(riid, ppv);
        preview->Release();
        return hr;
    }
    
    return E_NOTIMPL;
}

STDMETHODIMP ShellFolder::CompareIDs(LPARAM lParam, PCUIDLIST_RELATIVE pidl1, PCUIDLIST_RELATIVE pidl2) {
    const ItemData* item1 = GetItemData(pidl1);
    const ItemData* item2 = GetItemData(pidl2);

    if (!item1 || !item2) return E_INVALIDARG;

    // Coluna de ordenação (low word de lParam)
    int column = LOWORD(lParam);
    int result = 0;

    // Dentro de uma tabela, os itens sao linhas: comparar pelo valor da coluna
    // pedida (do cache) e desempatar SEMPRE pelo ordinal, para que o shell nunca
    // trate duas linhas distintas com mesmo valor como o mesmo item.
    if (!tableName_.empty()) {
        LoadTableData();
        size_t i1 = (item1->rowCount > 0) ? (size_t)(item1->rowCount - 1) : 0;
        size_t i2 = (item2->rowCount > 0) ? (size_t)(item2->rowCount - 1) : 0;
        if ((UINT)column < tableData_.columnNames.size() &&
            i1 < tableData_.rows.size() && i2 < tableData_.rows.size() &&
            column < (int)tableData_.rows[i1].size() && column < (int)tableData_.rows[i2].size()) {
            result = _wcsicmp(tableData_.rows[i1][column].ToString().c_str(),
                              tableData_.rows[i2][column].ToString().c_str());
        }
        if (result == 0) {
            if (item1->rowCount < item2->rowCount) result = -1;
            else if (item1->rowCount > item2->rowCount) result = 1;
        }
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, (USHORT)result);
    }

    switch (column) {
        case 0: // Nome
            result = _wcsicmp(item1->name, item2->name);
            break;
        case 1: // Tipo
            result = static_cast<int>(item1->type) - static_cast<int>(item2->type);
            break;
        case 2: // Linhas
            if (item1->rowCount < item2->rowCount) result = -1;
            else if (item1->rowCount > item2->rowCount) result = 1;
            break;
        case 3: // Colunas
            result = item1->columnCount - item2->columnCount;
            break;
        default:
            result = _wcsicmp(item1->name, item2->name);
            break;
    }

    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, (USHORT)result);
}

STDMETHODIMP ShellFolder::CreateViewObject(HWND hwndOwner, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    // Log what interface is being requested
    WCHAR guidStr[64];
    StringFromGUID2(riid, guidStr, 64);
    SQLLOCALDB_LOG(L"CreateViewObject: IID=%s IsTableView=%d tableName='%s'", 
        guidStr, IsTableView() ? 1 : 0, tableName_.c_str());

    if (IsEqualIID(riid, IID_IShellView)) {
        SQLLOCALDB_LOG(L"CreateViewObject: Creating IShellView for folder (IsTableView=%d)", IsTableView() ? 1 : 0);
        
        SFV_CREATE sfvc = {};
        sfvc.cbSize = sizeof(sfvc);
        sfvc.pshf = static_cast<IShellFolder*>(this);
        sfvc.psfvcb = static_cast<IShellFolderViewCB*>(this);
        
        HRESULT hr = SHCreateShellFolderView(&sfvc, (IShellView**)ppv);
        SQLLOCALDB_LOG(L"CreateViewObject: SHCreateShellFolderView returned 0x%08X", hr);
        return hr;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP ShellFolder::GetAttributesOf(UINT cidl, PCUITEMID_CHILD_ARRAY apidl, SFGAOF* rgfInOut) {
    if (!rgfInOut) return E_POINTER;
    if (cidl == 0) return E_INVALIDARG;

    SFGAOF attrs = *rgfInOut;  // Start with requested attributes
    
    SQLLOCALDB_LOG(L"GetAttributesOf called: cidl=%u requested=0x%08X", cidl, attrs);

    for (UINT i = 0; i < cidl; i++) {
        const ItemData* item = GetItemData(apidl[i]);
        if (item) {
            SFGAOF itemAttrs = 0;
            
            if (item->type == ItemType::Table || item->type == ItemType::View) {
                // Tabelas e Views são pastas navegáveis. APENAS FOLDER|BROWSABLE.
                // NAO declarar STORAGE / FILESYSANCESTOR / STORAGEANCESTOR: itens
                // virtuais que declaram isso fazem o shell tentar bind IStorage /
                // resolver caminho de arquivo -> falha -> navegacao abortada.
                // (Igual ao SQLiteShell, que navega corretamente.)
                itemAttrs = SFGAO_FOLDER | SFGAO_BROWSABLE;
                SQLLOCALDB_LOG(L"GetAttributesOf: '%s' type=%d -> FOLDER|BROWSABLE (0x%08X)",
                    item->name, (int)item->type, itemAttrs);
            }
            else if (item->type == ItemType::Row) {
                // Registros são arquivos (não navegáveis, não são pastas)
                // IMPORTANTE: NÃO incluir SFGAO_FOLDER para que não apareçam na árvore
                // SFGAO_STREAM indica que é um item de dados (como um arquivo)
                itemAttrs = SFGAO_STREAM | SFGAO_CANCOPY;
                SQLLOCALDB_LOG(L"GetAttributesOf: '%s' -> STREAM|CANCOPY (row - NON FOLDER)", item->name);
            }
            else {
                // Outros itens (Index, SystemTable)
                itemAttrs = SFGAO_STREAM | SFGAO_CANCOPY;
                SQLLOCALDB_LOG(L"GetAttributesOf: '%s' -> STREAM|CANCOPY (other)", item->name);
            }
            
            attrs &= itemAttrs;
        }
    }

    SQLLOCALDB_LOG(L"GetAttributesOf returning: 0x%08X", attrs);
    *rgfInOut = attrs;
    return S_OK;
}

STDMETHODIMP ShellFolder::GetUIObjectOf(HWND hwndOwner, UINT cidl, PCUITEMID_CHILD_ARRAY apidl,
    REFIID riid, UINT* rgfReserved, void** ppv) {
    
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (cidl != 1) return E_INVALIDARG;

    const ItemData* item = GetItemData(apidl[0]);
    if (!item) return E_INVALIDARG;

    // Log all requested IIDs for debugging
    WCHAR guidStr[64];
    StringFromGUID2(riid, guidStr, 64);
    SQLLOCALDB_LOG(L"GetUIObjectOf: item='%s' type=%d IID=%s", item->name, (int)item->type, guidStr);

    if (IsEqualIID(riid, IID_IContextMenu)) {
        ContextMenuHandler* menu = new (std::nothrow) ContextMenuHandler();
        if (!menu) return E_OUTOFMEMORY;
        
        // Configure menu with database path and table name
        menu->SetDatabasePath(dbPath_);
        menu->SetTableName(item->name);
        menu->SetItemType(item->type);
        menu->SetItemPidl(apidl[0]);  // Pass PIDL for navigation
        
        // Pass site for IShellBrowser access
        if (site_) {
            menu->SetSite(site_);
        }
        
        SQLLOCALDB_LOG(L"GetUIObjectOf: Creating ContextMenu for item='%s' type=%d in db '%s'", 
            item->name, (int)item->type, dbPath_.c_str());
        
        HRESULT hr = menu->QueryInterface(riid, ppv);
        menu->Release();
        return hr;
    }
    else if (IsEqualIID(riid, IID_IExtractIconW)) {
        IconHandler* icon = new (std::nothrow) IconHandler();
        if (!icon) return E_OUTOFMEMORY;
        
        switch (item->type) {
            case ItemType::Table:
                icon->SetIconType(IconType::Table);
                break;
            case ItemType::View:
                icon->SetIconType(IconType::View);
                break;
            case ItemType::Index:
                icon->SetIconType(IconType::Index);
                break;
            case ItemType::SystemTable:
                icon->SetIconType(IconType::SystemTable);
                break;
            default:
                icon->SetIconType(IconType::Table);
                break;
        }
        icon->SetItemName(item->name);
        
        HRESULT hr = icon->QueryInterface(riid, ppv);
        icon->Release();
        return hr;
    }
    else if (IsEqualIID(riid, IID_IDataObject)) {
        // Para copiar dados
        // TODO: Implementar IDataObject para exportar dados
    }
    else if (IsEqualIID(riid, IID_IPreviewHandler)) {
        PreviewHandler* preview = new (std::nothrow) PreviewHandler();
        if (!preview) return E_OUTOFMEMORY;
        
        preview->SetDatabasePath(dbPath_);
        preview->SetTableName(item->name);
        
        SQLLOCALDB_LOG(L"GetUIObjectOf: Creating PreviewHandler for table '%s' in db '%s'", 
            item->name, dbPath_.c_str());
        
        HRESULT hr = preview->QueryInterface(riid, ppv);
        preview->Release();
        return hr;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP ShellFolder::GetDisplayNameOf(PCUITEMID_CHILD pidl, SHGDNF uFlags, STRRET* pName) {
    if (!pName) return E_POINTER;

    const ItemData* item = GetItemData(pidl);
    if (!item) return E_INVALIDARG;

    pName->uType = STRRET_WSTR;
    return SHStrDupW(item->name, &pName->pOleStr);
}

STDMETHODIMP ShellFolder::SetNameOf(HWND hwnd, PCUITEMID_CHILD pidl, LPCWSTR pszName,
    SHGDNF uFlags, PITEMID_CHILD* ppidlOut) {
    // Não permitimos renomear tabelas pelo Explorer
    return E_NOTIMPL;
}

// IShellFolder2
STDMETHODIMP ShellFolder::GetDefaultSearchGUID(GUID* pguid) {
    return E_NOTIMPL;
}

STDMETHODIMP ShellFolder::EnumSearches(IEnumExtraSearch** ppenum) {
    return E_NOTIMPL;
}

STDMETHODIMP ShellFolder::GetDefaultColumn(DWORD dwRes, ULONG* pSort, ULONG* pDisplay) {
    if (pSort) *pSort = 0;      // Ordenar por nome
    if (pDisplay) *pDisplay = 0; // Mostrar nome
    return S_OK;
}

// Carrega (uma vez) os dados da tabela atual para exibir colunas dinamicas.
void ShellFolder::LoadTableData() const {
    if (tableDataLoaded_) return;
    tableDataLoaded_ = true;
    if (tableName_.empty() || dbPath_.empty()) return;
    auto db = DatabasePool::Instance().GetDatabase(dbPath_);
    if (!db) return;
    const int MAX_ROWS = 1000;
    tableData_ = db->GetTableData(tableName_, 0, MAX_ROWS);
}

// Numero de colunas de detalhe: root = 4 (Name/Type/Rows/Columns);
// dentro de tabela = colunas reais da tabela.
UINT ShellFolder::ColumnCount() const {
    if (tableName_.empty()) return 4;
    LoadTableData();
    UINT n = (UINT)tableData_.columnNames.size();
    return n ? n : 1;
}

STDMETHODIMP ShellFolder::GetDefaultColumnState(UINT iColumn, SHCOLSTATEF* pcsFlags) {
    if (!pcsFlags) return E_POINTER;

    // Root: colunas fixas.
    if (tableName_.empty()) {
        switch (iColumn) {
            case 0: *pcsFlags = SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT; return S_OK;
            case 1: *pcsFlags = SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT; return S_OK;
            case 2: *pcsFlags = SHCOLSTATE_TYPE_INT | SHCOLSTATE_ONBYDEFAULT; return S_OK;
            case 3: *pcsFlags = SHCOLSTATE_TYPE_INT | SHCOLSTATE_ONBYDEFAULT; return S_OK;
            default: return E_INVALIDARG;
        }
    }

    // Dentro de tabela: uma coluna por coluna real, todas visiveis.
    if (iColumn >= ColumnCount()) return E_INVALIDARG;
    *pcsFlags = SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT;
    return S_OK;
}

STDMETHODIMP ShellFolder::GetDetailsEx(PCUITEMID_CHILD pidl, const SHCOLUMNID* pscid, VARIANT* pv) {
    if (!pv) return E_POINTER;
    
    VariantInit(pv);

    const ItemData* item = GetItemData(pidl);
    if (!item) return E_INVALIDARG;

    // TODO: Mapear SCIDs para dados
    
    return E_NOTIMPL;
}

STDMETHODIMP ShellFolder::GetDetailsOf(PCUITEMID_CHILD pidl, UINT iColumn, SHELLDETAILS* psd) {
    if (!psd) return E_POINTER;

    psd->fmt = LVCFMT_LEFT;
    psd->cxChar = 20;

    // ---- Cabeçalho de coluna (pidl == NULL) ----
    if (!pidl) {
        // Root: colunas fixas.
        if (tableName_.empty()) {
            switch (iColumn) {
                case 0: psd->cxChar = 30; return SetStrRet(&psd->str, L"Name");
                case 1: psd->cxChar = 12; return SetStrRet(&psd->str, L"Type");
                case 2: psd->fmt = LVCFMT_RIGHT; psd->cxChar = 12; return SetStrRet(&psd->str, L"Rows");
                case 3: psd->fmt = LVCFMT_RIGHT; psd->cxChar = 10; return SetStrRet(&psd->str, L"Columns");
                default: return E_INVALIDARG;
            }
        }
        // Dentro de tabela: cabeçalho = nome da coluna real.
        LoadTableData();
        if (iColumn >= tableData_.columnNames.size()) return E_INVALIDARG;
        psd->cxChar = 24;
        return SetStrRet(&psd->str, tableData_.columnNames[iColumn].c_str());
    }

    // ---- Dados do item ----
    const ItemData* item = GetItemData(pidl);
    if (!item) return E_INVALIDARG;

    // Root (item = tabela/view).
    if (tableName_.empty()) {
        switch (iColumn) {
            case 0:
                return SetStrRet(&psd->str, item->name);
            case 1:
                switch (item->type) {
                    case ItemType::Table: return SetStrRet(&psd->str, L"Table");
                    case ItemType::View: return SetStrRet(&psd->str, L"View");
                    case ItemType::Index: return SetStrRet(&psd->str, L"Index");
                    case ItemType::SystemTable: return SetStrRet(&psd->str, L"System");
                    default: return SetStrRet(&psd->str, L"Unknown");
                }
            case 2:
                if (item->rowCount >= 0) {
                    wchar_t buf[32];
                    StringCchPrintfW(buf, 32, L"%lld", item->rowCount);
                    return SetStrRet(&psd->str, buf);
                }
                return SetStrRet(&psd->str, L"-");
            case 3: {
                wchar_t buf[32];
                StringCchPrintfW(buf, 32, L"%d", item->columnCount);
                return SetStrRet(&psd->str, buf);
            }
            default:
                return E_INVALIDARG;
        }
    }

    // Dentro de tabela (item = linha). item->rowCount = ordinal 1-based.
    LoadTableData();
    if (iColumn >= tableData_.columnNames.size()) return E_INVALIDARG;
    int64_t ordinal = item->rowCount;
    size_t idx = (ordinal > 0) ? (size_t)(ordinal - 1) : 0;
    if (idx < tableData_.rows.size() && iColumn < tableData_.rows[idx].size()) {
        const CellValue& cell = tableData_.rows[idx][iColumn];
        std::wstring value = cell.IsNull() ? L"" : cell.ToString();
        return SetStrRet(&psd->str, value.c_str());
    }
    return SetStrRet(&psd->str, L"");
}

STDMETHODIMP ShellFolder::MapColumnToSCID(UINT iColumn, SHCOLUMNID* pscid) {
    if (!pscid) return E_POINTER;
    if (iColumn >= ColumnCount()) return E_INVALIDARG;

    pscid->fmtid = LOCAL_FMTID_Storage;
    // Coluna 0 = nome; demais recebem PIDs unicos estaveis.
    pscid->pid = (iColumn == 0) ? PID_STG_NAME : (iColumn + 100);
    return S_OK;
}

// IShellFolderViewCB
STDMETHODIMP ShellFolder::MessageSFVCB(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Log messages para debug
    SQLLOCALDB_LOG(L"MessageSFVCB: uMsg=%u wParam=%p lParam=%p", uMsg, (void*)wParam, (void*)lParam);
    
    switch (uMsg) {
        case SFVM_DEFVIEWMODE: {
            // Define o modo de visualização padrão (Details)
            // wParam = não usado, lParam = ponteiro para FOLDERVIEWMODE
            if (lParam) {
                *reinterpret_cast<UINT*>(lParam) = FVM_DETAILS;
                return S_OK;
            }
            break;
        }
        
        case SFVM_WINDOWCREATED: {
            // A view foi criada, podemos guardar o hwnd
            SQLLOCALDB_LOG(L"SFVM_WINDOWCREATED: hwnd=%p", (void*)wParam);
            break;
        }
        
        case SFVM_INVOKECOMMAND: {
            // wParam = idCmd (comando invocado)
            // Este é chamado quando o usuário dá double-click ou pressiona Enter
            SQLLOCALDB_LOG(L"SFVM_INVOKECOMMAND: idCmd=%u", (UINT)wParam);
            // Retornar S_FALSE para permitir processamento padrão do shell
            // O shell deve então chamar BindToObject para navegação
            return S_FALSE;
        }
    }
    
    return E_NOTIMPL;
}

// IObjectWithSite
STDMETHODIMP ShellFolder::SetSite(IUnknown* pUnkSite) {
    site_ = pUnkSite;
    return S_OK;
}

STDMETHODIMP ShellFolder::GetSite(REFIID riid, void** ppvSite) {
    if (!ppvSite) return E_POINTER;
    
    if (site_) {
        return site_->QueryInterface(riid, ppvSite);
    }
    
    *ppvSite = nullptr;
    return E_FAIL;
}

// ============================================================================
// IPersistFile Implementation - CRÍTICO para Windows navegar dentro do arquivo!
// O Windows chama IPersistFile::Load() quando o usuário dá double-click no arquivo
// para "entrar" nele como se fosse uma pasta (igual a arquivos .zip)
// ============================================================================

STDMETHODIMP ShellFolder::IsDirty() {
    // Não suportamos modificações, então nunca está "dirty"
    return S_FALSE;
}

STDMETHODIMP ShellFolder::Load(LPCOLESTR pszFileName, DWORD dwMode) {
    if (!pszFileName) return E_INVALIDARG;
    
    SQLLOCALDB_LOG(L"ShellFolder::Load called with file: %s (dwMode=0x%X)", pszFileName, dwMode);
    
    // DIAGNOSTIC: Show message box to confirm Load is being called
    SQLLOCALDB_MSGBOX(L"SQLLocalDBView Debug", 
        L"IPersistFile::Load() called!\n\nFile: %s\nMode: 0x%X\n\nThis means Windows is trying to open the file as a folder.", 
        pszFileName, dwMode);
    
    dbPath_ = pszFileName;
    
    // Criar PIDL para o arquivo
    if (pidlRoot_) {
        CoTaskMemFree(pidlRoot_);
        pidlRoot_ = nullptr;
    }
    
    // Usar SHParseDisplayName para obter o PIDL absoluto do arquivo
    HRESULT hr = SHParseDisplayName(pszFileName, nullptr, &pidlRoot_, 0, nullptr);
    if (FAILED(hr)) {
        SQLLOCALDB_LOG(L"ShellFolder::Load - SHParseDisplayName failed: 0x%08X", hr);
        // Mesmo se falhar em obter PIDL, temos o caminho do arquivo
        return S_OK;
    }
    
    SQLLOCALDB_LOG(L"ShellFolder::Load succeeded for: %s", pszFileName);
    return S_OK;
}

STDMETHODIMP ShellFolder::Save(LPCOLESTR pszFileName, BOOL fRemember) {
    // Não suportamos salvar - somente leitura
    return E_NOTIMPL;
}

STDMETHODIMP ShellFolder::SaveCompleted(LPCOLESTR pszFileName) {
    return S_OK;
}

STDMETHODIMP ShellFolder::GetCurFile(LPOLESTR* ppszFileName) {
    if (!ppszFileName) return E_POINTER;
    
    *ppszFileName = nullptr;
    
    if (dbPath_.empty()) return S_FALSE;
    
    *ppszFileName = (LPOLESTR)CoTaskMemAlloc((dbPath_.length() + 1) * sizeof(WCHAR));
    if (!*ppszFileName) return E_OUTOFMEMORY;
    
    wcscpy_s(*ppszFileName, dbPath_.length() + 1, dbPath_.c_str());
    return S_OK;
}

// ============================================================================
// Helpers
// ============================================================================

const ItemData* ShellFolder::GetItemData(PCUITEMID_CHILD pidl) {
    if (!pidl) return nullptr;
    
    const ItemData* item = reinterpret_cast<const ItemData*>(pidl);
    
    // Validar signature
    if (item->signature != ItemData::SIGNATURE) {
        return nullptr;
    }
    
    return item;
}

PITEMID_CHILD ShellFolder::CreateItemID(const TableInfo& table) {
    return CreateItemID(
        table.name, 
        table.type == DatabaseObjectType::View ? ItemType::View : ItemType::Table,
        table.rowCount,
        table.columnCount
    );
}

PITEMID_CHILD ShellFolder::CreateItemID(const std::wstring& name, ItemType type, 
    int64_t rowCount, int columnCount) {
    
    size_t nameLen = name.length();
    if (nameLen >= 255) nameLen = 255;
    
    // Tamanho: ItemData struct + null terminator + final null PIDL
    UINT size = sizeof(ItemData) + sizeof(USHORT);
    
    PITEMID_CHILD pidl = (PITEMID_CHILD)CoTaskMemAlloc(size);
    if (!pidl) return nullptr;
    
    ZeroMemory(pidl, size);
    
    ItemData* item = reinterpret_cast<ItemData*>(pidl);
    item->cb = sizeof(ItemData);
    item->signature = ItemData::SIGNATURE;
    item->type = type;
    StringCchCopyW(item->name, 256, name.c_str());
    item->rowCount = rowCount;
    item->columnCount = (USHORT)columnCount;
    item->tableName[0] = L'\0';  // Não é um row
    
    // Terminating null PIDL
    *reinterpret_cast<USHORT*>(reinterpret_cast<BYTE*>(pidl) + item->cb) = 0;
    
    return pidl;
}

PITEMID_CHILD ShellFolder::CreateRowItemID(int64_t rowid, const std::wstring& displayName, 
    const std::wstring& tableName) {
    
    // Tamanho: ItemData struct + null terminator + final null PIDL
    UINT size = sizeof(ItemData) + sizeof(USHORT);
    
    PITEMID_CHILD pidl = (PITEMID_CHILD)CoTaskMemAlloc(size);
    if (!pidl) return nullptr;
    
    ZeroMemory(pidl, size);
    
    ItemData* item = reinterpret_cast<ItemData*>(pidl);
    item->cb = sizeof(ItemData);
    item->signature = ItemData::SIGNATURE;
    item->type = ItemType::Row;
    StringCchCopyW(item->name, 256, displayName.c_str());
    item->rowCount = rowid;  // Usar rowCount para armazenar o rowid
    item->columnCount = 0;
    StringCchCopyW(item->tableName, 128, tableName.c_str());
    
    // Terminating null PIDL
    *reinterpret_cast<USHORT*>(reinterpret_cast<BYTE*>(pidl) + item->cb) = 0;
    
    return pidl;
}

// ============================================================================
// EnumIDList Implementation
// ============================================================================

EnumIDList::EnumIDList(ShellFolder* folder, SHCONTF flags)
    : refCount_(1)
    , folder_(folder)
    , flags_(flags)
    , currentIndex_(0)
    , initialized_(false) {
    
    if (folder_) folder_->AddRef();
}

EnumIDList::~EnumIDList() {
    // Liberar PIDLs
    for (auto& pidl : items_) {
        if (pidl) CoTaskMemFree(pidl);
    }
    items_.clear();
    
    if (folder_) folder_->Release();
}

STDMETHODIMP EnumIDList::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IEnumIDList)) {
        *ppv = static_cast<IEnumIDList*>(this);
        AddRef();
        return S_OK;
    }

    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) EnumIDList::AddRef() {
    return InterlockedIncrement(&refCount_);
}

STDMETHODIMP_(ULONG) EnumIDList::Release() {
    LONG count = InterlockedDecrement(&refCount_);
    if (count == 0) {
        delete this;
    }
    return count;
}

void EnumIDList::Initialize() {
    if (initialized_) return;
    initialized_ = true;

    if (!folder_ || !folder_->OpenDatabase()) return;

    auto db = DatabasePool::Instance().GetDatabase(folder_->GetDatabasePath());
    if (!db) return;

    SQLLOCALDB_LOG(L"EnumIDList::Initialize flags=0x%08X IsTableView=%d", flags_, folder_->IsTableView() ? 1 : 0);

    // Se estamos dentro de uma tabela, listar os registros
    // IMPORTANTE: Rows são NONFOLDERS, então só listar se SHCONTF_NONFOLDERS estiver nos flags
    // Isso é crucial para que as rows apareçam apenas no painel direito (ListView) e não na árvore (TreeView)
    if (folder_->IsTableView()) {
        // TreeView pede apenas SHCONTF_FOLDERS, ListView pede SHCONTF_FOLDERS | SHCONTF_NONFOLDERS
        // Rows são NONFOLDERS, então verificamos se esse flag está presente
        if (!(flags_ & SHCONTF_NONFOLDERS)) {
            SQLLOCALDB_LOG(L"EnumIDList::Initialize TableView but SHCONTF_NONFOLDERS not set - returning empty (TreeView call)");
            return; // Não listar nada para a TreeView quando estamos mostrando rows
        }
        
        std::wstring tableName = folder_->GetTableName();
        SQLLOCALDB_LOG(L"EnumIDList::Initialize listing rows of table '%s' (SHCONTF_NONFOLDERS set)", tableName.c_str());
        
        // Usar o MESMO cache que GetDetailsOf/CompareIDs (folder_->tableData_) para
        // que o ordinal do PIDL corresponda exatamente a linha exibida.
        folder_->LoadTableData();
        const QueryResult& result = folder_->tableData_;
        if (!result.error.empty()) {
            SQLLOCALDB_LOG(L"EnumIDList::Initialize LoadTableData FAILED for '%s': %s",
                tableName.c_str(), result.error.c_str());
        }
        SQLLOCALDB_LOG(L"EnumIDList::Initialize table '%s' -> %d rows, %d cols",
            tableName.c_str(), (int)result.rows.size(), (int)result.columnNames.size());
        if (result.error.empty() && !result.rows.empty()) {
            // Determinar qual coluna usar para o nome de exibição.
            // Preferir colunas como 'name', 'title', etc; senão a primeira coluna.
            // SQL Server não possui rowid; usamos o ordinal da linha como identificador.
            int displayColIndex = 0;
            for (size_t i = 0; i < result.columnNames.size(); i++) {
                std::wstring colLower = result.columnNames[i];
                std::transform(colLower.begin(), colLower.end(), colLower.begin(), ::towlower);
                if (colLower == L"name" || colLower == L"nome" || colLower == L"title" ||
                    colLower == L"titulo" || colLower == L"descricao" || colLower == L"description") {
                    displayColIndex = (int)i;
                    break;
                }
            }

            for (size_t rowIdx = 0; rowIdx < result.rows.size(); rowIdx++) {
                const auto& row = result.rows[rowIdx];
                if (row.empty()) continue;

                // SQL Server não tem rowid: usar ordinal 1-based.
                int64_t rowid = (int64_t)rowIdx + 1;

                // Criar nome de exibição
                std::wstring displayName;
                if (displayColIndex < (int)row.size()) {
                    const auto& cell = row[displayColIndex];
                    if (cell.type == CellValue::Type::Text) {
                        displayName = cell.textValue;
                    } else if (cell.type == CellValue::Type::Integer) {
                        displayName = std::to_wstring(cell.intValue);
                    } else if (cell.type == CellValue::Type::Real) {
                        displayName = std::to_wstring(cell.realValue);
                    } else if (cell.type == CellValue::Type::Null) {
                        displayName = L"(null)";
                    } else {
                        displayName = L"(blob)";
                    }
                }
                
                // Truncar nome longo
                if (displayName.length() > 80) {
                    displayName = displayName.substr(0, 77) + L"...";
                }
                
                // Nome da linha (coluna 0) = valor da coluna de exibicao.
                wchar_t rowName[256];
                if (displayName.empty())
                    StringCchPrintfW(rowName, 256, L"Row %lld", rowid);
                else
                    StringCchCopyW(rowName, 256, displayName.c_str());
                
                PITEMID_CHILD pidl = folder_->CreateRowItemID(rowid, rowName, tableName);
                if (pidl) {
                    items_.push_back(pidl);
                }
            }
            
            SQLLOCALDB_LOG(L"EnumIDList::Initialize found %d rows in table '%s'", 
                (int)items_.size(), tableName.c_str());
        }
        return;
    }

    // Caso contrário, listar tabelas (nível raiz do banco)
    // Tabelas agora são FOLDERS (não NONFOLDERS)
    if (flags_ & SHCONTF_FOLDERS) {
        auto tables = db->GetTables();
        for (const auto& table : tables) {
            PITEMID_CHILD pidl = folder_->CreateItemID(table);
            if (pidl) {
                items_.push_back(pidl);
            }
        }

        // Obter views
        auto views = db->GetViews();
        for (const auto& view : views) {
            PITEMID_CHILD pidl = folder_->CreateItemID(
                view.name,
                ItemType::View,
                -1,
                view.columnCount
            );
            if (pidl) {
                items_.push_back(pidl);
            }
        }
    }

    SQLLOCALDB_LOG(L"EnumIDList::Initialize found %d tables/views", (int)items_.size());
}

STDMETHODIMP EnumIDList::Next(ULONG celt, PITEMID_CHILD* rgelt, ULONG* pceltFetched) {
    if (!rgelt) return E_POINTER;

    Initialize();

    SQLLOCALDB_LOG(L"EnumIDList::Next requested=%lu available=%zu currentIndex=%zu", 
        celt, items_.size(), currentIndex_);

    ULONG fetched = 0;
    while (fetched < celt && currentIndex_ < items_.size()) {
        rgelt[fetched] = ILCloneChild(items_[currentIndex_]);
        if (rgelt[fetched]) {
            fetched++;
        }
        currentIndex_++;
    }

    SQLLOCALDB_LOG(L"EnumIDList::Next returning %lu items", fetched);

    if (pceltFetched) *pceltFetched = fetched;
    
    return (fetched == celt) ? S_OK : S_FALSE;
}

STDMETHODIMP EnumIDList::Skip(ULONG celt) {
    Initialize();
    
    currentIndex_ += celt;
    if (currentIndex_ > items_.size()) {
        currentIndex_ = items_.size();
        return S_FALSE;
    }
    
    return S_OK;
}

STDMETHODIMP EnumIDList::Reset() {
    currentIndex_ = 0;
    return S_OK;
}

STDMETHODIMP EnumIDList::Clone(IEnumIDList** ppenum) {
    if (!ppenum) return E_POINTER;

    EnumIDList* clone = new (std::nothrow) EnumIDList(folder_, flags_);
    if (!clone) return E_OUTOFMEMORY;

    clone->currentIndex_ = currentIndex_;
    clone->initialized_ = initialized_;
    
    for (auto& pidl : items_) {
        PITEMID_CHILD cloned = ILCloneChild(pidl);
        if (cloned) {
            clone->items_.push_back(cloned);
        }
    }

    *ppenum = clone;
    return S_OK;
}

} // namespace SQLLocalDBView
