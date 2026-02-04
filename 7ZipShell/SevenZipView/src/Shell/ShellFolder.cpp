/*
** SevenZipView - Windows Explorer Shell Extension
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

namespace SevenZipView {

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
    SEVENZIPVIEW_LOG(L"ClassFactory::CreateInstance CLSID=%s riid=%s", clsidStr, riidStr);

    HRESULT hr = E_OUTOFMEMORY;

    if (IsEqualCLSID(_Clsid, CLSID_SevenZipViewFolder)) {
        SEVENZIPVIEW_LOG(L"  -> Creating ShellFolder instance");
        ShellFolder* folder = new (std::nothrow) ShellFolder();
        if (folder) {
            hr = folder->QueryInterface(riid, ppv);
            folder->Release();
        }
    }
    else if (IsEqualCLSID(_Clsid, CLSID_SevenZipViewPreview)) {
        SEVENZIPVIEW_LOG(L"  -> Creating PreviewHandler instance");
        PreviewHandler* preview = new (std::nothrow) PreviewHandler();
        if (preview) {
            hr = preview->QueryInterface(riid, ppv);
            preview->Release();
        }
    }
    else if (IsEqualCLSID(_Clsid, CLSID_SevenZipViewContextMenu)) {
        SEVENZIPVIEW_LOG(L"  -> Creating ContextMenu instance");
        ArchiveContextMenuHandler* menu = new (std::nothrow) ArchiveContextMenuHandler();
        if (menu) {
            hr = menu->QueryInterface(riid, ppv);
            menu->Release();
        }
    }
    else if (IsEqualCLSID(_Clsid, CLSID_SevenZipViewProperty)) {
        SEVENZIPVIEW_LOG(L"  -> Creating PropertyHandler instance");
        PropertyHandler* prop = new (std::nothrow) PropertyHandler();
        if (prop) {
            hr = prop->QueryInterface(riid, ppv);
            prop->Release();
        }
    }
    else if (IsEqualCLSID(_Clsid, CLSID_SevenZipViewIcon)) {
        SEVENZIPVIEW_LOG(L"  -> Creating IconHandler instance");
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

ShellFolder::ShellFolder() : _RefCount(1), _PidlRoot(nullptr) {
    SEVENZIPVIEW_LOG(L"ShellFolder created");
}

ShellFolder::~ShellFolder() {
    if (_PidlRoot) {
        CoTaskMemFree(_PidlRoot);
    }
    SEVENZIPVIEW_LOG(L"ShellFolder destroyed");
}

STDMETHODIMP ShellFolder::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    wchar_t iidStr[64];
    StringFromGUID2(riid, iidStr, 64);
    SEVENZIPVIEW_LOG(L"ShellFolder::QueryInterface requested: %s", iidStr);

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_IShellFolder) ||
        IsEqualIID(riid, IID_IShellFolder2)) {
        SEVENZIPVIEW_LOG(L"  -> Returning IShellFolder2");
        *ppv = static_cast<IShellFolder2*>(this);
    }
    else if (IsEqualIID(riid, IID_IPersistFile)) {
        SEVENZIPVIEW_LOG(L"  -> Returning IPersistFile");
        *ppv = static_cast<IPersistFile*>(this);
    }
    else if (IsEqualIID(riid, IID_IPersist) ||
             IsEqualIID(riid, IID_IPersistFolder) ||
             IsEqualIID(riid, IID_IPersistFolder2) ||
             IsEqualIID(riid, IID_IPersistFolder3)) {
        SEVENZIPVIEW_LOG(L"  -> Returning IPersistFolder3");
        *ppv = static_cast<IPersistFolder3*>(this);
    }
    else if (IsEqualIID(riid, IID_IShellFolderViewCB)) {
        SEVENZIPVIEW_LOG(L"  -> Returning IShellFolderViewCB");
        *ppv = static_cast<IShellFolderViewCB*>(this);
    }
    else if (IsEqualIID(riid, IID_IObjectWithSite)) {
        SEVENZIPVIEW_LOG(L"  -> Returning IObjectWithSite");
        *ppv = static_cast<IObjectWithSite*>(this);
    }
    else {
        SEVENZIPVIEW_LOG(L"  -> E_NOINTERFACE");
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) ShellFolder::AddRef() {
    return InterlockedIncrement(&_RefCount);
}

STDMETHODIMP_(ULONG) ShellFolder::Release() {
    LONG count = InterlockedDecrement(&_RefCount);
    if (count == 0) delete this;
    return count;
}

// IPersist
STDMETHODIMP ShellFolder::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    *pClassID = CLSID_SevenZipViewFolder;
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

    // Extract archive path from PIDL
    wchar_t path[MAX_PATH];
    if (SHGetPathFromIDListW(pidl, path)) {
        _ArchivePath = path;
        SEVENZIPVIEW_LOG(L"ShellFolder::Initialize path=%s", path);
    }

    // Check for subfolder item in PIDL
    if (pidl) {
        PCUIDLIST_RELATIVE child = pidl;
        int itemCount = 0;
        
        while (child && child->mkid.cb > 0) {
            itemCount++;
            
            if (child->mkid.cb >= sizeof(ItemData)) {
                const ItemData* item = reinterpret_cast<const ItemData*>(&child->mkid);
                if (item->signature == ItemData::SIGNATURE) {
                    if (item->type == ItemType::Folder) {
                        _CurrentFolder = item->path;
                        SEVENZIPVIEW_LOG(L"ShellFolder::Initialize found folder: '%s'", _CurrentFolder.c_str());
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
        _ArchivePath = ppfti->szTargetParsingName;
    }

    return S_OK;
}

STDMETHODIMP ShellFolder::GetFolderTargetInfo(PERSIST_FOLDER_TARGET_INFO* ppfti) {
    if (!ppfti) return E_POINTER;
    
    ZeroMemory(ppfti, sizeof(*ppfti));
    
    if (!_ArchivePath.empty()) {
        StringCchCopyW(ppfti->szTargetParsingName, MAX_PATH, _ArchivePath.c_str());
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
    
    _ArchivePath = pszFileName;
    SEVENZIPVIEW_LOG(L"ShellFolder::Load path=%s", pszFileName);
    
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
    
    if (_ArchivePath.empty()) {
        *ppszFileName = nullptr;
        return S_FALSE;
    }
    
    return SHStrDupW(_ArchivePath.c_str(), ppszFileName);
}

bool ShellFolder::OpenArchive() {
    if (_Archive && _Archive->IsOpen()) return true;
    if (_ArchivePath.empty()) return false;
    
    _Archive = ArchivePool::Instance().GetArchive(_ArchivePath);
    return _Archive && _Archive->IsOpen();
}

// IShellFolder
STDMETHODIMP ShellFolder::ParseDisplayName(HWND hwnd, IBindCtx* pbc, LPWSTR pszDisplayName,
    ULONG* pchEaten, PIDLIST_RELATIVE* ppidl, ULONG* pdwAttributes) {
    
    if (!ppidl) return E_POINTER;
    *ppidl = nullptr;
    
    if (!pszDisplayName || !*pszDisplayName) return E_INVALIDARG;
    
    if (!OpenArchive()) return E_FAIL;

    auto entries = _Archive->GetEntriesInFolder(_CurrentFolder);
    for (const auto& entry : entries) {
        if (_wcsicmp(entry.Name.c_str(), pszDisplayName) == 0) {
            *ppidl = CreateItemID(entry);
            if (pchEaten) *pchEaten = (ULONG)wcslen(pszDisplayName);
            if (pdwAttributes) {
                if (entry.Type == ItemType::Folder)
                    *pdwAttributes &= SFGAO_FOLDER | SFGAO_BROWSABLE | SFGAO_HASSUBFOLDER;
                else
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

    SEVENZIPVIEW_LOG(L"EnumObjects: flags=0x%08X folder='%s'", 
        grfFlags, _CurrentFolder.c_str());

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
    
    wchar_t guidStr[64];
    StringFromGUID2(riid, guidStr, 64);
    SEVENZIPVIEW_LOG(L"BindToObject: item='%s' type=%d IID=%s", item->name, (int)item->type, guidStr);
    
    // Folders are navigable
    if (item->type == ItemType::Folder) {
        if (IsEqualIID(riid, IID_IShellFolder) || IsEqualIID(riid, IID_IShellFolder2)) {
            ShellFolder* subfolder = new (std::nothrow) ShellFolder();
            if (!subfolder) return E_OUTOFMEMORY;
            
            subfolder->SetArchivePath(_ArchivePath);
            subfolder->SetCurrentFolder(item->path);
            
            // Share archive instance for performance
            if (_Archive) {
                subfolder->SetArchive(_Archive);
            }
            
            // Create the combined PIDL for this subfolder
            if (_PidlRoot) {
                PIDLIST_ABSOLUTE subPidl = ILCombine(_PidlRoot, pidl);
                if (subPidl) {
                    subfolder->Initialize(subPidl);
                    CoTaskMemFree(subPidl);
                }
            }
            
            SEVENZIPVIEW_LOG(L"BindToObject: Creating subfolder for '%s'", item->path);
            
            HRESULT hr = subfolder->QueryInterface(riid, ppv);
            subfolder->Release();
            return hr;
        }
    }
    
    return E_NOTIMPL;
}

STDMETHODIMP ShellFolder::BindToStorage(PCUIDLIST_RELATIVE pidl, IBindCtx* pbc, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    
    wchar_t guidStr[64];
    StringFromGUID2(riid, guidStr, 64);
    SEVENZIPVIEW_LOG(L"BindToStorage: IID=%s", guidStr);
    
    const ItemData* item = GetItemData(pidl);
    if (!item) return E_INVALIDARG;
    
    // For folders, delegate to BindToObject
    if (item->type == ItemType::Folder) {
        return BindToObject(pidl, pbc, riid, ppv);
    }
    
    // For files, return an IStream if requested
    if (IsEqualIID(riid, IID_IStream)) {
        // Don't try to extract synthetic folders
        if (item->archiveIndex == ArchiveEntry::SYNTHETIC_FOLDER_INDEX) {
            return E_NOTIMPL;
        }
        
        // Open archive if needed
        if (!OpenArchive()) {
            SEVENZIPVIEW_LOG(L"BindToStorage: Failed to open archive");
            return E_FAIL;
        }
        
        // Extract file to buffer
        std::vector<BYTE> buffer;
        if (!_Archive->ExtractToBuffer(item->archiveIndex, buffer)) {
            SEVENZIPVIEW_LOG(L"BindToStorage: Failed to extract file index %u", item->archiveIndex);
            return E_FAIL;
        }
        
        // Create memory stream from buffer
        IStream* pStream = SHCreateMemStream(buffer.data(), static_cast<UINT>(buffer.size()));
        if (!pStream) {
            SEVENZIPVIEW_LOG(L"BindToStorage: Failed to create memory stream");
            return E_OUTOFMEMORY;
        }
        
        SEVENZIPVIEW_LOG(L"BindToStorage: Created IStream for '%s' (%zu bytes)", item->name, buffer.size());
        *ppv = pStream;
        return S_OK;
    }
    
    return E_NOTIMPL;
}

STDMETHODIMP ShellFolder::CompareIDs(LPARAM lParam, PCUIDLIST_RELATIVE pidl1, PCUIDLIST_RELATIVE pidl2) {
    const ItemData* item1 = GetItemData(pidl1);
    const ItemData* item2 = GetItemData(pidl2);

    if (!item1 || !item2) return E_INVALIDARG;

    int column = LOWORD(lParam);
    int result = 0;

    switch (column) {
        case 0: // Name
            result = _wcsicmp(item1->name, item2->name);
            break;
        case 1: // Type
            result = static_cast<int>(item1->type) - static_cast<int>(item2->type);
            break;
        case 2: // Size
            if (item1->size < item2->size) result = -1;
            else if (item1->size > item2->size) result = 1;
            break;
        case 3: // Compressed size
            if (item1->compressedSize < item2->compressedSize) result = -1;
            else if (item1->compressedSize > item2->compressedSize) result = 1;
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

    wchar_t guidStr[64];
    StringFromGUID2(riid, guidStr, 64);
    SEVENZIPVIEW_LOG(L"CreateViewObject: IID=%s", guidStr);

    if (IsEqualIID(riid, IID_IShellView)) {
        SEVENZIPVIEW_LOG(L"CreateViewObject: Creating IShellView");
        
        SFV_CREATE sfvc = {};
        sfvc.cbSize = sizeof(sfvc);
        sfvc.pshf = static_cast<IShellFolder*>(this);
        sfvc.psfvcb = static_cast<IShellFolderViewCB*>(this);
        
        HRESULT hr = SHCreateShellFolderView(&sfvc, (IShellView**)ppv);
        SEVENZIPVIEW_LOG(L"CreateViewObject: SHCreateShellFolderView returned 0x%08X", hr);
        return hr;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP ShellFolder::GetAttributesOf(UINT cidl, PCUITEMID_CHILD_ARRAY apidl, SFGAOF* rgfInOut) {
    if (!rgfInOut) return E_POINTER;
    if (cidl == 0) return E_INVALIDARG;

    SFGAOF attrs = *rgfInOut;

    for (UINT i = 0; i < cidl; i++) {
        const ItemData* item = GetItemData(apidl[i]);
        if (item) {
            SFGAOF itemAttrs = 0;
            
            if (item->type == ItemType::Folder) {
                // Folders are navigable and can be copied
                // SFGAO_FOLDER - it's a folder
                // SFGAO_BROWSABLE - can be navigated into
                // SFGAO_HASSUBFOLDER - may contain subfolders
                // SFGAO_CANCOPY - can be copied (drag-drop, clipboard)
                itemAttrs = SFGAO_FOLDER | SFGAO_BROWSABLE | SFGAO_HASSUBFOLDER | SFGAO_CANCOPY;
            }
            else {
                // Files can be copied and we provide IStream via BindToStorage
                itemAttrs = SFGAO_STREAM | SFGAO_CANCOPY;
            }
            
            attrs &= itemAttrs;
        }
    }

    *rgfInOut = attrs;
    return S_OK;
}

STDMETHODIMP ShellFolder::GetUIObjectOf(HWND hwndOwner, UINT cidl, PCUITEMID_CHILD_ARRAY apidl,
    REFIID riid, UINT* rgfReserved, void** ppv) {
    
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (cidl == 0) return E_INVALIDARG;

    wchar_t guidStr[64];
    StringFromGUID2(riid, guidStr, 64);
    SEVENZIPVIEW_LOG(L"GetUIObjectOf: cidl=%u IID=%s", cidl, guidStr);

    // IDataObject - for drag-drop and clipboard
    if (IsEqualIID(riid, IID_IDataObject)) {
        SEVENZIPVIEW_LOG(L"GetUIObjectOf: Creating IDataObject for %u items", cidl);
        
        // Ensure archive is open
        if (!OpenArchive()) {
            SEVENZIPVIEW_LOG(L"GetUIObjectOf: Failed to open archive for IDataObject");
            return E_FAIL;
        }
        
        // Build list of items to extract
        std::vector<std::pair<UINT32, std::wstring>> items;
        for (UINT i = 0; i < cidl; i++) {
            const ItemData* item = GetItemData(apidl[i]);
            if (item) {
                items.push_back({item->archiveIndex, item->path});
            }
        }
        
        if (items.empty()) return E_INVALIDARG;
        
        // Create data object that extracts files on demand
        ArchiveDataObject* dataObj = new (std::nothrow) ArchiveDataObject();
        if (!dataObj) return E_OUTOFMEMORY;
        
        dataObj->SetArchive(_ArchivePath, _Archive, std::move(items));
        
        HRESULT hr = dataObj->QueryInterface(riid, ppv);
        dataObj->Release();
        return hr;
    }

    // Context menu - supports multiple items (cidl >= 1)
    if (IsEqualIID(riid, IID_IContextMenu) || IsEqualIID(riid, IID_IContextMenu2) || IsEqualIID(riid, IID_IContextMenu3)) {
        SEVENZIPVIEW_LOG(L"GetUIObjectOf: Creating IContextMenu for %u items", cidl);
        
        // Ensure archive is open
        if (!OpenArchive()) {
            return E_FAIL;
        }
        
        // Build list of items
        std::vector<std::pair<UINT32, std::wstring>> items;
        for (UINT i = 0; i < cidl; i++) {
            const ItemData* item = GetItemData(apidl[i]);
            if (item) {
                items.push_back({item->archiveIndex, item->path});
            }
        }
        
        if (items.empty()) return E_INVALIDARG;
        
        ItemContextMenuHandler* menu = new (std::nothrow) ItemContextMenuHandler();
        if (!menu) return E_OUTOFMEMORY;
        
        // Set multiple items
        menu->SetArchiveMultiple(_ArchivePath, std::move(items));
        
        // Pass folder PIDL for navigation support
        if (_PidlRoot) {
            menu->SetFolderPIDL(_PidlRoot);
        }
        
        // Pass site for IShellBrowser access (in-place navigation)
        if (_Site) {
            menu->SetSite(_Site);
        }
        
        HRESULT hr = menu->QueryInterface(riid, ppv);
        menu->Release();
        return hr;
    }

    // For single item operations only
    if (cidl != 1) return E_INVALIDARG;

    const ItemData* item = GetItemData(apidl[0]);
    if (!item) return E_INVALIDARG;

    SEVENZIPVIEW_LOG(L"GetUIObjectOf: single item='%s'", item->name);
    
    // Icon
    if (IsEqualIID(riid, IID_IExtractIconW)) {
        ItemIconExtractor* icon = new (std::nothrow) ItemIconExtractor();
        if (!icon) return E_OUTOFMEMORY;
        
        icon->SetItemInfo(item->name, item->type);
        
        HRESULT hr = icon->QueryInterface(riid, ppv);
        icon->Release();
        return hr;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP ShellFolder::GetDisplayNameOf(PCUITEMID_CHILD pidl, SHGDNF uFlags, STRRET* pName) {
    if (!pidl || !pName) return E_POINTER;

    const ItemData* item = GetItemData(pidl);
    if (!item) return E_INVALIDARG;

    SEVENZIPVIEW_LOG(L"GetDisplayNameOf: name='%s' flags=0x%08X", item->name, uFlags);

    if (uFlags & SHGDN_FORPARSING) {
        // For parsing, return full path
        if (uFlags & SHGDN_INFOLDER) {
            return SetStrRet(pName, item->name);
        } else {
            // Return full path including archive
            std::wstring fullPath = _ArchivePath + L"\\" + item->path;
            return SetStrRet(pName, fullPath.c_str());
        }
    }

    // Display name
    return SetStrRet(pName, item->name);
}

STDMETHODIMP ShellFolder::SetNameOf(HWND hwnd, PCUITEMID_CHILD pidl, LPCWSTR pszName,
    SHGDNF uFlags, PITEMID_CHILD* ppidlOut) {
    return E_NOTIMPL;  // Read-only
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
    
    switch (iColumn) {
        case 0: *pcsFlags = SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT; break;  // Name
        case 1: *pcsFlags = SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT; break;  // Type
        case 2: *pcsFlags = SHCOLSTATE_TYPE_INT | SHCOLSTATE_ONBYDEFAULT; break;  // Size
        case 3: *pcsFlags = SHCOLSTATE_TYPE_INT | SHCOLSTATE_ONBYDEFAULT; break;  // Compressed
        case 4: *pcsFlags = SHCOLSTATE_TYPE_DATE | SHCOLSTATE_ONBYDEFAULT; break; // Modified
        default: return E_INVALIDARG;
    }
    return S_OK;
}

STDMETHODIMP ShellFolder::GetDetailsEx(PCUITEMID_CHILD pidl, const SHCOLUMNID* pscid, VARIANT* pv) {
    if (!pscid || !pv) return E_POINTER;
    
    const ItemData* item = GetItemData(pidl);
    if (!item) return E_INVALIDARG;

    VariantInit(pv);

    // System columns
    if (IsEqualGUID(pscid->fmtid, FMTID_Storage)) {
        switch (pscid->pid) {
            case PID_STG_NAME:
                pv->vt = VT_BSTR;
                pv->bstrVal = SysAllocString(item->name);
                return S_OK;
            case PID_STG_SIZE:
                pv->vt = VT_UI8;
                pv->ullVal = item->size;
                return S_OK;
        }
    }

    return E_FAIL;
}

STDMETHODIMP ShellFolder::GetDetailsOf(PCUITEMID_CHILD pidl, UINT iColumn, SHELLDETAILS* psd) {
    if (!psd) return E_POINTER;

    psd->fmt = LVCFMT_LEFT;
    psd->cxChar = 20;

    // Column headers
    if (!pidl) {
        switch (iColumn) {
            case 0:
                psd->cxChar = 30;
                return SetStrRet(&psd->str, L"Name");
            case 1:
                psd->cxChar = 12;
                return SetStrRet(&psd->str, L"Type");
            case 2:
                psd->fmt = LVCFMT_RIGHT;
                psd->cxChar = 12;
                return SetStrRet(&psd->str, L"Size");
            case 3:
                psd->fmt = LVCFMT_RIGHT;
                psd->cxChar = 12;
                return SetStrRet(&psd->str, L"Compressed");
            case 4:
                psd->cxChar = 20;
                return SetStrRet(&psd->str, L"Modified");
            default:
                return E_INVALIDARG;
        }
    }

    // Item data
    const ItemData* item = GetItemData(pidl);
    if (!item) return E_INVALIDARG;

    switch (iColumn) {
        case 0: // Name
            return SetStrRet(&psd->str, item->name);
        
        case 1: // Type
            if (item->type == ItemType::Folder) {
                return SetStrRet(&psd->str, L"Folder");
            } else {
                // Get extension
                const wchar_t* ext = wcsrchr(item->name, L'.');
                if (ext) {
                    std::wstring type(ext + 1);
                    type += L" File";
                    return SetStrRet(&psd->str, type.c_str());
                }
                return SetStrRet(&psd->str, L"File");
            }
        
        case 2: // Size
            psd->fmt = LVCFMT_RIGHT;
            if (item->type == ItemType::Folder) {
                return SetStrRet(&psd->str, L"");
            }
            return SetStrRet(&psd->str, FormatFileSize(item->size).c_str());
        
        case 3: // Compressed
            psd->fmt = LVCFMT_RIGHT;
            if (item->type == ItemType::Folder || item->compressedSize == 0) {
                return SetStrRet(&psd->str, L"");
            }
            return SetStrRet(&psd->str, FormatFileSize(item->compressedSize).c_str());
        
        case 4: // Modified
            if (item->modifiedTime.dwHighDateTime != 0 || item->modifiedTime.dwLowDateTime != 0) {
                SYSTEMTIME st;
                FileTimeToSystemTime(&item->modifiedTime, &st);
                wchar_t buffer[64];
                GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, buffer, 32);
                wcscat_s(buffer, L" ");
                GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, nullptr, buffer + wcslen(buffer), 32);
                return SetStrRet(&psd->str, buffer);
            }
            return SetStrRet(&psd->str, L"");
        
        default:
            return E_INVALIDARG;
    }
}

STDMETHODIMP ShellFolder::MapColumnToSCID(UINT iColumn, SHCOLUMNID* pscid) {
    if (!pscid) return E_POINTER;

    pscid->fmtid = FMTID_Storage;
    switch (iColumn) {
        case 0: pscid->pid = PID_STG_NAME; break;
        case 1: pscid->pid = PID_STG_STORAGETYPE; break;
        case 2: pscid->pid = PID_STG_SIZE; break;
        default: return E_INVALIDARG;
    }
    return S_OK;
}

// IShellFolderViewCB
STDMETHODIMP ShellFolder::MessageSFVCB(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Log all messages for debugging
    SEVENZIPVIEW_LOG(L"MessageSFVCB: uMsg=%u wParam=%llu lParam=%lld", uMsg, (unsigned long long)wParam, (long long)lParam);
    
    switch (uMsg) {
        case SFVM_DEFVIEWMODE:
            // Default to Details view
            *(FOLDERVIEWMODE*)lParam = FVM_DETAILS;
            return S_OK;
        
        case SFVM_WINDOWCREATED:
            // Window has been created - store the hwnd
            SEVENZIPVIEW_LOG(L"MessageSFVCB: SFVM_WINDOWCREATED hwnd=%p", (void*)lParam);
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

PITEMID_CHILD ShellFolder::CreateItemID(const ArchiveEntry& entry) {
    return CreateItemID(
        entry.Name,
        entry.Type,
        entry.FullPath,
        entry.Size,
        entry.CompressedSize,
        entry.ArchiveIndex,
        entry.CRC,
        entry.Attributes,
        entry.ModifiedTime
    );
}

PITEMID_CHILD ShellFolder::CreateItemID(const std::wstring& name, ItemType type,
                                         const std::wstring& path, UINT64 size,
                                         UINT64 compressedSize, UINT32 index,
                                         UINT32 crc, UINT32 attrs, FILETIME mtime) {
    
    size_t itemSize = sizeof(ItemData);
    size_t totalSize = itemSize + sizeof(USHORT); // Terminator
    
    PITEMID_CHILD pidl = (PITEMID_CHILD)CoTaskMemAlloc(totalSize);
    if (!pidl) return nullptr;
    
    ZeroMemory(pidl, totalSize);
    
    ItemData* item = reinterpret_cast<ItemData*>(&pidl->mkid);
    item->cb = (USHORT)itemSize;
    item->signature = ItemData::SIGNATURE;
    item->type = type;
    StringCchCopyW(item->name, ARRAYSIZE(item->name), name.c_str());
    StringCchCopyW(item->path, ARRAYSIZE(item->path), path.c_str());
    item->size = size;
    item->compressedSize = compressedSize;
    item->archiveIndex = index;
    item->crc = crc;
    item->attributes = attrs;
    item->modifiedTime = mtime;
    
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
        CoTaskMemFree(pidl);
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

    if (!_Folder || !_Folder->OpenArchive()) return;

    bool includeFolders = (_Flags & SHCONTF_FOLDERS) != 0;
    bool includeFiles = (_Flags & SHCONTF_NONFOLDERS) != 0;

    SEVENZIPVIEW_LOG(L"EnumIDList::Initialize folders=%d files=%d currentFolder='%s'",
        includeFolders, includeFiles, _Folder->GetCurrentFolder().c_str());

    auto entries = _Folder->_Archive->GetEntriesInFolder(_Folder->GetCurrentFolder());
    
    for (const auto& entry : entries) {
        bool isFolder = (entry.Type == ItemType::Folder);
        
        if ((isFolder && includeFolders) || (!isFolder && includeFiles)) {
            PITEMID_CHILD pidl = _Folder->CreateItemID(entry);
            if (pidl) {
                _Items.push_back(pidl);
                SEVENZIPVIEW_LOG(L"  Added item: '%s' type=%d", entry.Name.c_str(), (int)entry.Type);
            }
        }
    }

    SEVENZIPVIEW_LOG(L"EnumIDList::Initialize found %zu items", _Items.size());
}

STDMETHODIMP EnumIDList::Next(ULONG celt, PITEMID_CHILD* rgelt, ULONG* pceltFetched) {
    if (!rgelt) return E_POINTER;
    
    Initialize();

    ULONG fetched = 0;
    while (fetched < celt && _CurrentIndex < _Items.size()) {
        rgelt[fetched] = ILCloneChild(_Items[_CurrentIndex]);
        if (!rgelt[fetched]) break;
        fetched++;
        _CurrentIndex++;
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
    
    clone->Initialize();
    clone->_CurrentIndex = _CurrentIndex;
    
    *ppenum = clone;
    return S_OK;
}

// ============================================================================
// ArchiveDataObject Implementation - For drag-drop and clipboard
// ============================================================================

ArchiveDataObject::ArchiveDataObject()
    : _RefCount(1)
    , _Extracted(false) {
    SEVENZIPVIEW_LOG(L"ArchiveDataObject created");
}

ArchiveDataObject::~ArchiveDataObject() {
    // Clean up temp files
    for (const auto& file : _ExtractedFiles) {
        DeleteFileW(file.c_str());
    }
    if (!_TempFolder.empty()) {
        RemoveDirectoryW(_TempFolder.c_str());
    }
    SEVENZIPVIEW_LOG(L"ArchiveDataObject destroyed");
}

STDMETHODIMP ArchiveDataObject::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDataObject)) {
        *ppv = static_cast<IDataObject*>(this);
        AddRef();
        return S_OK;
    }

    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ArchiveDataObject::AddRef() {
    return InterlockedIncrement(&_RefCount);
}

STDMETHODIMP_(ULONG) ArchiveDataObject::Release() {
    LONG count = InterlockedDecrement(&_RefCount);
    if (count == 0) delete this;
    return count;
}

void ArchiveDataObject::SetArchive(const std::wstring& archivePath, std::shared_ptr<Archive> archive,
                                    std::vector<std::pair<UINT32, std::wstring>>&& items) {
    _ArchivePath = archivePath;
    _Archive = archive;
    _Items = std::move(items);
    SEVENZIPVIEW_LOG(L"ArchiveDataObject::SetArchive: %zu items from '%s'", _Items.size(), archivePath.c_str());
    
    // Extract files immediately so they're ready for drag-drop
    // This is important because drop targets may call QueryGetData or GetData at any time
    ExtractToTemp();
}

bool ArchiveDataObject::ExtractToTemp() {
    SEVENZIPVIEW_LOG(L"ExtractToTemp: START - Extracted=%d items=%zu", _Extracted ? 1 : 0, _Items.size());
    
    if (_Extracted) return !_ExtractedFiles.empty();
    _Extracted = true;

    if (!_Archive || _Items.empty()) {
        SEVENZIPVIEW_LOG(L"ExtractToTemp: FAIL - no archive or items");
        return false;
    }

    // Create temp folder with hash of archive path for cache stability
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    
    // Use hash of archive path for unique folder name
    std::hash<std::wstring> hasher;
    size_t hashValue = hasher(_ArchivePath);
    
    wchar_t tempFolder[MAX_PATH];
    StringCchPrintfW(tempFolder, MAX_PATH, L"%sSevenZipView\\%zx", tempPath, hashValue);
    
    // Create base directory structure
    std::wstring baseDir = tempPath;
    baseDir += L"SevenZipView";
    CreateDirectoryW(baseDir.c_str(), nullptr);
    CreateDirectoryW(tempFolder, nullptr);
    _TempFolder = tempFolder;

    SEVENZIPVIEW_LOG(L"ExtractToTemp: extracting to '%s'", tempFolder);

    // Get all entries for folder expansion
    auto allEntries = _Archive->GetAllEntries();
    SEVENZIPVIEW_LOG(L"ExtractToTemp: allEntries count=%zu", allEntries.size());

    for (const auto& item : _Items) {
        UINT32 index = item.first;
        const std::wstring& path = item.second;
        
        SEVENZIPVIEW_LOG(L"ExtractToTemp: Processing index=%u path='%s'", index, path.c_str());

        // Check if this is a folder (synthetic or real)
        bool isFolder = false;
        if (index == ArchiveEntry::SYNTHETIC_FOLDER_INDEX) {
            isFolder = true;
            SEVENZIPVIEW_LOG(L"ExtractToTemp: SYNTHETIC folder");
        } else {
            ArchiveEntry entry;
            if (_Archive->GetEntry(index, entry)) {
                isFolder = entry.IsDirectory();
                SEVENZIPVIEW_LOG(L"ExtractToTemp: IsDirectory=%d", isFolder ? 1 : 0);
            }
        }

        if (isFolder) {
            // For folders: extract all files inside this folder
            std::wstring folderPrefix = path;
            if (!folderPrefix.empty() && folderPrefix.back() != L'\\' && folderPrefix.back() != L'/') {
                folderPrefix += L"\\";
            }
            
            // Normalize prefix
            for (auto& c : folderPrefix) {
                if (c == L'/') c = L'\\';
            }
            
            SEVENZIPVIEW_LOG(L"ExtractToTemp: Folder prefix='%s'", folderPrefix.c_str());
            
            // Get just the folder name
            std::wstring folderName = path;
            size_t pos = path.find_last_of(L"\\/");
            if (pos != std::wstring::npos) {
                folderName = path.substr(pos + 1);
            }
            
            // Create the folder in temp
            std::wstring tempFolderPath = _TempFolder + L"\\" + folderName;
            CreateDirectoryW(tempFolderPath.c_str(), nullptr);
            SEVENZIPVIEW_LOG(L"ExtractToTemp: Created folder '%s'", tempFolderPath.c_str());
            
            // Find and extract all files inside this folder
            for (const auto& entry : allEntries) {
                std::wstring entryPath = entry.FullPath;
                // Normalize path for comparison
                for (auto& c : entryPath) {
                    if (c == L'/') c = L'\\';
                }
                
                // Check if this entry is inside the folder
                if (entryPath.length() > folderPrefix.length() &&
                    _wcsnicmp(entryPath.c_str(), folderPrefix.c_str(), folderPrefix.length()) == 0) {
                    
                    if (!entry.IsDirectory() && entry.ArchiveIndex != ArchiveEntry::SYNTHETIC_FOLDER_INDEX) {
                        // Calculate relative path from the folder
                        std::wstring relativePath = entryPath.substr(folderPrefix.length());
                        
                        // Sanitize the relative path
                        for (auto& c : relativePath) {
                            if (c == L':' || c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|') {
                                c = L'_';
                            }
                        }
                        
                        std::wstring destPath = tempFolderPath + L"\\" + relativePath;
                        
                        // Create parent directories
                        size_t lastSlash = destPath.find_last_of(L'\\');
                        if (lastSlash != std::wstring::npos) {
                            std::wstring parentDir = destPath.substr(0, lastSlash);
                            for (size_t i = tempFolderPath.length() + 1; i < parentDir.length(); i++) {
                                if (parentDir[i] == L'\\') {
                                    CreateDirectoryW(parentDir.substr(0, i).c_str(), nullptr);
                                }
                            }
                            CreateDirectoryW(parentDir.c_str(), nullptr);
                        }
                        
                        // Extract the file
                        if (_Archive->ExtractToFile(entry.ArchiveIndex, destPath)) {
                            // Only add files, not folders (folders are created automatically)
                            SEVENZIPVIEW_LOG(L"  Extracted: '%s'", destPath.c_str());
                        }
                    }
                }
            }
            
            // Add folder to the list
            _ExtractedFiles.push_back(tempFolderPath);
            SEVENZIPVIEW_LOG(L"  Added folder: '%s'", tempFolderPath.c_str());
        } else {
            // For files: extract the individual file
            SEVENZIPVIEW_LOG(L"ExtractToTemp: Processing FILE index=%u", index);
            
            std::wstring safePath = path;
            
            // Normalize separators
            for (auto& c : safePath) {
                if (c == L'/') c = L'\\';
            }
            
            // Remove dangerous path traversal sequences
            size_t pos;
            while ((pos = safePath.find(L"..\\")) != std::wstring::npos) {
                safePath.erase(pos, 3);
            }
            
            // Remove leading backslashes
            while (!safePath.empty() && safePath[0] == L'\\') {
                safePath.erase(0, 1);
            }
            
            // Replace invalid characters
            for (auto& c : safePath) {
                if (c == L':' || c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|') {
                    c = L'_';
                }
            }

            std::wstring destPath = _TempFolder + L"\\" + safePath;
            SEVENZIPVIEW_LOG(L"ExtractToTemp: FILE destPath='%s'", destPath.c_str());
            
            // Create parent directories if needed
            size_t lastSlash = destPath.find_last_of(L'\\');
            if (lastSlash != std::wstring::npos) {
                std::wstring parentDir = destPath.substr(0, lastSlash);
                // Create all parent directories recursively
                for (size_t i = _TempFolder.length() + 1; i < parentDir.length(); i++) {
                    if (parentDir[i] == L'\\') {
                        CreateDirectoryW(parentDir.substr(0, i).c_str(), nullptr);
                    }
                }
                CreateDirectoryW(parentDir.c_str(), nullptr);
            }
            
            if (_Archive->ExtractToFile(index, destPath)) {
                _ExtractedFiles.push_back(destPath);
                SEVENZIPVIEW_LOG(L"ExtractToTemp: FILE extracted OK - '%s'", destPath.c_str());
            } else {
                SEVENZIPVIEW_LOG(L"ExtractToTemp: FILE extraction FAILED - index=%u to '%s'", index, destPath.c_str());
            }
        }
    }

    SEVENZIPVIEW_LOG(L"ExtractToTemp: END - extractedFiles=%zu", _ExtractedFiles.size());
    return !_ExtractedFiles.empty();
}

HGLOBAL ArchiveDataObject::CreateHDrop() {
    if (!ExtractToTemp()) return nullptr;

    // Calculate size needed for DROPFILES structure
    size_t totalSize = sizeof(DROPFILES);
    for (const auto& file : _ExtractedFiles) {
        totalSize += (file.length() + 1) * sizeof(wchar_t);
    }
    totalSize += sizeof(wchar_t);  // Double null terminator

    HGLOBAL hGlobal = GlobalAlloc(GHND, totalSize);
    if (!hGlobal) return nullptr;

    DROPFILES* df = (DROPFILES*)GlobalLock(hGlobal);
    if (!df) {
        GlobalFree(hGlobal);
        return nullptr;
    }

    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;

    wchar_t* p = (wchar_t*)((BYTE*)df + sizeof(DROPFILES));
    for (const auto& file : _ExtractedFiles) {
        wcscpy_s(p, file.length() + 1, file.c_str());
        p += file.length() + 1;
    }
    *p = L'\0';  // Double null terminator

    GlobalUnlock(hGlobal);
    return hGlobal;
}

STDMETHODIMP ArchiveDataObject::GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) {
    if (!pformatetcIn || !pmedium) return E_POINTER;

    SEVENZIPVIEW_LOG(L"ArchiveDataObject::GetData: cfFormat=%u tymed=0x%X extractedFiles=%zu", 
        pformatetcIn->cfFormat, pformatetcIn->tymed, _ExtractedFiles.size());
    
    for (const auto& f : _ExtractedFiles) {
        SEVENZIPVIEW_LOG(L"  ExtractedFile: '%s'", f.c_str());
    }

    ZeroMemory(pmedium, sizeof(*pmedium));

    // CF_HDROP - file list for drag-drop
    if (pformatetcIn->cfFormat == CF_HDROP &&
        (pformatetcIn->tymed & TYMED_HGLOBAL)) {
        
        HGLOBAL hDrop = CreateHDrop();
        if (!hDrop) {
            SEVENZIPVIEW_LOG(L"ArchiveDataObject::GetData: CreateHDrop FAILED");
            return E_FAIL;
        }

        pmedium->tymed = TYMED_HGLOBAL;
        pmedium->hGlobal = hDrop;
        pmedium->pUnkForRelease = nullptr;
        SEVENZIPVIEW_LOG(L"ArchiveDataObject::GetData: SUCCESS");
        return S_OK;
    }

    SEVENZIPVIEW_LOG(L"ArchiveDataObject::GetData: Format not supported");
    return DV_E_FORMATETC;
}

STDMETHODIMP ArchiveDataObject::GetDataHere(FORMATETC* pformatetc, STGMEDIUM* pmedium) {
    return E_NOTIMPL;
}

STDMETHODIMP ArchiveDataObject::QueryGetData(FORMATETC* pformatetc) {
    if (!pformatetc) return E_POINTER;

    SEVENZIPVIEW_LOG(L"ArchiveDataObject::QueryGetData: cfFormat=%u tymed=0x%X", 
        pformatetc->cfFormat, pformatetc->tymed);

    // We support CF_HDROP for drag-drop
    if (pformatetc->cfFormat == CF_HDROP &&
        (pformatetc->tymed & TYMED_HGLOBAL)) {
        // Make sure files are extracted
        if (_ExtractedFiles.empty() && !_Extracted) {
            return S_FALSE;  // Not ready yet
        }
        return _ExtractedFiles.empty() ? DV_E_FORMATETC : S_OK;
    }

    return DV_E_FORMATETC;
}

STDMETHODIMP ArchiveDataObject::GetCanonicalFormatEtc(FORMATETC* pformatectIn, FORMATETC* pformatetcOut) {
    if (!pformatetcOut) return E_POINTER;
    pformatetcOut->ptd = nullptr;
    return DATA_S_SAMEFORMATETC;
}

STDMETHODIMP ArchiveDataObject::SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease) {
    return E_NOTIMPL;
}

STDMETHODIMP ArchiveDataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc) {
    if (!ppenumFormatEtc) return E_POINTER;
    
    if (dwDirection != DATADIR_GET) return E_NOTIMPL;

    // Create a simple enumerator for CF_HDROP
    FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    return SHCreateStdEnumFmtEtc(1, &fmt, ppenumFormatEtc);
}

STDMETHODIMP ArchiveDataObject::DAdvise(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink, DWORD* pdwConnection) {
    return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP ArchiveDataObject::DUnadvise(DWORD dwConnection) {
    return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP ArchiveDataObject::EnumDAdvise(IEnumSTATDATA** ppenumAdvise) {
    return OLE_E_ADVISENOTSUPPORTED;
}

} // namespace SevenZipView
