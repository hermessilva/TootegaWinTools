#pragma once
/*
** SevenZipView - Windows Explorer Shell Extension
** Context Menu Handler
*/

#ifndef SEVENZIPVIEW_CONTEXTMENU_H
#define SEVENZIPVIEW_CONTEXTMENU_H

#include "Common.h"

namespace SevenZipView {

// Context menu handler for .7z files (appears when right-clicking .7z in Explorer)
class ArchiveContextMenuHandler : 
    public IContextMenu,
    public IShellExtInit {
public:
    ArchiveContextMenuHandler();
    virtual ~ArchiveContextMenuHandler();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IShellExtInit
    STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidlFolder, IDataObject* pDataObj, HKEY hkeyProgID) override;

    // IContextMenu
    STDMETHODIMP QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst,
        UINT idCmdLast, UINT uFlags) override;
    STDMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* pici) override;
    STDMETHODIMP GetCommandString(UINT_PTR idCmd, UINT uType,
        UINT* pReserved, CHAR* pszName, UINT cchMax) override;

private:
    LONG _RefCount;
    std::wstring _ArchivePath;
    
    enum MenuCommand {
        CMD_EXTRACT_HERE = 0,
        CMD_EXTRACT_TO_FOLDER,
        CMD_TEST_ARCHIVE,
        CMD_OPEN_WITH_7ZIP,
        CMD_COUNT
    };
    
    bool ExtractHere();
    bool ExtractToFolder();
    bool TestArchive();
    bool OpenWith7Zip();
};

// Context menu handler for items inside archive (when browsing as folder)
class ItemContextMenuHandler : public IContextMenu, public IObjectWithSite {
public:
    ItemContextMenuHandler();
    virtual ~ItemContextMenuHandler();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IContextMenu
    STDMETHODIMP QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst,
        UINT idCmdLast, UINT uFlags) override;
    STDMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* pici) override;
    STDMETHODIMP GetCommandString(UINT_PTR idCmd, UINT uType,
        UINT* pReserved, CHAR* pszName, UINT cchMax) override;
    
    // IObjectWithSite
    STDMETHODIMP SetSite(IUnknown* pUnkSite) override;
    STDMETHODIMP GetSite(REFIID riid, void** ppvSite) override;

    // Initialize with archive and single item info
    void SetArchive(const std::wstring& archivePath, UINT32 itemIndex, const std::wstring& itemPath);
    
    // Initialize with archive and multiple items
    void SetArchiveMultiple(const std::wstring& archivePath, std::vector<std::pair<UINT32, std::wstring>>&& items);
    
    // Set folder PIDL for navigation
    void SetFolderPIDL(PCIDLIST_ABSOLUTE pidlFolder);

private:
    LONG _RefCount;
    std::wstring _ArchivePath;
    std::vector<std::pair<UINT32, std::wstring>> _Items;  // index, path pairs
    PIDLIST_ABSOLUTE _FolderPIDL;  // PIDL of the folder containing items (for navigation)
    IUnknown* _Site;  // Site for IShellBrowser access
    
    enum MenuCommand {
        CMD_OPEN = 0,
        CMD_COPY,
        CMD_EXTRACT_TO,
        CMD_PROPERTIES,
        CMD_COUNT
    };
    
    bool OpenItem(HWND hwnd);
    bool NavigateToFolder(HWND hwnd, const std::wstring& folderPath);
    bool CopyItem(HWND hwnd);
    bool ExtractTo(HWND hwnd);
    bool ShowProperties(HWND hwnd);
    
    std::wstring GetTempCachePath();
    std::wstring SanitizePath(const std::wstring& path);
};

} // namespace SevenZipView

#endif // SEVENZIPVIEW_CONTEXTMENU_H
