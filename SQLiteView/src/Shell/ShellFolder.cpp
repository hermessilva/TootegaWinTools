/*
** SQLiteView - Windows Explorer Shell Extension
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

namespace SQLiteView {

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
    SQLITEVIEW_LOG(L"ClassFactory::CreateInstance CLSID=%s riid=%s", clsidStr, riidStr);

    HRESULT hr = E_OUTOFMEMORY;

    if (IsEqualCLSID(clsid_, CLSID_SQLiteViewFolder)) {
        SQLITEVIEW_LOG(L"  -> Creating ShellFolder instance");
        ShellFolder* folder = new (std::nothrow) ShellFolder();
        if (folder) {
            hr = folder->QueryInterface(riid, ppv);
            folder->Release();
            SQLITEVIEW_LOG(L"  -> ShellFolder QueryInterface returned 0x%08X", hr);
        }
    }
    else if (IsEqualCLSID(clsid_, CLSID_SQLiteViewPreview)) {
        SQLITEVIEW_LOG(L"  -> Creating PreviewHandler instance");
        // PreviewHandler criado aqui
        PreviewHandler* preview = new (std::nothrow) PreviewHandler();
        if (preview) {
            hr = preview->QueryInterface(riid, ppv);
            preview->Release();
        }
    }
    else if (IsEqualCLSID(clsid_, CLSID_SQLiteViewContextMenu)) {
        SQLITEVIEW_LOG(L"  -> Creating ContextMenu instance");
        // Use DatabaseContextMenuHandler for .db files context menu
        DatabaseContextMenuHandler* menu = new (std::nothrow) DatabaseContextMenuHandler();
        if (menu) {
            hr = menu->QueryInterface(riid, ppv);
            menu->Release();
        }
    }
    else if (IsEqualCLSID(clsid_, CLSID_SQLiteViewProperty)) {
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
    SQLITEVIEW_LOG(L"ShellFolder created");
}

ShellFolder::~ShellFolder() {
    if (pidlRoot_) {
        CoTaskMemFree(pidlRoot_);
    }
    SQLITEVIEW_LOG(L"ShellFolder destroyed");
}

STDMETHODIMP ShellFolder::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    // Log all interface requests for debugging
    wchar_t iidStr[64];
    StringFromGUID2(riid, iidStr, 64);
    SQLITEVIEW_LOG(L"ShellFolder::QueryInterface requested: %s", iidStr);

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_IShellFolder) ||
        IsEqualIID(riid, IID_IShellFolder2)) {
        SQLITEVIEW_LOG(L"  -> Returning IShellFolder2");
        *ppv = static_cast<IShellFolder2*>(this);
    }
    else if (IsEqualIID(riid, IID_IPersistFile)) {
        // CRÍTICO: Windows chama IPersistFile::Load() para passar o caminho do arquivo!
        SQLITEVIEW_LOG(L"  -> Returning IPersistFile (CRITICAL for file-based navigation!)");
        *ppv = static_cast<IPersistFile*>(this);
    }
    else if (IsEqualIID(riid, IID_IPersist) ||
             IsEqualIID(riid, IID_IPersistFolder) ||
             IsEqualIID(riid, IID_IPersistFolder2) ||
             IsEqualIID(riid, IID_IPersistFolder3)) {
        SQLITEVIEW_LOG(L"  -> Returning IPersistFolder3");
        *ppv = static_cast<IPersistFolder3*>(this);
    }
    else if (IsEqualIID(riid, IID_IShellFolderViewCB)) {
        SQLITEVIEW_LOG(L"  -> Returning IShellFolderViewCB");
        *ppv = static_cast<IShellFolderViewCB*>(this);
    }
    else if (IsEqualIID(riid, IID_IObjectWithSite)) {
        SQLITEVIEW_LOG(L"  -> Returning IObjectWithSite");
        *ppv = static_cast<IObjectWithSite*>(this);
    }
    else {
        SQLITEVIEW_LOG(L"  -> E_NOINTERFACE");
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
    *pClassID = CLSID_SQLiteViewFolder;
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

    // Extrair caminho do arquivo DB do PIDL
    wchar_t path[MAX_PATH];
    if (SHGetPathFromIDListW(pidl, path)) {
        dbPath_ = path;
        SQLITEVIEW_LOG(L"ShellFolder::Initialize path=%s", path);
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
            SQLITEVIEW_LOG(L"ShellFolder::Initialize PIDL item %d: cb=%u", itemCount, child->mkid.cb);
            
            // Verificar se este item é um ItemData nosso (pela assinatura)
            if (child->mkid.cb >= sizeof(ItemData)) {
                const ItemData* item = reinterpret_cast<const ItemData*>(&child->mkid);
                SQLITEVIEW_LOG(L"ShellFolder::Initialize PIDL item %d: signature=0x%04X (expected 0x%04X)", 
                    itemCount, item->signature, ItemData::SIGNATURE);
                if (item->signature == ItemData::SIGNATURE) {
                    // Encontramos um item nosso
                    SQLITEVIEW_LOG(L"ShellFolder::Initialize found ItemData: type=%d name='%s'", 
                        (int)item->type, item->name);
                    if (item->type == ItemType::Table || item->type == ItemType::View) {
                        tableItem = item;
                        SQLITEVIEW_LOG(L"ShellFolder::Initialize found table in PIDL: '%s'", item->name);
                    }
                }
            }
            // Avançar para o próximo item do PIDL
            child = reinterpret_cast<PCUIDLIST_RELATIVE>(
                reinterpret_cast<const BYTE*>(child) + child->mkid.cb);
        }
        
        SQLITEVIEW_LOG(L"ShellFolder::Initialize processed %d PIDL items, tableItem=%p", itemCount, tableItem);
        
        // Se encontramos uma tabela, configurar para mostrar seus registros
        if (tableItem) {
            tableName_ = tableItem->name;
            SQLITEVIEW_LOG(L"ShellFolder::Initialize configured as TableView for '%s'", tableName_.c_str());
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
    SQLITEVIEW_LOG(L"EnumObjects called: hwnd=%p flags=0x%08X (FOLDERS=%d NONFOLDERS=%d) IsTableView=%d tableName='%s'",
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
    SQLITEVIEW_LOG(L"BindToObject: item='%s' type=%d IID=%s", item->name, (int)item->type, guidStr);
    
    // Tabelas e Views são navegáveis como subpastas
    if (item->type == ItemType::Table || item->type == ItemType::View) {
        // Para IShellFolder/IShellFolder2, criar subfolder configurado para mostrar rows
        if (IsEqualIID(riid, IID_IShellFolder) || IsEqualIID(riid, IID_IShellFolder2)) {
            ShellFolder* subfolder = new (std::nothrow) ShellFolder();
            if (!subfolder) return E_OUTOFMEMORY;
            
            // Configurar o subfolder para mostrar registros da tabela
            subfolder->SetDatabasePath(dbPath_);
            subfolder->SetTableName(item->name);
            
            SQLITEVIEW_LOG(L"BindToObject: Creating subfolder for table '%s' in db '%s'", 
                item->name, dbPath_.c_str());
            
            HRESULT hr = subfolder->QueryInterface(riid, ppv);
            subfolder->Release();
            return hr;
        }
        
        // Para outras interfaces de navegação, também criar o subfolder e deixar QueryInterface resolver
        // Isso permite que o Explorer obtenha informações do subfolder
        ShellFolder* subfolder = new (std::nothrow) ShellFolder();
        if (!subfolder) return E_OUTOFMEMORY;
        
        subfolder->SetDatabasePath(dbPath_);
        subfolder->SetTableName(item->name);
        
        SQLITEVIEW_LOG(L"BindToObject: Trying subfolder QueryInterface for '%s'", item->name);
        
        HRESULT hr = subfolder->QueryInterface(riid, ppv);
        subfolder->Release();
        
        if (SUCCEEDED(hr)) {
            return hr;
        }
        // Se falhar, continua para tentar outras interfaces
    }
    
    // Support IPreviewHandler for Preview Pane
    if (IsEqualIID(riid, IID_IPreviewHandler)) {
        PreviewHandler* preview = new (std::nothrow) PreviewHandler();
        if (!preview) return E_OUTOFMEMORY;
        
        preview->SetDatabasePath(dbPath_);
        preview->SetTableName(item->name);
        
        SQLITEVIEW_LOG(L"BindToObject: Creating PreviewHandler for table '%s'", item->name);
        
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
    SQLITEVIEW_LOG(L"BindToStorage: item='%s' IID=%s", item->name, guidStr);
    
    // Support IPreviewHandler via BindToStorage too
    if (IsEqualIID(riid, IID_IPreviewHandler)) {
        PreviewHandler* preview = new (std::nothrow) PreviewHandler();
        if (!preview) return E_OUTOFMEMORY;
        
        preview->SetDatabasePath(dbPath_);
        preview->SetTableName(item->name);
        
        SQLITEVIEW_LOG(L"BindToStorage: Creating PreviewHandler for table '%s'", item->name);
        
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
    SQLITEVIEW_LOG(L"CreateViewObject: IID=%s IsTableView=%d tableName='%s'", 
        guidStr, IsTableView() ? 1 : 0, tableName_.c_str());

    if (IsEqualIID(riid, IID_IShellView)) {
        SQLITEVIEW_LOG(L"CreateViewObject: Creating IShellView for folder (IsTableView=%d)", IsTableView() ? 1 : 0);
        
        SFV_CREATE sfvc = {};
        sfvc.cbSize = sizeof(sfvc);
        sfvc.pshf = static_cast<IShellFolder*>(this);
        sfvc.psfvcb = static_cast<IShellFolderViewCB*>(this);
        
        HRESULT hr = SHCreateShellFolderView(&sfvc, (IShellView**)ppv);
        SQLITEVIEW_LOG(L"CreateViewObject: SHCreateShellFolderView returned 0x%08X", hr);
        return hr;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP ShellFolder::GetAttributesOf(UINT cidl, PCUITEMID_CHILD_ARRAY apidl, SFGAOF* rgfInOut) {
    if (!rgfInOut) return E_POINTER;
    if (cidl == 0) return E_INVALIDARG;

    SFGAOF attrs = *rgfInOut;  // Start with requested attributes
    
    SQLITEVIEW_LOG(L"GetAttributesOf called: cidl=%u requested=0x%08X", cidl, attrs);

    for (UINT i = 0; i < cidl; i++) {
        const ItemData* item = GetItemData(apidl[i]);
        if (item) {
            SFGAOF itemAttrs = 0;
            
            if (item->type == ItemType::Table || item->type == ItemType::View) {
                // Tabelas e Views são pastas navegáveis (como subpastas de ZIP)
                // SFGAO_FOLDER (0x20000000) - É uma pasta
                // SFGAO_BROWSABLE (0x08000000) - Pode ser navegado no Explorer  
                // SFGAO_HASSUBFOLDER (0x80000000) - TEM subpastas (crítico para navegação!)
                // SFGAO_STORAGE (0x00000008) - Contém storage
                // SFGAO_FILESYSANCESTOR (0x10000000) - Ancestral do sistema de arquivos
                // SFGAO_STORAGEANCESTOR (0x00800000) - Ancestral de storage
                itemAttrs = SFGAO_FOLDER | SFGAO_BROWSABLE | SFGAO_HASSUBFOLDER | SFGAO_STORAGE |
                            SFGAO_FILESYSANCESTOR | SFGAO_STORAGEANCESTOR;
                SQLITEVIEW_LOG(L"GetAttributesOf: '%s' type=%d -> FOLDER+HASSUBFOLDER (0x%08X)", 
                    item->name, (int)item->type, itemAttrs);
            }
            else if (item->type == ItemType::Row) {
                // Registros são arquivos (não navegáveis, não são pastas)
                // IMPORTANTE: NÃO incluir SFGAO_FOLDER para que não apareçam na árvore
                // SFGAO_STREAM indica que é um item de dados (como um arquivo)
                itemAttrs = SFGAO_STREAM | SFGAO_CANCOPY;
                SQLITEVIEW_LOG(L"GetAttributesOf: '%s' -> STREAM|CANCOPY (row - NON FOLDER)", item->name);
            }
            else {
                // Outros itens (Index, SystemTable)
                itemAttrs = SFGAO_STREAM | SFGAO_CANCOPY;
                SQLITEVIEW_LOG(L"GetAttributesOf: '%s' -> STREAM|CANCOPY (other)", item->name);
            }
            
            attrs &= itemAttrs;
        }
    }

    SQLITEVIEW_LOG(L"GetAttributesOf returning: 0x%08X", attrs);
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
    SQLITEVIEW_LOG(L"GetUIObjectOf: item='%s' type=%d IID=%s", item->name, (int)item->type, guidStr);

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
        
        SQLITEVIEW_LOG(L"GetUIObjectOf: Creating ContextMenu for item='%s' type=%d in db '%s'", 
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
        
        SQLITEVIEW_LOG(L"GetUIObjectOf: Creating PreviewHandler for table '%s' in db '%s'", 
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

STDMETHODIMP ShellFolder::GetDefaultColumnState(UINT iColumn, SHCOLSTATEF* pcsFlags) {
    if (!pcsFlags) return E_POINTER;

    switch (iColumn) {
        case 0: // Nome
            *pcsFlags = SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT;
            break;
        case 1: // Tipo
            *pcsFlags = SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT;
            break;
        case 2: // Linhas
            *pcsFlags = SHCOLSTATE_TYPE_INT | SHCOLSTATE_ONBYDEFAULT;
            break;
        case 3: // Colunas
            *pcsFlags = SHCOLSTATE_TYPE_INT | SHCOLSTATE_ONBYDEFAULT;
            break;
        default:
            return E_INVALIDARG;
    }

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

    // Cabeçalho (pidl == NULL)
    if (!pidl) {
        switch (iColumn) {
            case 0:
                psd->fmt = LVCFMT_LEFT;
                psd->cxChar = 30;
                return SetStrRet(&psd->str, L"Name");
            case 1:
                psd->cxChar = 12;
                return SetStrRet(&psd->str, L"Type");
            case 2:
                psd->fmt = LVCFMT_RIGHT;
                psd->cxChar = 12;
                return SetStrRet(&psd->str, L"Rows");
            case 3:
                psd->fmt = LVCFMT_RIGHT;
                psd->cxChar = 10;
                return SetStrRet(&psd->str, L"Columns");
            default:
                return E_INVALIDARG;
        }
    }

    // Dados do item
    const ItemData* item = GetItemData(pidl);
    if (!item) return E_INVALIDARG;

    switch (iColumn) {
        case 0: // Nome
            return SetStrRet(&psd->str, item->name);
            
        case 1: // Tipo
            switch (item->type) {
                case ItemType::Table:
                    return SetStrRet(&psd->str, L"Table");
                case ItemType::View:
                    return SetStrRet(&psd->str, L"View");
                case ItemType::Index:
                    return SetStrRet(&psd->str, L"Index");
                case ItemType::SystemTable:
                    return SetStrRet(&psd->str, L"System");
                default:
                    return SetStrRet(&psd->str, L"Unknown");
            }
            
        case 2: // Linhas
            if (item->rowCount >= 0) {
                wchar_t buf[32];
                StringCchPrintfW(buf, 32, L"%lld", item->rowCount);
                return SetStrRet(&psd->str, buf);
            }
            return SetStrRet(&psd->str, L"-");
            
        case 3: // Colunas
            {
                wchar_t buf[32];
                StringCchPrintfW(buf, 32, L"%d", item->columnCount);
                return SetStrRet(&psd->str, buf);
            }
            
        default:
            return E_INVALIDARG;
    }
}

STDMETHODIMP ShellFolder::MapColumnToSCID(UINT iColumn, SHCOLUMNID* pscid) {
    if (!pscid) return E_POINTER;

    switch (iColumn) {
        case 0:
            pscid->fmtid = LOCAL_FMTID_Storage;
            pscid->pid = PID_STG_NAME;
            return S_OK;
        case 1:
            pscid->fmtid = LOCAL_FMTID_Storage;
            pscid->pid = PID_STG_STORAGETYPE;
            return S_OK;
        case 2:
            pscid->fmtid = LOCAL_FMTID_Storage;
            pscid->pid = PID_STG_SIZE;
            return S_OK;
        default:
            return E_INVALIDARG;
    }
}

// IShellFolderViewCB
STDMETHODIMP ShellFolder::MessageSFVCB(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Log messages para debug
    SQLITEVIEW_LOG(L"MessageSFVCB: uMsg=%u wParam=%p lParam=%p", uMsg, (void*)wParam, (void*)lParam);
    
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
            SQLITEVIEW_LOG(L"SFVM_WINDOWCREATED: hwnd=%p", (void*)wParam);
            break;
        }
        
        case SFVM_INVOKECOMMAND: {
            // wParam = idCmd (comando invocado)
            // Este é chamado quando o usuário dá double-click ou pressiona Enter
            SQLITEVIEW_LOG(L"SFVM_INVOKECOMMAND: idCmd=%u", (UINT)wParam);
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
    
    SQLITEVIEW_LOG(L"ShellFolder::Load called with file: %s (dwMode=0x%X)", pszFileName, dwMode);
    
    // DIAGNOSTIC: Show message box to confirm Load is being called
    SQLITEVIEW_MSGBOX(L"SQLiteView Debug", 
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
        SQLITEVIEW_LOG(L"ShellFolder::Load - SHParseDisplayName failed: 0x%08X", hr);
        // Mesmo se falhar em obter PIDL, temos o caminho do arquivo
        return S_OK;
    }
    
    SQLITEVIEW_LOG(L"ShellFolder::Load succeeded for: %s", pszFileName);
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
        (int)table.columns.size()
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

    SQLITEVIEW_LOG(L"EnumIDList::Initialize flags=0x%08X IsTableView=%d", flags_, folder_->IsTableView() ? 1 : 0);

    // Se estamos dentro de uma tabela, listar os registros
    // IMPORTANTE: Rows são NONFOLDERS, então só listar se SHCONTF_NONFOLDERS estiver nos flags
    // Isso é crucial para que as rows apareçam apenas no painel direito (ListView) e não na árvore (TreeView)
    if (folder_->IsTableView()) {
        // TreeView pede apenas SHCONTF_FOLDERS, ListView pede SHCONTF_FOLDERS | SHCONTF_NONFOLDERS
        // Rows são NONFOLDERS, então verificamos se esse flag está presente
        if (!(flags_ & SHCONTF_NONFOLDERS)) {
            SQLITEVIEW_LOG(L"EnumIDList::Initialize TableView but SHCONTF_NONFOLDERS not set - returning empty (TreeView call)");
            return; // Não listar nada para a TreeView quando estamos mostrando rows
        }
        
        std::wstring tableName = folder_->GetTableName();
        SQLITEVIEW_LOG(L"EnumIDList::Initialize listing rows of table '%s' (SHCONTF_NONFOLDERS set)", tableName.c_str());
        
        // Obter os primeiros N registros da tabela (limite para performance)
        const int MAX_ROWS = 1000;
        std::wstring query = L"SELECT rowid, * FROM [" + tableName + L"] LIMIT " + std::to_wstring(MAX_ROWS);
        
        auto result = db->ExecuteQuery(query, MAX_ROWS);
        if (result.error.empty() && !result.rows.empty()) {
            // Determinar qual coluna usar para o nome de exibição
            // Preferir colunas como 'name', 'title', 'id' ou usar a primeira coluna
            int displayColIndex = 0;
            for (size_t i = 0; i < result.columnNames.size(); i++) {
                std::wstring colLower = result.columnNames[i];
                std::transform(colLower.begin(), colLower.end(), colLower.begin(), ::towlower);
                if (colLower == L"name" || colLower == L"nome" || colLower == L"title" || 
                    colLower == L"titulo" || colLower == L"descricao" || colLower == L"description") {
                    displayColIndex = (int)i + 1;  // +1 porque rowid é a primeira coluna
                    break;
                }
            }
            if (displayColIndex == 0 && result.columnNames.size() > 0) {
                displayColIndex = 1;  // Usar primeira coluna após rowid
            }
            
            for (size_t rowIdx = 0; rowIdx < result.rows.size(); rowIdx++) {
                const auto& row = result.rows[rowIdx];
                if (row.empty()) continue;
                
                // Extrair rowid (primeira coluna)
                int64_t rowid = 0;
                if (row[0].type == CellValue::Type::Integer) {
                    rowid = row[0].intValue;
                } else if (row[0].type == CellValue::Type::Text) {
                    rowid = _wtoi64(row[0].textValue.c_str());
                } else {
                    rowid = (int64_t)rowIdx + 1;  // Fallback
                }
                
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
                
                // Formatar: "RowID - valor"
                wchar_t rowName[256];
                if (displayName.empty()) {
                    StringCchPrintfW(rowName, 256, L"Row %lld", rowid);
                } else {
                    StringCchPrintfW(rowName, 256, L"%lld - %s", rowid, displayName.c_str());
                }
                
                PITEMID_CHILD pidl = folder_->CreateRowItemID(rowid, rowName, tableName);
                if (pidl) {
                    items_.push_back(pidl);
                }
            }
            
            SQLITEVIEW_LOG(L"EnumIDList::Initialize found %d rows in table '%s'", 
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
                (int)view.columns.size()
            );
            if (pidl) {
                items_.push_back(pidl);
            }
        }
    }

    SQLITEVIEW_LOG(L"EnumIDList::Initialize found %d tables/views", (int)items_.size());
}

STDMETHODIMP EnumIDList::Next(ULONG celt, PITEMID_CHILD* rgelt, ULONG* pceltFetched) {
    if (!rgelt) return E_POINTER;

    Initialize();

    SQLITEVIEW_LOG(L"EnumIDList::Next requested=%lu available=%zu currentIndex=%zu", 
        celt, items_.size(), currentIndex_);

    ULONG fetched = 0;
    while (fetched < celt && currentIndex_ < items_.size()) {
        rgelt[fetched] = ILCloneChild(items_[currentIndex_]);
        if (rgelt[fetched]) {
            fetched++;
        }
        currentIndex_++;
    }

    SQLITEVIEW_LOG(L"EnumIDList::Next returning %lu items", fetched);

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

} // namespace SQLiteView
