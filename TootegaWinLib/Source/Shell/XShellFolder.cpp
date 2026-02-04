// ============================================================================
// XShellFolder.cpp - Shell Namespace Extension Implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "../Include/Shell/XShellFolder.h"
#include <strsafe.h>

namespace Tootega::Shell
{
    // ========================================================================
    // XPidlManager Implementation
    // ========================================================================

    PITEMID_CHILD XPidlManager::CreateItemID(const void* pData, size_t pSize)
    {
        size_t totalSize = pSize + XPidlItemBase::TERMINATOR_SIZE;
        PITEMID_CHILD pidl = static_cast<PITEMID_CHILD>(CoTaskMemAlloc(totalSize));
        if (!pidl)
            return nullptr;

        memcpy(pidl, pData, pSize);
        
        // Add terminator
        BYTE* terminator = reinterpret_cast<BYTE*>(pidl) + pSize;
        *reinterpret_cast<USHORT*>(terminator) = 0;

        return pidl;
    }

    PITEMID_CHILD XPidlManager::CloneItemID(PCUITEMID_CHILD pPidl)
    {
        if (!pPidl)
            return nullptr;

        UINT size = pPidl->mkid.cb + XPidlItemBase::TERMINATOR_SIZE;
        PITEMID_CHILD clone = static_cast<PITEMID_CHILD>(CoTaskMemAlloc(size));
        if (!clone)
            return nullptr;

        memcpy(clone, pPidl, size);
        return clone;
    }

    PIDLIST_ABSOLUTE XPidlManager::CloneAbsoluteIDList(PCIDLIST_ABSOLUTE pPidl)
    {
        if (!pPidl)
            return nullptr;

        UINT size = GetPidlSize(pPidl);
        PIDLIST_ABSOLUTE clone = static_cast<PIDLIST_ABSOLUTE>(CoTaskMemAlloc(size));
        if (!clone)
            return nullptr;

        memcpy(clone, pPidl, size);
        return clone;
    }

    PIDLIST_ABSOLUTE XPidlManager::AppendItemID(PCIDLIST_ABSOLUTE pParent, PCUITEMID_CHILD pChild)
    {
        if (!pParent || !pChild)
            return nullptr;

        UINT parentSize = GetPidlSize(pParent) - XPidlItemBase::TERMINATOR_SIZE;
        UINT childSize = pChild->mkid.cb + XPidlItemBase::TERMINATOR_SIZE;
        UINT totalSize = parentSize + childSize;

        PIDLIST_ABSOLUTE result = static_cast<PIDLIST_ABSOLUTE>(CoTaskMemAlloc(totalSize));
        if (!result)
            return nullptr;

        memcpy(result, pParent, parentSize);
        memcpy(reinterpret_cast<BYTE*>(result) + parentSize, pChild, childSize);

        return result;
    }

    UINT XPidlManager::GetPidlSize(PCUIDLIST_RELATIVE pPidl) noexcept
    {
        if (!pPidl)
            return 0;

        UINT size = 0;
        while (pPidl->mkid.cb > 0)
        {
            size += pPidl->mkid.cb;
            pPidl = GetNextItem(pPidl);
        }
        return size + XPidlItemBase::TERMINATOR_SIZE;
    }

    bool XPidlManager::IsEmpty(PCUIDLIST_RELATIVE pPidl) noexcept
    {
        return !pPidl || pPidl->mkid.cb == 0;
    }

    PCUIDLIST_RELATIVE XPidlManager::GetNextItem(PCUIDLIST_RELATIVE pPidl) noexcept
    {
        if (!pPidl || pPidl->mkid.cb == 0)
            return nullptr;
        return reinterpret_cast<PCUIDLIST_RELATIVE>(
            reinterpret_cast<const BYTE*>(pPidl) + pPidl->mkid.cb);
    }

    PCUITEMID_CHILD XPidlManager::GetLastItem(PCIDLIST_ABSOLUTE pPidl) noexcept
    {
        if (!pPidl || pPidl->mkid.cb == 0)
            return nullptr;

        PCUIDLIST_RELATIVE current = pPidl;
        PCUIDLIST_RELATIVE next = GetNextItem(current);

        while (next && next->mkid.cb > 0)
        {
            current = next;
            next = GetNextItem(current);
        }

        return reinterpret_cast<PCUITEMID_CHILD>(current);
    }

    // ========================================================================
    // XEnumIDList Implementation
    // ========================================================================

    XEnumIDList::XEnumIDList()
        : _RefCount(1)
        , _CurrentIndex(0)
    {
        XShellModule::Instance().AddRef();
    }

    XEnumIDList::~XEnumIDList()
    {
        for (auto pidl : _Items)
        {
            if (pidl)
                CoTaskMemFree(pidl);
        }
        XShellModule::Instance().Release();
    }

    STDMETHODIMP XEnumIDList::QueryInterface(REFIID pRiid, void** pPpv)
    {
        if (!pPpv)
            return E_POINTER;

        if (IsEqualIID(pRiid, IID_IUnknown) || IsEqualIID(pRiid, IID_IEnumIDList))
        {
            *pPpv = static_cast<IEnumIDList*>(this);
            AddRef();
            return S_OK;
        }

        *pPpv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) XEnumIDList::AddRef()
    {
        return InterlockedIncrement(&_RefCount);
    }

    STDMETHODIMP_(ULONG) XEnumIDList::Release()
    {
        LONG ref = InterlockedDecrement(&_RefCount);
        if (ref == 0)
            delete this;
        return ref;
    }

    STDMETHODIMP XEnumIDList::Next(ULONG pCelt, PITEMID_CHILD* pRgelt, ULONG* pPceltFetched)
    {
        if (!pRgelt)
            return E_POINTER;

        ULONG fetched = 0;

        while (fetched < pCelt && _CurrentIndex < _Items.size())
        {
            pRgelt[fetched] = XPidlManager::CloneItemID(_Items[_CurrentIndex]);
            if (!pRgelt[fetched])
            {
                // Cleanup already allocated items
                for (ULONG i = 0; i < fetched; ++i)
                {
                    CoTaskMemFree(pRgelt[i]);
                    pRgelt[i] = nullptr;
                }
                return E_OUTOFMEMORY;
            }
            ++fetched;
            ++_CurrentIndex;
        }

        if (pPceltFetched)
            *pPceltFetched = fetched;

        return (fetched == pCelt) ? S_OK : S_FALSE;
    }

    STDMETHODIMP XEnumIDList::Skip(ULONG pCelt)
    {
        _CurrentIndex += pCelt;
        if (_CurrentIndex > _Items.size())
            _CurrentIndex = _Items.size();
        return (_CurrentIndex < _Items.size()) ? S_OK : S_FALSE;
    }

    STDMETHODIMP XEnumIDList::Reset()
    {
        _CurrentIndex = 0;
        return S_OK;
    }

    STDMETHODIMP XEnumIDList::Clone(IEnumIDList** pPenum)
    {
        if (!pPenum)
            return E_POINTER;

        XEnumIDList* clone = new (std::nothrow) XEnumIDList();
        if (!clone)
            return E_OUTOFMEMORY;

        clone->_Items.reserve(_Items.size());
        for (auto pidl : _Items)
        {
            PITEMID_CHILD clonedPidl = XPidlManager::CloneItemID(pidl);
            if (!clonedPidl)
            {
                clone->Release();
                return E_OUTOFMEMORY;
            }
            clone->_Items.push_back(clonedPidl);
        }
        clone->_CurrentIndex = _CurrentIndex;

        *pPenum = clone;
        return S_OK;
    }

    void XEnumIDList::AddItem(PITEMID_CHILD pPidl)
    {
        _Items.push_back(pPidl);
    }

    void XEnumIDList::SetItems(std::vector<PITEMID_CHILD>&& pItems)
    {
        // Free existing items
        for (auto pidl : _Items)
        {
            if (pidl)
                CoTaskMemFree(pidl);
        }
        _Items = std::move(pItems);
        _CurrentIndex = 0;
    }

    // ========================================================================
    // XShellFolderBase Implementation
    // ========================================================================

    XShellFolderBase::XShellFolderBase()
        : _PidlRoot(nullptr)
    {
    }

    XShellFolderBase::~XShellFolderBase()
    {
        if (_PidlRoot)
            CoTaskMemFree(_PidlRoot);
    }

    STDMETHODIMP XShellFolderBase::QueryInterface(REFIID pRiid, void** pPpv)
    {
        if (!pPpv)
            return E_POINTER;

        if (IsEqualIID(pRiid, IID_IUnknown) ||
            IsEqualIID(pRiid, IID_IShellFolder) ||
            IsEqualIID(pRiid, IID_IShellFolder2))
        {
            *pPpv = static_cast<IShellFolder2*>(this);
        }
        else if (IsEqualIID(pRiid, IID_IPersist) ||
                 IsEqualIID(pRiid, IID_IPersistFolder) ||
                 IsEqualIID(pRiid, IID_IPersistFolder2) ||
                 IsEqualIID(pRiid, IID_IPersistFolder3))
        {
            *pPpv = static_cast<IPersistFolder3*>(this);
        }
        else if (IsEqualIID(pRiid, IID_IPersistFile))
        {
            *pPpv = static_cast<IPersistFile*>(this);
        }
        else if (IsEqualIID(pRiid, IID_IShellFolderViewCB))
        {
            *pPpv = static_cast<IShellFolderViewCB*>(this);
        }
        else if (IsEqualIID(pRiid, IID_IObjectWithSite))
        {
            *pPpv = static_cast<IObjectWithSite*>(this);
        }
        else
        {
            *pPpv = nullptr;
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    // IPersist
    STDMETHODIMP XShellFolderBase::GetClassID(CLSID* pClassID)
    {
        if (!pClassID)
            return E_POINTER;
        *pClassID = GetClassGUID();
        return S_OK;
    }

    // IPersistFolder
    STDMETHODIMP XShellFolderBase::Initialize(PCIDLIST_ABSOLUTE pPidl)
    {
        if (_PidlRoot)
        {
            CoTaskMemFree(_PidlRoot);
            _PidlRoot = nullptr;
        }

        if (pPidl)
            _PidlRoot = XPidlManager::CloneAbsoluteIDList(pPidl);

        return OnInitialize(pPidl) ? S_OK : E_FAIL;
    }

    // IPersistFolder2
    STDMETHODIMP XShellFolderBase::GetCurFolder(PIDLIST_ABSOLUTE* pPidl)
    {
        if (!pPidl)
            return E_POINTER;

        *pPidl = _PidlRoot ? XPidlManager::CloneAbsoluteIDList(_PidlRoot) : nullptr;
        return *pPidl ? S_OK : E_FAIL;
    }

    // IPersistFolder3
    STDMETHODIMP XShellFolderBase::InitializeEx(
        IBindCtx* pBc,
        PCIDLIST_ABSOLUTE pPidlRoot,
        const PERSIST_FOLDER_TARGET_INFO* pPfti)
    {
        return Initialize(pPidlRoot);
    }

    STDMETHODIMP XShellFolderBase::GetFolderTargetInfo(PERSIST_FOLDER_TARGET_INFO* pPfti)
    {
        if (!pPfti)
            return E_POINTER;

        ZeroMemory(pPfti, sizeof(*pPfti));

        if (!_FilePath.empty())
        {
            StringCchCopyW(pPfti->szTargetParsingName, ARRAYSIZE(pPfti->szTargetParsingName),
                _FilePath.c_str());
            pPfti->dwAttributes = FILE_ATTRIBUTE_NORMAL;
            pPfti->csidl = -1;
        }

        return S_OK;
    }

    // IPersistFile
    STDMETHODIMP XShellFolderBase::IsDirty()
    {
        return S_FALSE;
    }

    STDMETHODIMP XShellFolderBase::Load(LPCOLESTR pszFileName, DWORD pDwMode)
    {
        if (!pszFileName)
            return E_POINTER;

        _FilePath = pszFileName;
        return OnLoad(_FilePath) ? S_OK : E_FAIL;
    }

    STDMETHODIMP XShellFolderBase::Save(LPCOLESTR pszFileName, BOOL pFRemember)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP XShellFolderBase::SaveCompleted(LPCOLESTR pszFileName)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP XShellFolderBase::GetCurFile(LPOLESTR* ppszFileName)
    {
        if (!ppszFileName)
            return E_POINTER;

        return SHStrDupW(_FilePath.c_str(), ppszFileName);
    }

    // IShellFolder
    STDMETHODIMP XShellFolderBase::ParseDisplayName(
        HWND pHwnd,
        IBindCtx* pBc,
        LPWSTR pszDisplayName,
        ULONG* pChEaten,
        PIDLIST_RELATIVE* pPidl,
        ULONG* pDwAttributes)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP XShellFolderBase::EnumObjects(
        HWND pHwnd,
        SHCONTF pGrfFlags,
        IEnumIDList** pEnumIDList)
    {
        if (!pEnumIDList)
            return E_POINTER;

        XEnumIDList* enumList = new (std::nothrow) XEnumIDList();
        if (!enumList)
            return E_OUTOFMEMORY;

        if (!OnEnumObjects(pGrfFlags, enumList))
        {
            enumList->Release();
            return E_FAIL;
        }

        *pEnumIDList = enumList;
        return S_OK;
    }

    STDMETHODIMP XShellFolderBase::BindToObject(
        PCUIDLIST_RELATIVE pPidl,
        IBindCtx* pBc,
        REFIID pRiid,
        void** pPv)
    {
        if (!pPidl || !pPv)
            return E_POINTER;

        *pPv = nullptr;
        return OnBindToObject(pPidl, pRiid, pPv) ? S_OK : E_FAIL;
    }

    STDMETHODIMP XShellFolderBase::BindToStorage(
        PCUIDLIST_RELATIVE pPidl,
        IBindCtx* pBc,
        REFIID pRiid,
        void** pPv)
    {
        return BindToObject(pPidl, pBc, pRiid, pPv);
    }

    STDMETHODIMP XShellFolderBase::CompareIDs(
        LPARAM pLParam,
        PCUIDLIST_RELATIVE pPidl1,
        PCUIDLIST_RELATIVE pPidl2)
    {
        if (!pPidl1 || !pPidl2)
            return E_INVALIDARG;

        UINT column = LOWORD(pLParam);
        short result = OnCompareItems(
            reinterpret_cast<PCUITEMID_CHILD>(pPidl1),
            reinterpret_cast<PCUITEMID_CHILD>(pPidl2),
            column);

        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, static_cast<USHORT>(result));
    }

    STDMETHODIMP XShellFolderBase::CreateViewObject(
        HWND pHwndOwner,
        REFIID pRiid,
        void** pPv)
    {
        if (!pPv)
            return E_POINTER;

        *pPv = nullptr;

        if (IsEqualIID(pRiid, IID_IShellView))
        {
            SFV_CREATE sfvCreate = {};
            sfvCreate.cbSize = sizeof(sfvCreate);
            sfvCreate.pshf = static_cast<IShellFolder*>(this);
            sfvCreate.psfvcb = static_cast<IShellFolderViewCB*>(this);

            return SHCreateShellFolderView(&sfvCreate, reinterpret_cast<IShellView**>(pPv));
        }

        return E_NOINTERFACE;
    }

    STDMETHODIMP XShellFolderBase::GetAttributesOf(
        UINT pCidl,
        PCUITEMID_CHILD_ARRAY pApidl,
        SFGAOF* pRgfInOut)
    {
        if (!pApidl || !pRgfInOut)
            return E_POINTER;

        SFGAOF requestedAttrs = *pRgfInOut;
        *pRgfInOut = 0xFFFFFFFF;

        for (UINT i = 0; i < pCidl; ++i)
        {
            SFGAOF itemAttrs = OnGetAttributes(pApidl[i]);
            *pRgfInOut &= itemAttrs;
        }

        *pRgfInOut &= requestedAttrs;
        return S_OK;
    }

    STDMETHODIMP XShellFolderBase::GetUIObjectOf(
        HWND pHwndOwner,
        UINT pCidl,
        PCUITEMID_CHILD_ARRAY pApidl,
        REFIID pRiid,
        UINT* pRgfReserved,
        void** pPv)
    {
        if (!pApidl || !pPv)
            return E_POINTER;

        *pPv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP XShellFolderBase::GetDisplayNameOf(
        PCUITEMID_CHILD pPidl,
        SHGDNF pUFlags,
        STRRET* pName)
    {
        if (!pPidl || !pName)
            return E_POINTER;

        std::wstring name = OnGetDisplayName(pPidl, pUFlags);
        return SetStrRet(pName, name.c_str());
    }

    STDMETHODIMP XShellFolderBase::SetNameOf(
        HWND pHwnd,
        PCUITEMID_CHILD pPidl,
        LPCWSTR pszName,
        SHGDNF pUFlags,
        PITEMID_CHILD* pPidlOut)
    {
        return E_NOTIMPL;
    }

    // IShellFolder2
    STDMETHODIMP XShellFolderBase::GetDefaultSearchGUID(GUID* pGuid)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP XShellFolderBase::EnumSearches(IEnumExtraSearch** pEnum)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP XShellFolderBase::GetDefaultColumn(DWORD pDwRes, ULONG* pSort, ULONG* pDisplay)
    {
        if (pSort)
            *pSort = 0;
        if (pDisplay)
            *pDisplay = 0;
        return S_OK;
    }

    STDMETHODIMP XShellFolderBase::GetDefaultColumnState(UINT pColumn, SHCOLSTATEF* pCsFlags)
    {
        if (!pCsFlags)
            return E_POINTER;

        if (_Columns.empty())
            OnGetColumns(_Columns);

        if (pColumn >= _Columns.size())
            return E_FAIL;

        *pCsFlags = _Columns[pColumn].State;
        return S_OK;
    }

    STDMETHODIMP XShellFolderBase::GetDetailsEx(
        PCUITEMID_CHILD pPidl,
        const SHCOLUMNID* pScid,
        VARIANT* pV)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP XShellFolderBase::GetDetailsOf(
        PCUITEMID_CHILD pPidl,
        UINT pColumn,
        SHELLDETAILS* pSd)
    {
        if (!pSd)
            return E_POINTER;

        if (_Columns.empty())
            OnGetColumns(_Columns);

        if (pColumn >= _Columns.size())
            return E_FAIL;

        // Column header request
        if (!pPidl)
        {
            pSd->fmt = _Columns[pColumn].Format;
            pSd->cxChar = _Columns[pColumn].DefaultWidth;
            return SetStrRet(&pSd->str, _Columns[pColumn].Title.c_str());
        }

        // Item detail request
        return OnGetDetailsOf(pPidl, pColumn, pSd) ? S_OK : E_FAIL;
    }

    STDMETHODIMP XShellFolderBase::MapColumnToSCID(UINT pColumn, SHCOLUMNID* pScid)
    {
        if (!pScid)
            return E_POINTER;

        if (_Columns.empty())
            OnGetColumns(_Columns);

        if (pColumn >= _Columns.size())
            return E_FAIL;

        *pScid = _Columns[pColumn].ID;
        return S_OK;
    }

    // IShellFolderViewCB
    STDMETHODIMP XShellFolderBase::MessageSFVCB(UINT pMsg, WPARAM pWParam, LPARAM pLParam)
    {
        return E_NOTIMPL;
    }

    // IObjectWithSite
    STDMETHODIMP XShellFolderBase::SetSite(IUnknown* pUnkSite)
    {
        _Site.Attach(pUnkSite);
        if (pUnkSite)
            pUnkSite->AddRef();
        return S_OK;
    }

    STDMETHODIMP XShellFolderBase::GetSite(REFIID pRiid, void** pPvSite)
    {
        if (!pPvSite)
            return E_POINTER;

        if (!_Site)
        {
            *pPvSite = nullptr;
            return E_FAIL;
        }

        return _Site->QueryInterface(pRiid, pPvSite);
    }

    // Protected helpers
    short XShellFolderBase::OnCompareItems(
        PCUITEMID_CHILD pPidl1,
        PCUITEMID_CHILD pPidl2,
        UINT pColumn)
    {
        // Default implementation: compare by display name
        std::wstring name1 = OnGetDisplayName(pPidl1, SHGDN_NORMAL);
        std::wstring name2 = OnGetDisplayName(pPidl2, SHGDN_NORMAL);
        return static_cast<short>(_wcsicmp(name1.c_str(), name2.c_str()));
    }

    HRESULT XShellFolderBase::SetStrRet(STRRET* pSr, const wchar_t* pStr)
    {
        if (!pSr || !pStr)
            return E_POINTER;

        pSr->uType = STRRET_WSTR;
        return SHStrDupW(pStr, &pSr->pOleStr);
    }

} // namespace Tootega::Shell

