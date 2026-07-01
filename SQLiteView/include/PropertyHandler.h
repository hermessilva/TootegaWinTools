#pragma once
/*
** SQLiteView - Windows Explorer Shell Extension
** Property Handler Implementation
*/

#ifndef SQLITEVIEW_PROPERTYHANDLER_H
#define SQLITEVIEW_PROPERTYHANDLER_H

#include "Common.h"
#include "Database.h"

namespace SQLiteView {

// Comparador para PROPERTYKEY em std::map
struct PropertyKeyLess {
    bool operator()(const PROPERTYKEY& a, const PROPERTYKEY& b) const {
        int cmp = memcmp(&a.fmtid, &b.fmtid, sizeof(GUID));
        if (cmp != 0) return cmp < 0;
        return a.pid < b.pid;
    }
};

// Property Handler - Mostra propriedades do arquivo DB
class PropertyHandler :
    public IPropertyStore,
    public IPropertyStoreCapabilities,
    public IInitializeWithFile,
    public IInitializeWithStream {
public:
    PropertyHandler();
    virtual ~PropertyHandler();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IPropertyStore
    STDMETHODIMP GetCount(DWORD* cProps) override;
    STDMETHODIMP GetAt(DWORD iProp, PROPERTYKEY* pkey) override;
    STDMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT* pv) override;
    STDMETHODIMP SetValue(REFPROPERTYKEY key, REFPROPVARIANT propvar) override;
    STDMETHODIMP Commit() override;

    // IPropertyStoreCapabilities
    STDMETHODIMP IsPropertyWritable(REFPROPERTYKEY key) override;

    // IInitializeWithFile
    STDMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;

    // IInitializeWithStream
    STDMETHODIMP Initialize(IStream* pstream, DWORD grfMode) override;

private:
    LONG refCount_;
    std::wstring filePath_;
    std::shared_ptr<Database> database_;
    DatabaseInfo dbInfo_;
    bool initialized_ = false;

    std::vector<PROPERTYKEY> propertyKeys_;
    std::map<PROPERTYKEY, PROPVARIANT, PropertyKeyLess> properties_;

    void LoadProperties();
    void AddProperty(REFPROPERTYKEY key, const std::wstring& value);
    void AddProperty(REFPROPERTYKEY key, LONGLONG value);
    void AddProperty(REFPROPERTYKEY key, ULONG value);
};

// Custom property keys para SQLite
// Usando PKEYs existentes onde possível, definindo customizados quando necessário

// PKEY_SQLite_TableCount - Número de tabelas
// PKEY_SQLite_ViewCount - Número de views
// PKEY_SQLite_IndexCount - Número de índices
// PKEY_SQLite_PageSize - Tamanho da página
// PKEY_SQLite_Encoding - Encoding
// PKEY_SQLite_Version - Versão do schema

} // namespace SQLiteView

#endif // SQLITEVIEW_PROPERTYHANDLER_H
