/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** Property Handler Implementation
*/

#include "PropertyHandler.h"

namespace SQLiteView {

PropertyHandler::PropertyHandler()
    : _RefCount(1)
    , _Initialized(false) {
    ZeroMemory(&_Stats, sizeof(_Stats));
}

PropertyHandler::~PropertyHandler() {
}

STDMETHODIMP PropertyHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown)) {
        *ppv = static_cast<IPropertyStore*>(this);
    }
    else if (IsEqualIID(riid, IID_IPropertyStore)) {
        *ppv = static_cast<IPropertyStore*>(this);
    }
    else if (IsEqualIID(riid, IID_IPropertyStoreCapabilities)) {
        *ppv = static_cast<IPropertyStoreCapabilities*>(this);
    }
    else if (IsEqualIID(riid, IID_IInitializeWithFile)) {
        *ppv = static_cast<IInitializeWithFile*>(this);
    }
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
    LONG count = InterlockedDecrement(&_RefCount);
    if (count == 0) delete this;
    return count;
}

// IPropertyStore
STDMETHODIMP PropertyHandler::GetCount(DWORD* pcProps) {
    if (!pcProps) return E_POINTER;
    
    LoadProperties();
    
    // Standard properties + custom SQLite properties
    *pcProps = 13; // Table count, view count, etc.
    
    return S_OK;
}

STDMETHODIMP PropertyHandler::GetAt(DWORD iProp, PROPERTYKEY* pkey) {
    if (!pkey) return E_POINTER;
    
    switch (iProp) {
        case 0: *pkey = PKEY_SQLite_TableCount; break;
        case 1: *pkey = PKEY_SQLite_ViewCount; break;
        case 2: *pkey = PKEY_SQLite_IndexCount; break;
        case 3: *pkey = PKEY_SQLite_TriggerCount; break;
        case 4: *pkey = PKEY_SQLite_RecordCount; break;
        case 5: *pkey = PKEY_SQLite_PageSize; break;
        case 6: *pkey = PKEY_SQLite_Encoding; break;
        case 7: *pkey = PKEY_SQLite_SQLiteVersion; break;
        case 8: *pkey = PKEY_Size; break;
        case 9: *pkey = PKEY_ItemTypeText; break;
        case 10: *pkey = PKEY_FileDescription; break;
        case 11: *pkey = PKEY_ItemType; break;
        case 12: *pkey = PKEY_ContentType; break;
        default: return E_INVALIDARG;
    }
    
    return S_OK;
}

STDMETHODIMP PropertyHandler::GetValue(REFPROPERTYKEY key, PROPVARIANT* pv) {
    if (!pv) return E_POINTER;
    
    PropVariantInit(pv);
    LoadProperties();
    
    // Custom SQLite properties
    if (IsEqualPropertyKey(key, PKEY_SQLite_TableCount)) {
        pv->vt = VT_I8;
        pv->hVal.QuadPart = _Stats.TableCount;
        return S_OK;
    }
    if (IsEqualPropertyKey(key, PKEY_SQLite_ViewCount)) {
        pv->vt = VT_I8;
        pv->hVal.QuadPart = _Stats.ViewCount;
        return S_OK;
    }
    if (IsEqualPropertyKey(key, PKEY_SQLite_IndexCount)) {
        pv->vt = VT_I8;
        pv->hVal.QuadPart = _Stats.IndexCount;
        return S_OK;
    }
    if (IsEqualPropertyKey(key, PKEY_SQLite_TriggerCount)) {
        pv->vt = VT_I8;
        pv->hVal.QuadPart = _Stats.TriggerCount;
        return S_OK;
    }
    if (IsEqualPropertyKey(key, PKEY_SQLite_RecordCount)) {
        pv->vt = VT_I8;
        pv->hVal.QuadPart = _Stats.TotalRecords;
        return S_OK;
    }
    if (IsEqualPropertyKey(key, PKEY_SQLite_PageSize)) {
        pv->vt = VT_I8;
        pv->hVal.QuadPart = _Stats.PageSize;
        return S_OK;
    }
    if (IsEqualPropertyKey(key, PKEY_SQLite_Encoding)) {
        return InitPropVariantFromString(_Stats.Encoding.c_str(), pv);
    }
    if (IsEqualPropertyKey(key, PKEY_SQLite_SQLiteVersion)) {
        if (_Database) {
            return InitPropVariantFromString(_Database->GetSQLiteVersion().c_str(), pv);
        }
        return S_OK;
    }
    
    // Standard shell properties
    if (IsEqualPropertyKey(key, PKEY_Size)) {
        pv->vt = VT_UI8;
        pv->uhVal.QuadPart = static_cast<ULONGLONG>(_Stats.FileSize);
        return S_OK;
    }
    if (IsEqualPropertyKey(key, PKEY_ItemTypeText)) {
        return InitPropVariantFromString(L"SQLite Database", pv);
    }
    if (IsEqualPropertyKey(key, PKEY_FileDescription)) {
        std::wstring desc = L"SQLite Database with " + 
                           std::to_wstring(_Stats.TableCount) + L" tables, " +
                           std::to_wstring(_Stats.TotalRecords) + L" records";
        return InitPropVariantFromString(desc.c_str(), pv);
    }
    if (IsEqualPropertyKey(key, PKEY_ItemType)) {
        return InitPropVariantFromString(L".db", pv);
    }
    if (IsEqualPropertyKey(key, PKEY_ContentType)) {
        return InitPropVariantFromString(L"application/x-sqlite3", pv);
    }
    
    return S_OK;
}

STDMETHODIMP PropertyHandler::SetValue(REFPROPERTYKEY key, REFPROPVARIANT propvar) {
    // Read-only
    return STG_E_ACCESSDENIED;
}

STDMETHODIMP PropertyHandler::Commit() {
    // Read-only
    return S_OK;
}

// IPropertyStoreCapabilities
STDMETHODIMP PropertyHandler::IsPropertyWritable(REFPROPERTYKEY key) {
    // All properties are read-only
    return S_FALSE;
}

// IInitializeWithFile
STDMETHODIMP PropertyHandler::Initialize(LPCWSTR pszFilePath, DWORD grfMode) {
    if (!pszFilePath) return E_POINTER;
    
    _FilePath = pszFilePath;
    _Initialized = false;
    
    return S_OK;
}

void PropertyHandler::LoadProperties() {
    if (_Initialized) return;
    if (_FilePath.empty()) return;
    
    _Database = DatabasePool::Instance().GetDatabase(_FilePath);
    if (_Database) {
        _Stats = _Database->GetStatistics();
    }
    
    _Initialized = true;
}

} // namespace SQLiteView
