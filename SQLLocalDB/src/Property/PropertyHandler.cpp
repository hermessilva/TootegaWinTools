/*
** SQLLocalDBView - Windows Explorer Shell Extension
** Property Handler Implementation
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovação. All rights reserved.
** Licensed under MIT License
*/

#include "PropertyHandler.h"

// Custom property keys - definidos aqui para garantir linkage
// {D9FFDCA0-1234-5678-9ABC-AABBCCDDEEFF}
static const PROPERTYKEY PKEY_SQLLocalDB_TableCount = { { 0xD9FFDCA0, 0x1234, 0x5678, { 0x9A, 0xBC, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } }, 1 };
static const PROPERTYKEY PKEY_SQLLocalDB_ViewCount = { { 0xD9FFDCA0, 0x1234, 0x5678, { 0x9A, 0xBC, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } }, 2 };
static const PROPERTYKEY PKEY_SQLLocalDB_IndexCount = { { 0xD9FFDCA0, 0x1234, 0x5678, { 0x9A, 0xBC, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } }, 3 };
static const PROPERTYKEY PKEY_SQLLocalDB_PageSize = { { 0xD9FFDCA0, 0x1234, 0x5678, { 0x9A, 0xBC, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } }, 4 };
static const PROPERTYKEY PKEY_SQLLocalDB_Encoding = { { 0xD9FFDCA0, 0x1234, 0x5678, { 0x9A, 0xBC, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } }, 5 };

namespace SQLLocalDBView {

PropertyHandler::PropertyHandler() : refCount_(1) {
}

PropertyHandler::~PropertyHandler() {
    // Limpar propriedades
    for (auto& pair : properties_) {
        PropVariantClear(&pair.second);
    }
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
    else if (IsEqualIID(riid, IID_IInitializeWithStream)) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    }
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) PropertyHandler::AddRef() {
    return InterlockedIncrement(&refCount_);
}

STDMETHODIMP_(ULONG) PropertyHandler::Release() {
    LONG count = InterlockedDecrement(&refCount_);
    if (count == 0) {
        delete this;
    }
    return count;
}

// IInitializeWithFile
STDMETHODIMP PropertyHandler::Initialize(LPCWSTR pszFilePath, DWORD grfMode) {
    if (!pszFilePath) return E_POINTER;
    
    filePath_ = pszFilePath;
    LoadProperties();
    
    return S_OK;
}

// IInitializeWithStream
STDMETHODIMP PropertyHandler::Initialize(IStream* pstream, DWORD grfMode) {
    // SQL LocalDB não suporta streams diretamente
    return E_NOTIMPL;
}

// IPropertyStore
STDMETHODIMP PropertyHandler::GetCount(DWORD* cProps) {
    if (!cProps) return E_POINTER;
    *cProps = (DWORD)propertyKeys_.size();
    return S_OK;
}

STDMETHODIMP PropertyHandler::GetAt(DWORD iProp, PROPERTYKEY* pkey) {
    if (!pkey) return E_POINTER;
    if (iProp >= propertyKeys_.size()) return E_INVALIDARG;
    
    *pkey = propertyKeys_[iProp];
    return S_OK;
}

STDMETHODIMP PropertyHandler::GetValue(REFPROPERTYKEY key, PROPVARIANT* pv) {
    if (!pv) return E_POINTER;
    
    PropVariantInit(pv);
    
    auto it = properties_.find(key);
    if (it != properties_.end()) {
        return PropVariantCopy(pv, &it->second);
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
    return S_FALSE; // Todas as propriedades são read-only
}

void PropertyHandler::LoadProperties() {
    if (filePath_.empty()) return;
    
    database_ = DatabasePool::Instance().GetDatabase(filePath_);
    if (!database_ || !database_->IsOpen()) return;
    
    dbInfo_ = database_->GetDatabaseInfo();
    initialized_ = true;
    
    // Propriedades padrão do sistema
    AddProperty(PKEY_ItemType, L"SQL LocalDB Database");
    AddProperty(PKEY_ItemTypeText, L"SQL LocalDB Database File");
    AddProperty(PKEY_FileDescription, L"SQL LocalDB Database");
    AddProperty(PKEY_Size, dbInfo_.fileSize);
    
    // Propriedades customizadas
    std::wstring summary = StringUtil::Format(
        L"%d tables, %d views, %d indexes",
        (int)dbInfo_.tables.size(),
        (int)dbInfo_.views.size(),
        (int)dbInfo_.indexes.size()
    );
    AddProperty(PKEY_Comment, summary);
    
    // Encoding
    AddProperty(PKEY_SQLLocalDB_Encoding, dbInfo_.encoding);
    
    // Page size
    AddProperty(PKEY_SQLLocalDB_PageSize, (ULONG)dbInfo_.pageSize);
    
    // Counts
    AddProperty(PKEY_SQLLocalDB_TableCount, (ULONG)dbInfo_.tables.size());
    AddProperty(PKEY_SQLLocalDB_ViewCount, (ULONG)dbInfo_.views.size());
    AddProperty(PKEY_SQLLocalDB_IndexCount, (ULONG)dbInfo_.indexes.size());
}

void PropertyHandler::AddProperty(REFPROPERTYKEY key, const std::wstring& value) {
    propertyKeys_.push_back(key);
    
    PROPVARIANT pv;
    PropVariantInit(&pv);
    InitPropVariantFromString(value.c_str(), &pv);
    properties_[key] = pv;
}

void PropertyHandler::AddProperty(REFPROPERTYKEY key, LONGLONG value) {
    propertyKeys_.push_back(key);
    
    PROPVARIANT pv;
    PropVariantInit(&pv);
    InitPropVariantFromInt64(value, &pv);
    properties_[key] = pv;
}

void PropertyHandler::AddProperty(REFPROPERTYKEY key, ULONG value) {
    propertyKeys_.push_back(key);
    
    PROPVARIANT pv;
    PropVariantInit(&pv);
    InitPropVariantFromUInt32(value, &pv);
    properties_[key] = pv;
}

} // namespace SQLLocalDBView
