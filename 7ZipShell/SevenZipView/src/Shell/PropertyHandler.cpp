/*
** SevenZipView - Windows Explorer Shell Extension for 7z Archives
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
**
** Property Handler Implementation
*/

#include "PropertyHandler.h"
#include <propvarutil.h>
#include <strsafe.h>

namespace SevenZipView {

// Note: PKEY_7z_* constants are defined in DllMain.cpp

//==============================================================================
// PropertyHandler
//==============================================================================

PropertyHandler::PropertyHandler()
    : _RefCount(1)
    , _Loaded(false)
    , _FileCount(0)
    , _FolderCount(0)
    , _TotalSize(0)
    , _CompressedSize(0)
    , _IsEncrypted(false) {
    
    InterlockedIncrement(&g_DllRefCount);
}

PropertyHandler::~PropertyHandler() {
    InterlockedDecrement(&g_DllRefCount);
}

// IUnknown
STDMETHODIMP PropertyHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    
    if (riid == IID_IUnknown)
        *ppv = static_cast<IUnknown*>(static_cast<IPropertyStore*>(this));
    else if (riid == IID_IPropertyStore)
        *ppv = static_cast<IPropertyStore*>(this);
    else if (riid == IID_IPropertyStoreCapabilities)
        *ppv = static_cast<IPropertyStoreCapabilities*>(this);
    else if (riid == IID_IInitializeWithFile)
        *ppv = static_cast<IInitializeWithFile*>(this);
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) PropertyHandler::AddRef() {
    return InterlockedIncrement(&_RefCount);
}

STDMETHODIMP_(ULONG) PropertyHandler::Release() {
    LONG ref = InterlockedDecrement(&_RefCount);
    if (ref == 0)
        delete this;
    return ref;
}

// IInitializeWithFile
STDMETHODIMP PropertyHandler::Initialize(LPCWSTR pszFilePath, DWORD grfMode) {
    if (!pszFilePath) return E_POINTER;
    
    _ArchivePath = pszFilePath;
    
    // Open archive and load stats
    _Archive = ArchivePool::Instance().GetArchive(_ArchivePath);
    if (_Archive && _Archive->IsOpen()) {
        _FileCount = _Archive->GetFileCount();
        _FolderCount = _Archive->GetFolderCount();
        _TotalSize = _Archive->GetTotalUncompressedSize();
        _CompressedSize = _Archive->GetTotalCompressedSize();
        _Loaded = true;
    }
    
    return S_OK;
}

// IPropertyStore
STDMETHODIMP PropertyHandler::GetCount(DWORD* cProps) {
    if (!cProps) return E_POINTER;
    *cProps = 5; // FileCount, FolderCount, Size, CompressedSize, CompressionRatio
    return S_OK;
}

STDMETHODIMP PropertyHandler::GetAt(DWORD iProp, PROPERTYKEY* pkey) {
    if (!pkey) return E_POINTER;
    
    switch (iProp) {
    case 0:
        *pkey = PKEY_7z_FileCount;
        break;
    case 1:
        *pkey = PKEY_7z_FolderCount;
        break;
    case 2:
        *pkey = PKEY_Size;
        break;
    case 3:
        *pkey = PKEY_7z_CompressionRatio;
        break;
    case 4:
        *pkey = PKEY_7z_IsEncrypted;
        break;
    default:
        return E_INVALIDARG;
    }
    
    return S_OK;
}

STDMETHODIMP PropertyHandler::GetValue(REFPROPERTYKEY key, PROPVARIANT* pv) {
    if (!pv) return E_POINTER;
    PropVariantInit(pv);
    
    if (!_Loaded) return E_FAIL;
    
    if (IsEqualPropertyKey(key, PKEY_7z_FileCount)) {
        pv->vt = VT_UI4;
        pv->ulVal = _FileCount;
        return S_OK;
    }
    
    if (IsEqualPropertyKey(key, PKEY_7z_FolderCount)) {
        pv->vt = VT_UI4;
        pv->ulVal = _FolderCount;
        return S_OK;
    }
    
    if (IsEqualPropertyKey(key, PKEY_Size)) {
        pv->vt = VT_UI8;
        pv->uhVal.QuadPart = _TotalSize;
        return S_OK;
    }
    
    if (IsEqualPropertyKey(key, PKEY_7z_CompressionRatio)) {
        pv->vt = VT_UI4;
        if (_TotalSize > 0)
            pv->ulVal = static_cast<UINT32>((_CompressedSize * 100) / _TotalSize);
        else
            pv->ulVal = 0;
        return S_OK;
    }
    
    if (IsEqualPropertyKey(key, PKEY_7z_IsEncrypted)) {
        pv->vt = VT_BOOL;
        pv->boolVal = _IsEncrypted ? VARIANT_TRUE : VARIANT_FALSE;
        return S_OK;
    }
    
    return E_INVALIDARG;
}

STDMETHODIMP PropertyHandler::SetValue(REFPROPERTYKEY key, REFPROPVARIANT propvar) {
    return STG_E_ACCESSDENIED; // Read-only
}

STDMETHODIMP PropertyHandler::Commit() {
    return S_OK;
}

// IPropertyStoreCapabilities
STDMETHODIMP PropertyHandler::IsPropertyWritable(REFPROPERTYKEY key) {
    return S_FALSE; // All properties are read-only
}

//==============================================================================
// ItemPropertyHandler
//==============================================================================

ItemPropertyHandler::ItemPropertyHandler()
    : _RefCount(1) {
    InterlockedIncrement(&g_DllRefCount);
}

ItemPropertyHandler::~ItemPropertyHandler() {
    InterlockedDecrement(&g_DllRefCount);
}

// IUnknown
STDMETHODIMP ItemPropertyHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    
    if (riid == IID_IUnknown || riid == IID_IPropertyStore)
        *ppv = static_cast<IPropertyStore*>(this);
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) ItemPropertyHandler::AddRef() {
    return InterlockedIncrement(&_RefCount);
}

STDMETHODIMP_(ULONG) ItemPropertyHandler::Release() {
    LONG ref = InterlockedDecrement(&_RefCount);
    if (ref == 0)
        delete this;
    return ref;
}

// IPropertyStore
STDMETHODIMP ItemPropertyHandler::GetCount(DWORD* cProps) {
    if (!cProps) return E_POINTER;
    *cProps = 4; // Name, Size, DateModified, Attributes
    return S_OK;
}

STDMETHODIMP ItemPropertyHandler::GetAt(DWORD iProp, PROPERTYKEY* pkey) {
    if (!pkey) return E_POINTER;
    
    switch (iProp) {
    case 0:
        *pkey = PKEY_ItemNameDisplay;
        break;
    case 1:
        *pkey = PKEY_Size;
        break;
    case 2:
        *pkey = PKEY_DateModified;
        break;
    case 3:
        *pkey = PKEY_FileAttributes;
        break;
    default:
        return E_INVALIDARG;
    }
    
    return S_OK;
}

STDMETHODIMP ItemPropertyHandler::GetValue(REFPROPERTYKEY key, PROPVARIANT* pv) {
    if (!pv) return E_POINTER;
    PropVariantInit(pv);
    
    if (IsEqualPropertyKey(key, PKEY_ItemNameDisplay)) {
        return InitPropVariantFromString(_Entry.Name.c_str(), pv);
    }
    
    if (IsEqualPropertyKey(key, PKEY_Size)) {
        pv->vt = VT_UI8;
        pv->uhVal.QuadPart = _Entry.Size;
        return S_OK;
    }
    
    if (IsEqualPropertyKey(key, PKEY_DateModified)) {
        if (_Entry.ModifiedTime.dwLowDateTime != 0 || _Entry.ModifiedTime.dwHighDateTime != 0) {
            return InitPropVariantFromFileTime(&_Entry.ModifiedTime, pv);
        }
        return E_FAIL;
    }
    
    if (IsEqualPropertyKey(key, PKEY_FileAttributes)) {
        pv->vt = VT_UI4;
        pv->ulVal = _Entry.Attributes;
        return S_OK;
    }
    
    return E_INVALIDARG;
}

STDMETHODIMP ItemPropertyHandler::SetValue(REFPROPERTYKEY key, REFPROPVARIANT propvar) {
    return STG_E_ACCESSDENIED; // Read-only
}

STDMETHODIMP ItemPropertyHandler::Commit() {
    return S_OK;
}

} // namespace SevenZipView
