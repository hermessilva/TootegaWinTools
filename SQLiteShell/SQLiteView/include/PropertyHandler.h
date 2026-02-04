#pragma once
/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** Property Handler - Shows database properties in Explorer details pane
*/

#ifndef SQLITEVIEW_PROPERTYHANDLER_H
#define SQLITEVIEW_PROPERTYHANDLER_H

#include "Common.h"
#include "Database.h"

namespace SQLiteView {

// Property Handler - Provides property sheet information
class PropertyHandler : 
    public IPropertyStore,
    public IPropertyStoreCapabilities,
    public IInitializeWithFile {
public:
    PropertyHandler();
    virtual ~PropertyHandler();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IPropertyStore
    STDMETHODIMP GetCount(DWORD* pcProps) override;
    STDMETHODIMP GetAt(DWORD iProp, PROPERTYKEY* pkey) override;
    STDMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT* pv) override;
    STDMETHODIMP SetValue(REFPROPERTYKEY key, REFPROPVARIANT propvar) override;
    STDMETHODIMP Commit() override;

    // IPropertyStoreCapabilities
    STDMETHODIMP IsPropertyWritable(REFPROPERTYKEY key) override;

    // IInitializeWithFile
    STDMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;

private:
    LONG _RefCount;
    std::wstring _FilePath;
    std::shared_ptr<Database> _Database;
    Database::Statistics _Stats;
    bool _Initialized;

    void LoadProperties();
};

} // namespace SQLiteView

#endif // SQLITEVIEW_PROPERTYHANDLER_H
