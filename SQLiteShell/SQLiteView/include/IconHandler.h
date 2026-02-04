#pragma once
/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** Icon Handler - Provides custom icons for database files
*/

#ifndef SQLITEVIEW_ICONHANDLER_H
#define SQLITEVIEW_ICONHANDLER_H

#include "Common.h"

namespace SQLiteView {

// Icon Handler - Provides icons for .db, .sqlite, .sqlite3 files
class IconHandler : 
    public IExtractIconW,
    public IPersistFile {
public:
    IconHandler();
    virtual ~IconHandler();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IExtractIconW
    STDMETHODIMP GetIconLocation(UINT uFlags, LPWSTR pszIconFile, UINT cchMax,
        int* piIndex, UINT* pwFlags) override;
    STDMETHODIMP Extract(LPCWSTR pszFile, UINT nIconIndex, HICON* phiconLarge,
        HICON* phiconSmall, UINT nIconSize) override;

    // IPersist
    STDMETHODIMP GetClassID(CLSID* pClassID) override;

    // IPersistFile
    STDMETHODIMP IsDirty() override;
    STDMETHODIMP Load(LPCOLESTR pszFileName, DWORD dwMode) override;
    STDMETHODIMP Save(LPCOLESTR pszFileName, BOOL fRemember) override;
    STDMETHODIMP SaveCompleted(LPCOLESTR pszFileName) override;
    STDMETHODIMP GetCurFile(LPOLESTR* ppszFileName) override;

private:
    LONG _RefCount;
    std::wstring _FilePath;
};

} // namespace SQLiteView

#endif // SQLITEVIEW_ICONHANDLER_H
