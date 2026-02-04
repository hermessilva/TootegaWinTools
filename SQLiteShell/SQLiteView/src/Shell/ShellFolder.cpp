/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** Shell Folder Implementation
*/

#include "ShellFolder.h"
#include "PreviewHandler.h"
#include "ContextMenu.h"
#include "PropertyHandler.h"
#include "IconHandler.h"
#include <algorithm>
#include <cstdio>

// Shell definitions that may be missing
#ifndef PID_STG_NAME
#define PID_STG_NAME 10
#endif

#ifndef PID_STG_STORAGETYPE
#define PID_STG_STORAGETYPE 4
#endif

#ifndef PID_STG_SIZE
#define PID_STG_SIZE 12
#endif

// SFVM messages
#ifndef SFVM_DEFVIEWMODE
#define SFVM_DEFVIEWMODE 27
#endif

#ifndef SFVM_WINDOWCREATED
#define SFVM_WINDOWCREATED 15
#endif

// FMTID_Storage GUID
static const GUID LOCAL_FMTID_Storage =
{ 0xB725F130, 0x47EF, 0x101A, { 0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC } };

// Helper to fill STRRET
static HRESULT SetStrRet(STRRET* psr, const WCHAR* psz) {
    if (!psr || !psz) return E_POINTER;
    psr->uType = STRRET_WSTR;
    return SHStrDupW(psz, &psr->pOleStr);
}

namespace SQLiteView {

    // ============================================================================
    // ClassFactory Implementation
    // ============================================================================

    ClassFactory::ClassFactory(REFCLSID clsid) : _RefCount(1), _Clsid(clsid) {
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
        return InterlockedIncrement(&_RefCount);
    }

    STDMETHODIMP_(ULONG) ClassFactory::Release() {
        LONG count = InterlockedDecrement(&_RefCount);
        if (count == 0) delete this;
        return count;
    }

    STDMETHODIMP ClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;

        if (pUnkOuter) return CLASS_E_NOAGGREGATION;

        wchar_t clsidStr[64], riidStr[64];
        StringFromGUID2(_Clsid, clsidStr, 64);
        StringFromGUID2(riid, riidStr, 64);
        SQLITEVIEW_LOG(L"ClassFactory::CreateInstance CLSID=%s riid=%s", clsidStr, riidStr);

        HRESULT hr = E_OUTOFMEMORY;

        if (IsEqualCLSID(_Clsid, CLSID_SQLiteViewFolder)) {
            SQLITEVIEW_LOG(L"  -> Creating ShellFolder instance");
            ShellFolder* folder = new (std::nothrow) ShellFolder();
            if (folder) {
                hr = folder->QueryInterface(riid, ppv);
                folder->Release();
            }
        }
        else if (IsEqualCLSID(_Clsid, CLSID_SQLiteViewPreview)) {
            SQLITEVIEW_LOG(L"  -> Creating PreviewHandler instance");
            PreviewHandler* preview = new (std::nothrow) PreviewHandler();
            if (preview) {
                hr = preview->QueryInterface(riid, ppv);
                preview->Release();
            }
        }
        else if (IsEqualCLSID(_Clsid, CLSID_SQLiteViewContextMenu)) {
            SQLITEVIEW_LOG(L"  -> Creating ContextMenu instance");
            ContextMenuHandler* menu = new (std::nothrow) ContextMenuHandler();
            if (menu) {
                hr = menu->QueryInterface(riid, ppv);
                menu->Release();
            }
        }
        else if (IsEqualCLSID(_Clsid, CLSID_SQLiteViewProperty)) {
            SQLITEVIEW_LOG(L"  -> Creating PropertyHandler instance");
            PropertyHandler* prop = new (std::nothrow) PropertyHandler();
            if (prop) {
                hr = prop->QueryInterface(riid, ppv);
                prop->Release();
            }
        }
        else if (IsEqualCLSID(_Clsid, CLSID_SQLiteViewIcon)) {
            SQLITEVIEW_LOG(L"  -> Creating IconHandler instance");
            IconHandler* icon = new (std::nothrow) IconHandler();
            if (icon) {
                hr = icon->QueryInterface(riid, ppv);
                icon->Release();
            }
        }
        else {
            hr = CLASS_E_CLASSNOTAVAILABLE;
        }

        return hr;
    }

    STDMETHODIMP ClassFactory::LockServer(BOOL fLock) {
        extern LONG g_DllRefCount;
        if (fLock) InterlockedIncrement(&g_DllRefCount);
        else InterlockedDecrement(&g_DllRefCount);
        return S_OK;
    }

    // ============================================================================
    // ShellFolder Implementation
    // ============================================================================

    ShellFolder::ShellFolder()
        : _RefCount(1)
        , _PidlRoot(nullptr)
        , _ColumnsLoaded(false)
        , _LastCachedRowID(-1) {
        SQLITEVIEW_LOG(L"ShellFolder created");
    }

    ShellFolder::~ShellFolder() {
        if (_PidlRoot) {
            CoTaskMemFree(_PidlRoot);
        }
        SQLITEVIEW_LOG(L"ShellFolder destroyed");
    }

    STDMETHODIMP ShellFolder::QueryInterface(REFIID riid, void** ppv) {
        if (!ppv) return E_POINTER;

        wchar_t guidStr[64];
        StringFromGUID2(riid, guidStr, 64);
        SQLITEVIEW_LOG(L"ShellFolder::QI riid=%s table='%s'", guidStr, _CurrentTable.c_str());

        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, IID_IShellFolder) ||
            IsEqualIID(riid, IID_IShellFolder2)) {
            *ppv = static_cast<IShellFolder2*>(this);
        }
        else if (IsEqualIID(riid, IID_IPersistFile)) {
            *ppv = static_cast<IPersistFile*>(this);
        }
        else if (IsEqualIID(riid, IID_IPersist) ||
            IsEqualIID(riid, IID_IPersistFolder) ||
            IsEqualIID(riid, IID_IPersistFolder2) ||
            IsEqualIID(riid, IID_IPersistFolder3)) {
            *ppv = static_cast<IPersistFolder3*>(this);
        }
        else if (IsEqualIID(riid, IID_IShellFolderViewCB)) {
            // Temporarily disabled to debug navigation issues
            // *ppv = static_cast<IShellFolderViewCB*>(this);
            *ppv = nullptr;
            return E_NOINTERFACE;
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

    STDMETHODIMP_(ULONG) ShellFolder::AddRef() {
        LONG count = InterlockedIncrement(&_RefCount);
        SQLITEVIEW_LOG(L"ShellFolder::AddRef -> %ld table='%s'", count, _CurrentTable.c_str());
        return count;
    }

    STDMETHODIMP_(ULONG) ShellFolder::Release() {
        LONG count = InterlockedDecrement(&_RefCount);
        SQLITEVIEW_LOG(L"ShellFolder::Release -> %ld table='%s'", count, _CurrentTable.c_str());
        if (count == 0) delete this;
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
        if (_PidlRoot) {
            CoTaskMemFree(_PidlRoot);
            _PidlRoot = nullptr;
        }

        if (pidl) {
            _PidlRoot = ILClone(pidl);
            if (!_PidlRoot) return E_OUTOFMEMORY;
        }

        // Extract database path from PIDL (only if not already set by SetDatabasePath)
        if (_DatabasePath.empty()) {
            wchar_t path[MAX_PATH];
            if (SHGetPathFromIDListW(pidl, path)) {
                _DatabasePath = path;
                SQLITEVIEW_LOG(L"ShellFolder::Initialize path=%s", path);
            }
        } else {
            SQLITEVIEW_LOG(L"ShellFolder::Initialize keeping existing path=%s", _DatabasePath.c_str());
        }

        // Check for table item in PIDL (navigation inside database)
        if (pidl) {
            PCUIDLIST_RELATIVE child = pidl;

            while (child && child->mkid.cb > 0) {
                if (child->mkid.cb >= sizeof(ItemData)) {
                    const ItemData* item = reinterpret_cast<const ItemData*>(&child->mkid);
                    if (item->signature == ItemData::SIGNATURE) {
                        if (item->type == ItemType::Table ||
                            item->type == ItemType::View ||
                            item->type == ItemType::SystemTable) {
                            _CurrentTable = item->name;
                            _RecordCache.clear();
                            _LastCachedRowID = -1;
                            _ColumnsLoaded = false;  // Reset to force column reload for new table
                            _CurrentColumns.clear();
                            SQLITEVIEW_LOG(L"ShellFolder::Initialize found table: '%s'", _CurrentTable.c_str());
                        }
                    }
                }

                child = reinterpret_cast<PCUIDLIST_RELATIVE>(
                    reinterpret_cast<const BYTE*>(child) + child->mkid.cb);
            }
        }

        return S_OK;
    }

    // IPersistFolder2
    STDMETHODIMP ShellFolder::GetCurFolder(PIDLIST_ABSOLUTE* ppidl) {
        if (!ppidl) return E_POINTER;

        if (_PidlRoot) {
            *ppidl = ILClone(_PidlRoot);
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
            _DatabasePath = ppfti->szTargetParsingName;
        }

        return S_OK;
    }

    STDMETHODIMP ShellFolder::GetFolderTargetInfo(PERSIST_FOLDER_TARGET_INFO* ppfti) {
        if (!ppfti) return E_POINTER;

        ZeroMemory(ppfti, sizeof(*ppfti));

        if (!_DatabasePath.empty()) {
            StringCchCopyW(ppfti->szTargetParsingName, MAX_PATH, _DatabasePath.c_str());
            ppfti->dwAttributes = FILE_ATTRIBUTE_NORMAL;
            ppfti->csidl = -1;
        }

        return S_OK;
    }

    // IPersistFile
    STDMETHODIMP ShellFolder::IsDirty() {
        return S_FALSE;
    }

    STDMETHODIMP ShellFolder::Load(LPCOLESTR pszFileName, DWORD dwMode) {
        if (!pszFileName) return E_POINTER;

        _DatabasePath = pszFileName;
        SQLITEVIEW_LOG(L"ShellFolder::Load path=%s", pszFileName);

        return S_OK;
    }

    STDMETHODIMP ShellFolder::Save(LPCOLESTR pszFileName, BOOL fRemember) {
        return E_NOTIMPL;
    }

    STDMETHODIMP ShellFolder::SaveCompleted(LPCOLESTR pszFileName) {
        return S_OK;
    }

    STDMETHODIMP ShellFolder::GetCurFile(LPOLESTR* ppszFileName) {
        if (!ppszFileName) return E_POINTER;

        if (_DatabasePath.empty()) {
            *ppszFileName = nullptr;
            return S_FALSE;
        }

        return SHStrDupW(_DatabasePath.c_str(), ppszFileName);
    }

    bool ShellFolder::OpenDatabase() {
        if (_Database && _Database->IsOpen()) return true;
        if (_DatabasePath.empty()) return false;

        _Database = DatabasePool::Instance().GetDatabase(_DatabasePath);
        return _Database && _Database->IsOpen();
    }

    const DatabaseEntry* ShellFolder::GetCachedRecord(INT64 rowid) const {
        SQLITEVIEW_LOG(L"GetCachedRecord ENTER: rowid=%lld table='%s'", rowid, _CurrentTable.c_str());

        // Check cache first
        auto it = _RecordCache.find(rowid);
        if (it != _RecordCache.end()) {
            SQLITEVIEW_LOG(L"GetCachedRecord: cache hit");
            return &it->second;
        }

        // Limit cache size to prevent memory issues - clear before adding
        if (_RecordCache.size() >= 1000) {
            SQLITEVIEW_LOG(L"GetCachedRecord: clearing cache");
            _RecordCache.clear();
        }

        // Query and cache
        if (_Database && !_CurrentTable.empty()) {
            SQLITEVIEW_LOG(L"GetCachedRecord: querying DB...");
            DatabaseEntry entry = _Database->GetRecordByRowID(_CurrentTable, rowid);
            SQLITEVIEW_LOG(L"GetCachedRecord: query done, type=%d", (int)entry.Type);
            if (entry.Type != ItemType::Unknown) {
                _RecordCache[rowid] = std::move(entry);
                _LastCachedRowID = rowid;
                SQLITEVIEW_LOG(L"GetCachedRecord EXIT: cached");
                return &_RecordCache[rowid];
            }
        }

        SQLITEVIEW_LOG(L"GetCachedRecord EXIT: not found");
        return nullptr;
    }

    void ShellFolder::LoadColumns() const {
        SQLITEVIEW_LOG(L"LoadColumns: table='%s' loaded=%d", _CurrentTable.c_str(), _ColumnsLoaded ? 1 : 0);

        if (_ColumnsLoaded) {
            SQLITEVIEW_LOG(L"LoadColumns: already loaded, count=%zu", _CurrentColumns.size());
            return;
        }

        _CurrentColumns.clear();

        if (_CurrentTable.empty() || !_Database) {
            SQLITEVIEW_LOG(L"LoadColumns: skipping (table empty=%d, db null=%d)",
                _CurrentTable.empty() ? 1 : 0, _Database ? 0 : 1);
            _ColumnsLoaded = true;
            return;
        }

        _CurrentColumns = _Database->GetColumns(_CurrentTable);
        _ColumnsLoaded = true;

        SQLITEVIEW_LOG(L"LoadColumns: loaded %zu columns", _CurrentColumns.size());
    }

    UINT ShellFolder::GetColumnCount() const {
        if (_CurrentTable.empty()) {
            // At root level - showing tables
            return 4; // Name, Type, Records, Columns
        }

        // Inside a table - show dynamic columns based on table schema
        LoadColumns();
        return 2 + static_cast<UINT>(_CurrentColumns.size()); // Name, RowID, + columns
    }

    std::wstring ShellFolder::GetColumnName(UINT iColumn) const {
        if (_CurrentTable.empty()) {
            // Root level columns
            switch (iColumn) {
            case 0: return L"Name";
            case 1: return L"Type";
            case 2: return L"Records";
            case 3: return L"Columns";
            default: return L"";
            }
        }

        // Inside table - dynamic columns
        LoadColumns();
        switch (iColumn) {
        case 0: return L"ID";
        case 1: return L"RowID";
        default:
            if (iColumn - 2 < _CurrentColumns.size()) {
                return _CurrentColumns[iColumn - 2].Name;
            }
            return L"";
        }
    }

    SHCOLSTATEF ShellFolder::GetColumnFlags(UINT iColumn) const {
        if (_CurrentTable.empty()) {
            switch (iColumn) {
            case 0: return SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT;
            case 1: return SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT;
            case 2: return SHCOLSTATE_TYPE_INT | SHCOLSTATE_ONBYDEFAULT;
            case 3: return SHCOLSTATE_TYPE_INT | SHCOLSTATE_ONBYDEFAULT;
            default: return SHCOLSTATE_HIDDEN;
            }
        }

        // All columns visible for records
        return SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT;
    }

    // IShellFolder
    STDMETHODIMP ShellFolder::ParseDisplayName(HWND hwnd, IBindCtx* pbc, LPWSTR pszDisplayName,
        ULONG* pchEaten, PIDLIST_RELATIVE* ppidl, ULONG* pdwAttributes) {

        SQLITEVIEW_LOG(L"ParseDisplayName ENTER: name='%s' table='%s'", 
                       pszDisplayName ? pszDisplayName : L"(null)", _CurrentTable.c_str());

        if (!ppidl) return E_POINTER;
        *ppidl = nullptr;

        if (!pszDisplayName || !*pszDisplayName) return E_INVALIDARG;

        if (!OpenDatabase()) return E_FAIL;

        auto entries = _Database->GetEntriesInFolder(_CurrentTable);
        SQLITEVIEW_LOG(L"ParseDisplayName: searching in %zu entries", entries.size());
        
        for (const auto& entry : entries) {
            if (_wcsicmp(entry.Name.c_str(), pszDisplayName) == 0) {
                SQLITEVIEW_LOG(L"ParseDisplayName: found '%s' isTable=%d", entry.Name.c_str(), entry.IsTable() ? 1 : 0);
                *ppidl = CreateItemID(entry);
                if (pchEaten) *pchEaten = (ULONG)wcslen(pszDisplayName);
                if (pdwAttributes) {
                    if (entry.IsTable())
                        *pdwAttributes &= SFGAO_FOLDER | SFGAO_BROWSABLE | SFGAO_HASSUBFOLDER;
                    else
                        *pdwAttributes &= SFGAO_STREAM | SFGAO_CANCOPY;
                }
                SQLITEVIEW_LOG(L"ParseDisplayName EXIT: S_OK");
                return S_OK;
            }
        }

        SQLITEVIEW_LOG(L"ParseDisplayName EXIT: FILE_NOT_FOUND");
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    STDMETHODIMP ShellFolder::EnumObjects(HWND hwnd, SHCONTF grfFlags, IEnumIDList** ppenumIDList) {
        if (!ppenumIDList) return E_POINTER;
        *ppenumIDList = nullptr;

        SQLITEVIEW_LOG(L"EnumObjects ENTER: flags=0x%08X table='%s'",
            grfFlags, _CurrentTable.c_str());

        EnumIDList* enumList = new (std::nothrow) EnumIDList(this, grfFlags);
        if (!enumList) {
            SQLITEVIEW_LOG(L"EnumObjects EXIT: E_OUTOFMEMORY");
            return E_OUTOFMEMORY;
        }

        *ppenumIDList = enumList;
        SQLITEVIEW_LOG(L"EnumObjects EXIT: S_OK");
        return S_OK;
    }

    STDMETHODIMP ShellFolder::BindToObject(PCUIDLIST_RELATIVE pidl, IBindCtx* pbc, REFIID riid, void** ppv) {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;

        const ItemData* item = GetItemData(pidl);
        if (!item) return E_INVALIDARG;

        wchar_t guidStr[64];
        StringFromGUID2(riid, guidStr, 64);
        SQLITEVIEW_LOG(L"BindToObject ENTER: name=%s riid=%s type=%d", item->name, guidStr, (int)item->type);

        // Only tables can be bound to as folders
        if (item->type != ItemType::Table &&
            item->type != ItemType::View &&
            item->type != ItemType::SystemTable) {
            SQLITEVIEW_LOG(L"BindToObject EXIT: E_NOINTERFACE (not a table) type=%d", (int)item->type);
            return E_NOINTERFACE;
        }

        SQLITEVIEW_LOG(L"BindToObject: checking riid support");

        // Only create subfolder for interfaces we actually support
        // This avoids creating/destroying objects for unsupported interfaces
        bool isSupported = IsEqualIID(riid, IID_IShellFolder) ||
            IsEqualIID(riid, IID_IShellFolder2) ||
            IsEqualIID(riid, IID_IPersist) ||
            IsEqualIID(riid, IID_IPersistFolder) ||
            IsEqualIID(riid, IID_IPersistFolder2) ||
            IsEqualIID(riid, IID_IPersistFolder3) ||
            IsEqualIID(riid, IID_IUnknown);

        SQLITEVIEW_LOG(L"BindToObject: isSupported=%d", isSupported ? 1 : 0);

        if (!isSupported) {
            SQLITEVIEW_LOG(L"BindToObject EXIT: E_NOINTERFACE (unsupported riid)");
            return E_NOINTERFACE;
        }

        // Ensure database is open before creating subfolder
        if (!OpenDatabase()) {
            SQLITEVIEW_LOG(L"BindToObject EXIT: E_FAIL (cannot open database)");
            return E_FAIL;
        }

        SQLITEVIEW_LOG(L"BindToObject: creating subfolder... (parent db=%p)", _Database.get());

        // Create a new ShellFolder for the table
        // DEBUG BREAKPOINT: Set breakpoint on next line to debug navigation
        ShellFolder* subFolder = new (std::nothrow) ShellFolder();
        if (!subFolder) return E_OUTOFMEMORY;

        subFolder->SetDatabasePath(_DatabasePath);
        subFolder->SetCurrentTable(item->name);
        subFolder->SetDatabase(_Database);

        SQLITEVIEW_LOG(L"BindToObject: subfolder configured (db=%p, table='%s')",
            subFolder->_Database.get(), subFolder->_CurrentTable.c_str());

        // Initialize with combined PIDL (like 7ZipShell does)
        if (_PidlRoot) {
            PIDLIST_ABSOLUTE subPidl = ILCombine(_PidlRoot, pidl);
            if (subPidl) {
                subFolder->Initialize(subPidl);
                CoTaskMemFree(subPidl);
            }
        }

        SQLITEVIEW_LOG(L"BindToObject: subfolder table='%s'", subFolder->_CurrentTable.c_str());

        HRESULT hr = subFolder->QueryInterface(riid, ppv);
        subFolder->Release();

        SQLITEVIEW_LOG(L"BindToObject EXIT: hr=0x%08X", hr);
        return hr;
    }

    STDMETHODIMP ShellFolder::BindToStorage(PCUIDLIST_RELATIVE pidl, IBindCtx* pbc, REFIID riid, void** ppv) {
        return BindToObject(pidl, pbc, riid, ppv);
    }

    STDMETHODIMP ShellFolder::CompareIDs(LPARAM lParam, PCUIDLIST_RELATIVE pidl1, PCUIDLIST_RELATIVE pidl2) {
        static LONG s_CompareCount = 0;
        LONG count = InterlockedIncrement(&s_CompareCount);
        if (count % 1000 == 0) {
            SQLITEVIEW_LOG(L"CompareIDs: call count = %ld", count);
        }

        const ItemData* item1 = GetItemData(pidl1);
        const ItemData* item2 = GetItemData(pidl2);

        if (!item1 || !item2) return E_INVALIDARG;

        // Sort tables before records
        if (item1->type != item2->type) {
            // Tables/views first
            bool isFolder1 = (item1->type == ItemType::Table ||
                item1->type == ItemType::View ||
                item1->type == ItemType::SystemTable);
            bool isFolder2 = (item2->type == ItemType::Table ||
                item2->type == ItemType::View ||
                item2->type == ItemType::SystemTable);
            if (isFolder1 != isFolder2) {
                return MAKE_HRESULT(SEVERITY_SUCCESS, 0, isFolder1 ? (USHORT)-1 : 1);
            }
        }

        // Sort by column
        USHORT column = LOWORD(lParam);
        int result = 0;

        switch (column) {
        case 0: // Name
            result = _wcsicmp(item1->name, item2->name);
            break;
        case 1: // Type or RowID
            if (_CurrentTable.empty()) {
                result = static_cast<int>(item1->type) - static_cast<int>(item2->type);
            }
            else {
                result = (item1->rowid < item2->rowid) ? -1 : (item1->rowid > item2->rowid) ? 1 : 0;
            }
            break;
        case 2: // Records or column data
            result = (item1->recordCount < item2->recordCount) ? -1 :
                (item1->recordCount > item2->recordCount) ? 1 : 0;
            break;
        default:
            result = _wcsicmp(item1->name, item2->name);
            break;
        }

        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, static_cast<USHORT>(result));
    }

    STDMETHODIMP ShellFolder::CreateViewObject(HWND hwndOwner, REFIID riid, void** ppv) {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;

        wchar_t guidStr[64];
        StringFromGUID2(riid, guidStr, 64);
        SQLITEVIEW_LOG(L"CreateViewObject ENTER: riid=%s path='%s'", guidStr, _DatabasePath.c_str());

        // Manual GUID comparison for IShellView: {000214E3-0000-0000-C000-000000000046}
        static const GUID myIID_IShellView = { 0x000214E3, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

        if (IsEqualIID(riid, myIID_IShellView)) {
            SQLITEVIEW_LOG(L"CreateViewObject: IShellView requested, creating view");
            // Create default shell view with callback (like 7ZipShell does)
            SFV_CREATE sfvc = {};
            sfvc.cbSize = sizeof(sfvc);
            sfvc.pshf = static_cast<IShellFolder*>(this);
            sfvc.psfvcb = static_cast<IShellFolderViewCB*>(this);  // Use callback!

            HRESULT hr = SHCreateShellFolderView(&sfvc, reinterpret_cast<IShellView**>(ppv));
            SQLITEVIEW_LOG(L"CreateViewObject: SHCreateShellFolderView returned 0x%08X", hr);
            return hr;
        }

        if (IsEqualIID(riid, IID_IContextMenu)) {
            // Return this for folder background menu
            return QueryInterface(riid, ppv);
        }

        // IMPORTANT: Do NOT handle IObjectWithSite here - Explorer queries for many
        // interfaces through CreateViewObject but we should only provide view-related
        // objects. IObjectWithSite should be obtained through QueryInterface directly.
        // Returning E_NOINTERFACE for unknown interfaces is the correct behavior.

        SQLITEVIEW_LOG(L"CreateViewObject EXIT: E_NOINTERFACE");
        return E_NOINTERFACE;
    }

    STDMETHODIMP ShellFolder::GetAttributesOf(UINT cidl, PCUITEMID_CHILD_ARRAY apidl, SFGAOF* rgfInOut) {
        if (!rgfInOut) return E_POINTER;
        if (cidl == 0 || !apidl) return E_INVALIDARG;

        SFGAOF requested = *rgfInOut;
        SFGAOF result = requested;

        for (UINT i = 0; i < cidl; i++) {
            const ItemData* item = GetItemData(apidl[i]);
            if (!item) continue;

            SFGAOF attrs = 0;

            if (item->type == ItemType::Table ||
                item->type == ItemType::View ||
                item->type == ItemType::SystemTable) {
                // Tables are folders - don't use SFGAO_HASSUBFOLDER to avoid extra queries
                attrs = SFGAO_FOLDER | SFGAO_BROWSABLE;
                SQLITEVIEW_LOG(L"GetAttributesOf: '%s' type=%d -> FOLDER|BROWSABLE (0x%08X)", 
                               item->name, (int)item->type, attrs);
            }
            else {
                // Records are files
                attrs = SFGAO_STREAM | SFGAO_CANCOPY;
            }

            result &= attrs;
        }

        *rgfInOut = result;
        return S_OK;
    }

    STDMETHODIMP ShellFolder::GetUIObjectOf(HWND hwndOwner, UINT cidl, PCUITEMID_CHILD_ARRAY apidl,
        REFIID riid, UINT* rgfReserved, void** ppv) {

        if (!ppv) return E_POINTER;
        *ppv = nullptr;

        wchar_t guidStr[64];
        StringFromGUID2(riid, guidStr, 64);
        SQLITEVIEW_LOG(L"GetUIObjectOf: cidl=%u riid=%s", cidl, guidStr);

        if (cidl == 0 || !apidl) return E_INVALIDARG;

        // Handle IContextMenu for navigation and context menus
        if (IsEqualIID(riid, IID_IContextMenu) || IsEqualIID(riid, IID_IContextMenu2) || IsEqualIID(riid, IID_IContextMenu3)) {
            const ItemData* item = GetItemData(apidl[0]);
            if (!item) return E_INVALIDARG;
            
            SQLITEVIEW_LOG(L"GetUIObjectOf: IContextMenu for '%s' type=%d", item->name, (int)item->type);
            
            ContextMenuHandler* menu = new (std::nothrow) ContextMenuHandler();
            if (!menu) return E_OUTOFMEMORY;

            // Configure for navigation support
            menu->SetSite(_Site.Get());
            menu->SetFolderPIDL(_PidlRoot);
            menu->SetItemInfo(item->name, item->type);

            HRESULT hr = menu->QueryInterface(riid, ppv);
            menu->Release();
            return hr;
        }

        if (IsEqualIID(riid, IID_IDataObject)) {
            DatabaseDataObject* dataObj = new (std::nothrow) DatabaseDataObject();
            if (!dataObj) return E_OUTOFMEMORY;

            dataObj->SetDatabase(_Database);
            if (!_CurrentTable.empty()) {
                dataObj->SetTableName(_CurrentTable);
                for (UINT i = 0; i < cidl; i++) {
                    const ItemData* item = GetItemData(apidl[i]);
                    if (item && item->type == ItemType::Record) {
                        dataObj->AddRowID(item->rowid);
                    }
                }
            }

            HRESULT hr = dataObj->QueryInterface(riid, ppv);
            dataObj->Release();
            return hr;
        }

        if (IsEqualIID(riid, IID_IExtractIconW)) {
            IconHandler* icon = new (std::nothrow) IconHandler();
            if (!icon) return E_OUTOFMEMORY;

            HRESULT hr = icon->QueryInterface(riid, ppv);
            icon->Release();
            return hr;
        }

        return E_NOINTERFACE;
    }

    STDMETHODIMP ShellFolder::GetDisplayNameOf(PCUITEMID_CHILD pidl, SHGDNF uFlags, STRRET* pName) {
        if (!pName) return E_POINTER;

        const ItemData* item = GetItemData(pidl);
        if (!item) return E_INVALIDARG;

        std::wstring displayName = item->name;

        // For full parsing name, include path
        if (uFlags & SHGDN_FORPARSING) {
            if (!_CurrentTable.empty()) {
                displayName = _CurrentTable + L"/" + displayName;
            }
        }

        return SetStrRet(pName, displayName.c_str());
    }

    STDMETHODIMP ShellFolder::SetNameOf(HWND hwnd, PCUITEMID_CHILD pidl, LPCWSTR pszName,
        SHGDNF uFlags, PITEMID_CHILD* ppidlOut) {
        // Read-only - no renaming
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
        if (pSort) *pSort = 0;
        if (pDisplay) *pDisplay = 0;
        return S_OK;
    }

    STDMETHODIMP ShellFolder::GetDefaultColumnState(UINT iColumn, SHCOLSTATEF* pcsFlags) {
        if (!pcsFlags) return E_POINTER;

        if (iColumn >= GetColumnCount()) return E_INVALIDARG;

        *pcsFlags = GetColumnFlags(iColumn);
        return S_OK;
    }

    STDMETHODIMP ShellFolder::GetDetailsEx(PCUITEMID_CHILD pidl, const SHCOLUMNID* pscid, VARIANT* pv) {
        if (!pv) return E_POINTER;

        // TODO: Implement detailed property access
        return E_NOTIMPL;
    }

    STDMETHODIMP ShellFolder::GetDetailsOf(PCUITEMID_CHILD pidl, UINT iColumn, SHELLDETAILS* psd) {
        if (!psd) return E_POINTER;

        psd->fmt = LVCFMT_LEFT;
        psd->cxChar = 20;

        if (!pidl) {
            // Column header request
            UINT colCount = GetColumnCount();
            SQLITEVIEW_LOG(L"GetDetailsOf HEADER: table='%s' col=%u colCount=%u", 
                           _CurrentTable.c_str(), iColumn, colCount);
            
            if (iColumn >= colCount) return E_INVALIDARG;

            std::wstring colName = GetColumnName(iColumn);
            SQLITEVIEW_LOG(L"GetDetailsOf HEADER: name='%s'", colName.c_str());
            return SetStrRet(&psd->str, colName.c_str());
        }

        // Item details
        const ItemData* item = GetItemData(pidl);
        if (!item) return E_INVALIDARG;

        std::wstring value;

        if (_CurrentTable.empty()) {
            // Root level - showing tables
            switch (iColumn) {
            case 0: // Name
                value = item->name;
                break;
            case 1: // Type
                switch (item->type) {
                case ItemType::Table: value = L"Table"; break;
                case ItemType::View: value = L"View"; break;
                case ItemType::SystemTable: value = L"System Table"; break;
                default: value = L"Unknown"; break;
                }
                break;
            case 2: // Record count
                value = std::to_wstring(item->recordCount);
                break;
            case 3: // Column count
                value = std::to_wstring(item->columnCount);
                break;
            }
        }
        else {
            // Inside table - showing records
            SQLITEVIEW_LOG(L"GetDetailsOf: table='%s' col=%u rowid=%lld", _CurrentTable.c_str(), iColumn, item->rowid);
            switch (iColumn) {
            case 0: // Display name (primary key or Row_N)
                value = item->name;
                break;
            case 1: // RowID
                value = std::to_wstring(item->rowid);
                break;
            default: {
                // Get column value from cached record data
                LoadColumns();
                if (item->rowid >= 0 && iColumn - 2 < _CurrentColumns.size()) {
                    const DatabaseEntry* entry = GetCachedRecord(item->rowid);
                    if (entry) {
                        auto it = entry->RecordData.find(_CurrentColumns[iColumn - 2].Name);
                        if (it != entry->RecordData.end())
                            value = it->second;
                    }
                }
                break;
            }
            }
        }

        return SetStrRet(&psd->str, value.c_str());
    }

    STDMETHODIMP ShellFolder::MapColumnToSCID(UINT iColumn, SHCOLUMNID* pscid) {
        SQLITEVIEW_LOG(L"MapColumnToSCID: iColumn=%u table='%s'", iColumn, _CurrentTable.c_str());

        if (!pscid) {
            SQLITEVIEW_LOG(L"MapColumnToSCID EXIT: E_POINTER");
            return E_POINTER;
        }

        // Check column bounds
        UINT colCount = GetColumnCount();
        SQLITEVIEW_LOG(L"MapColumnToSCID: colCount=%u", colCount);

        if (iColumn >= colCount) {
            SQLITEVIEW_LOG(L"MapColumnToSCID EXIT: E_INVALIDARG (iColumn >= colCount)");
            return E_INVALIDARG;
        }

        // Use storage property IDs for standard columns
        pscid->fmtid = LOCAL_FMTID_Storage;

        switch (iColumn) {
        case 0: pscid->pid = PID_STG_NAME; break;
        default: pscid->pid = iColumn + 100; break;
        }

        SQLITEVIEW_LOG(L"MapColumnToSCID EXIT: S_OK pid=%u", pscid->pid);
        return S_OK;
    }

    // IShellFolderViewCB
    STDMETHODIMP ShellFolder::MessageSFVCB(UINT uMsg, WPARAM wParam, LPARAM lParam) {
        SQLITEVIEW_LOG(L"MessageSFVCB: uMsg=%u wParam=%llu lParam=%lld", uMsg, (unsigned long long)wParam, (long long)lParam);

        switch (uMsg) {
        case SFVM_DEFVIEWMODE:
            // Default to details view
            SQLITEVIEW_LOG(L"MessageSFVCB: SFVM_DEFVIEWMODE -> FVM_DETAILS");
            *reinterpret_cast<FOLDERVIEWMODE*>(lParam) = FVM_DETAILS;
            return S_OK;

        case SFVM_WINDOWCREATED:
            SQLITEVIEW_LOG(L"MessageSFVCB: SFVM_WINDOWCREATED hwnd=%p", (void*)lParam);
            return S_OK;
        }

        return E_NOTIMPL;
    }

    // IObjectWithSite
    STDMETHODIMP ShellFolder::SetSite(IUnknown* pUnkSite) {
        _Site = pUnkSite;
        return S_OK;
    }

    STDMETHODIMP ShellFolder::GetSite(REFIID riid, void** ppvSite) {
        if (!ppvSite) return E_POINTER;

        if (!_Site) {
            *ppvSite = nullptr;
            return E_FAIL;
        }

        return _Site->QueryInterface(riid, ppvSite);
    }

    // Internal methods
    const ItemData* ShellFolder::GetItemData(PCUITEMID_CHILD pidl) {
        if (!pidl || pidl->mkid.cb < sizeof(ItemData)) return nullptr;

        const ItemData* item = reinterpret_cast<const ItemData*>(&pidl->mkid);
        if (item->signature != ItemData::SIGNATURE) return nullptr;

        return item;
    }

    PITEMID_CHILD ShellFolder::CreateItemID(const DatabaseEntry& entry) {
        return CreateItemID(
            entry.Name,
            entry.Type,
            entry.FullPath,
            entry.RowID,
            entry.RecordCount,
            entry.ColumnCount,
            entry.ModifiedTime
        );
    }

    PITEMID_CHILD ShellFolder::CreateItemID(const std::wstring& name, ItemType type,
        const std::wstring& path, INT64 rowid,
        INT64 recordCount, INT64 columnCount,
        FILETIME mtime) {
        // Calculate size
        UINT cb = sizeof(ItemData);
        UINT totalSize = cb + sizeof(USHORT); // Include terminator

        PITEMID_CHILD pidl = static_cast<PITEMID_CHILD>(CoTaskMemAlloc(totalSize));
        if (!pidl) return nullptr;

        ZeroMemory(pidl, totalSize);

        ItemData* item = reinterpret_cast<ItemData*>(&pidl->mkid);
        item->cb = static_cast<USHORT>(cb);
        item->signature = ItemData::SIGNATURE;
        item->type = type;
        item->rowid = rowid;
        item->recordCount = recordCount;
        item->columnCount = columnCount;
        item->modifiedTime = mtime;

        StringCchCopyW(item->name, ARRAYSIZE(item->name), name.c_str());
        StringCchCopyW(item->path, ARRAYSIZE(item->path), path.c_str());

        return pidl;
    }

    // ============================================================================
    // EnumIDList Implementation
    // ============================================================================

    EnumIDList::EnumIDList(ShellFolder* folder, SHCONTF flags)
        : _RefCount(1)
        , _Folder(folder)
        , _Flags(flags)
        , _CurrentIndex(0)
        , _Initialized(false) {

        if (_Folder) _Folder->AddRef();
    }

    EnumIDList::~EnumIDList() {
        for (auto pidl : _Items) {
            if (pidl) CoTaskMemFree(pidl);
        }

        if (_Folder) _Folder->Release();
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
        return InterlockedIncrement(&_RefCount);
    }

    STDMETHODIMP_(ULONG) EnumIDList::Release() {
        LONG count = InterlockedDecrement(&_RefCount);
        if (count == 0) delete this;
        return count;
    }

    void EnumIDList::Initialize() {
        if (_Initialized) return;
        _Initialized = true;

        SQLITEVIEW_LOG(L"EnumIDList::Initialize ENTER");

        if (!_Folder || !_Folder->OpenDatabase()) {
            SQLITEVIEW_LOG(L"EnumIDList::Initialize EXIT: no folder or db");
            return;
        }

        bool includeFolders = (_Flags & SHCONTF_FOLDERS) != 0;
        bool includeFiles = (_Flags & SHCONTF_NONFOLDERS) != 0;

        SQLITEVIEW_LOG(L"EnumIDList::Initialize: calling GetEntriesInFolder");
        auto entries = _Folder->_Database->GetEntriesInFolder(_Folder->_CurrentTable);
        SQLITEVIEW_LOG(L"EnumIDList::Initialize: got %zu entries", entries.size());

        for (const auto& entry : entries) {
            bool isFolder = entry.IsTable();

            if ((isFolder && includeFolders) || (!isFolder && includeFiles)) {
                PITEMID_CHILD pidl = _Folder->CreateItemID(entry);
                if (pidl) {
                    _Items.push_back(pidl);
                }
            }
        }

        SQLITEVIEW_LOG(L"EnumIDList::Initialize EXIT: %zu items", _Items.size());
    }

    STDMETHODIMP EnumIDList::Next(ULONG celt, PITEMID_CHILD* rgelt, ULONG* pceltFetched) {
        if (!rgelt) return E_POINTER;

        Initialize();

        ULONG fetched = 0;

        while (fetched < celt && _CurrentIndex < _Items.size()) {
            rgelt[fetched] = ILCloneChild(_Items[_CurrentIndex]);
            if (!rgelt[fetched]) break;

            _CurrentIndex++;
            fetched++;
        }

        if (pceltFetched) *pceltFetched = fetched;

        return (fetched == celt) ? S_OK : S_FALSE;
    }

    STDMETHODIMP EnumIDList::Skip(ULONG celt) {
        Initialize();
        _CurrentIndex += celt;
        return (_CurrentIndex <= _Items.size()) ? S_OK : S_FALSE;
    }

    STDMETHODIMP EnumIDList::Reset() {
        _CurrentIndex = 0;
        return S_OK;
    }

    STDMETHODIMP EnumIDList::Clone(IEnumIDList** ppenum) {
        if (!ppenum) return E_POINTER;

        EnumIDList* clone = new (std::nothrow) EnumIDList(_Folder, _Flags);
        if (!clone) return E_OUTOFMEMORY;

        clone->_CurrentIndex = _CurrentIndex;

        *ppenum = clone;
        return S_OK;
    }

    // ============================================================================
    // DatabaseDataObject Implementation
    // ============================================================================

    DatabaseDataObject::DatabaseDataObject()
        : _RefCount(1)
        , _AllRecords(false) {
    }

    DatabaseDataObject::~DatabaseDataObject() {
    }

    STDMETHODIMP DatabaseDataObject::QueryInterface(REFIID riid, void** ppv) {
        if (!ppv) return E_POINTER;

        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDataObject)) {
            *ppv = static_cast<IDataObject*>(this);
            AddRef();
            return S_OK;
        }

        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) DatabaseDataObject::AddRef() {
        return InterlockedIncrement(&_RefCount);
    }

    STDMETHODIMP_(ULONG) DatabaseDataObject::Release() {
        LONG count = InterlockedDecrement(&_RefCount);
        if (count == 0) delete this;
        return count;
    }

    std::wstring DatabaseDataObject::GenerateCSVData() const {
        std::wstring result;

        if (!_Database || _TableName.empty()) return result;

        auto columns = _Database->GetColumns(_TableName);

        // Header
        for (size_t i = 0; i < columns.size(); i++) {
            if (i > 0) result += L",";
            result += L"\"" + columns[i].Name + L"\"";
        }
        result += L"\n";

        // Data
        for (INT64 rowid : _RowIDs) {
            DatabaseEntry entry = _Database->GetRecordByRowID(_TableName, rowid);
            for (size_t i = 0; i < columns.size(); i++) {
                if (i > 0) result += L",";
                auto it = entry.RecordData.find(columns[i].Name);
                if (it != entry.RecordData.end()) {
                    std::wstring value = it->second;
                    // Escape quotes
                    size_t pos = 0;
                    while ((pos = value.find(L'"', pos)) != std::wstring::npos) {
                        value.replace(pos, 1, L"\"\"");
                        pos += 2;
                    }
                    result += L"\"" + value + L"\"";
                }
            }
            result += L"\n";
        }

        return result;
    }

    std::wstring DatabaseDataObject::GenerateJSONData() const {
        std::wstring result = L"[\n";

        if (!_Database || _TableName.empty()) {
            result += L"]";
            return result;
        }

        bool first = true;
        for (INT64 rowid : _RowIDs) {
            std::wstring json;
            if (_Database->ExportRecordToJSON(_TableName, rowid, json)) {
                if (!first) result += L",\n";
                first = false;
                result += json;
            }
        }

        result += L"\n]";
        return result;
    }

    std::wstring DatabaseDataObject::GenerateSQLData() const {
        std::wstring result;

        if (!_Database || _TableName.empty()) return result;

        auto columns = _Database->GetColumns(_TableName);

        for (INT64 rowid : _RowIDs) {
            DatabaseEntry entry = _Database->GetRecordByRowID(_TableName, rowid);

            result += L"INSERT INTO \"" + _TableName + L"\" (";

            bool first = true;
            for (const auto& col : columns) {
                if (!first) result += L", ";
                first = false;
                result += L"\"" + col.Name + L"\"";
            }

            result += L") VALUES (";

            first = true;
            for (const auto& col : columns) {
                if (!first) result += L", ";
                first = false;

                auto it = entry.RecordData.find(col.Name);
                if (it != entry.RecordData.end()) {
                    if (it->second == L"NULL") {
                        result += L"NULL";
                    }
                    else {
                        // Escape single quotes
                        std::wstring value = it->second;
                        size_t pos = 0;
                        while ((pos = value.find(L'\'', pos)) != std::wstring::npos) {
                            value.replace(pos, 1, L"''");
                            pos += 2;
                        }
                        result += L"'" + value + L"'";
                    }
                }
                else {
                    result += L"NULL";
                }
            }

            result += L");\n";
        }

        return result;
    }

    STDMETHODIMP DatabaseDataObject::GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) {
        if (!pformatetcIn || !pmedium) return E_POINTER;

        ZeroMemory(pmedium, sizeof(*pmedium));

        // Support text formats
        if (pformatetcIn->cfFormat == CF_UNICODETEXT || pformatetcIn->cfFormat == CF_TEXT) {
            std::wstring data = GenerateCSVData();
            if (data.empty()) return DV_E_FORMATETC;

            // Allocate global memory
            SIZE_T size = (data.length() + 1) * sizeof(wchar_t);
            HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
            if (!hGlobal) return E_OUTOFMEMORY;

            void* pData = GlobalLock(hGlobal);
            if (!pData) {
                GlobalFree(hGlobal);
                return E_OUTOFMEMORY;
            }

            memcpy(pData, data.c_str(), size);
            GlobalUnlock(hGlobal);

            pmedium->tymed = TYMED_HGLOBAL;
            pmedium->hGlobal = hGlobal;
            pmedium->pUnkForRelease = nullptr;

            return S_OK;
        }

        return DV_E_FORMATETC;
    }

    STDMETHODIMP DatabaseDataObject::GetDataHere(FORMATETC* pformatetc, STGMEDIUM* pmedium) {
        return E_NOTIMPL;
    }

    STDMETHODIMP DatabaseDataObject::QueryGetData(FORMATETC* pformatetc) {
        if (!pformatetc) return E_POINTER;

        if (pformatetc->cfFormat == CF_UNICODETEXT || pformatetc->cfFormat == CF_TEXT) {
            return S_OK;
        }

        return DV_E_FORMATETC;
    }

    STDMETHODIMP DatabaseDataObject::GetCanonicalFormatEtc(FORMATETC* pformatetcIn, FORMATETC* pformatetcOut) {
        if (!pformatetcOut) return E_POINTER;
        *pformatetcOut = *pformatetcIn;
        pformatetcOut->ptd = nullptr;
        return DATA_S_SAMEFORMATETC;
    }

    STDMETHODIMP DatabaseDataObject::SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease) {
        return E_NOTIMPL;
    }

    STDMETHODIMP DatabaseDataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc) {
        return E_NOTIMPL;
    }

    STDMETHODIMP DatabaseDataObject::DAdvise(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink, DWORD* pdwConnection) {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    STDMETHODIMP DatabaseDataObject::DUnadvise(DWORD dwConnection) {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    STDMETHODIMP DatabaseDataObject::EnumDAdvise(IEnumSTATDATA** ppenumAdvise) {
        return OLE_E_ADVISENOTSUPPORTED;
    }

} // namespace SQLiteView
