#pragma once
/*
** SevenZipView - Windows Explorer Shell Extension
** Icon Handler
*/

#ifndef SEVENZIPVIEW_ICONHANDLER_H
#define SEVENZIPVIEW_ICONHANDLER_H

#include "Common.h"

namespace SevenZipView {

// Icon handler for .7z files
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

    // IPersist
    STDMETHODIMP GetClassID(CLSID* pClassID) override;

    // IPersistFile
    STDMETHODIMP IsDirty() override;
    STDMETHODIMP Load(LPCOLESTR pszFileName, DWORD dwMode) override;
    STDMETHODIMP Save(LPCOLESTR pszFileName, BOOL fRemember) override;
    STDMETHODIMP SaveCompleted(LPCOLESTR pszFileName) override;
    STDMETHODIMP GetCurFile(LPOLESTR* ppszFileName) override;

    // IExtractIconW
    STDMETHODIMP GetIconLocation(UINT uFlags, LPWSTR pszIconFile, UINT cchMax,
        int* piIndex, UINT* pwFlags) override;
    STDMETHODIMP Extract(LPCWSTR pszFile, UINT nIconIndex,
        HICON* phiconLarge, HICON* phiconSmall, UINT nIconSize) override;

private:
    LONG _RefCount;
    std::wstring _FilePath;
};

// Icon provider for items inside the archive
class ItemIconExtractor : public IExtractIconW {
public:
    ItemIconExtractor();
    virtual ~ItemIconExtractor();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IExtractIconW
    STDMETHODIMP GetIconLocation(UINT uFlags, LPWSTR pszIconFile, UINT cchMax,
        int* piIndex, UINT* pwFlags) override;
    STDMETHODIMP Extract(LPCWSTR pszFile, UINT nIconIndex,
        HICON* phiconLarge, HICON* phiconSmall, UINT nIconSize) override;

    // Initialize with item info
    void SetItemInfo(const std::wstring& name, ItemType type);

private:
    LONG _RefCount;
    std::wstring _ItemName;
    ItemType _ItemType;
    
    // Get icon index for file extension
    int GetIconIndexForExtension(const std::wstring& ext) const;
};

// Helper to get shell icon for file type
HICON GetShellIconForFile(const std::wstring& fileName, bool largeIcon);
HICON GetShellFolderIcon(bool largeIcon);

} // namespace SevenZipView

#endif // SEVENZIPVIEW_ICONHANDLER_H
