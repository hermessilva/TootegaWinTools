#pragma once
/*
** SQLLocalDBView - Windows Explorer Shell Extension
** Context Menu Handler Implementation
*/

#ifndef SQLLocalDBView_CONTEXTMENU_H
#define SQLLocalDBView_CONTEXTMENU_H

#include "Common.h"
#include "Database.h"

namespace SQLLocalDBView {

// IDs dos comandos do menu
enum MenuCommand {
    CMD_FIRST = 0,
    CMD_COPY_CREATE_TABLE = 0,
    CMD_COPY_AS_CSV,
    CMD_COPY_AS_JSON,
    CMD_COPY_AS_INSERT,
    CMD_EXPORT_CSV,
    CMD_EXPORT_JSON,
    CMD_EXPORT_SQL,
    CMD_VIEW_SCHEMA,
    CMD_VIEW_INDEXES,
    CMD_OPEN_WITH_TOOL,
    CMD_REFRESH,
    CMD_PROPERTIES,
    CMD_LAST
};

// Context Menu Handler - Menu de contexto para tabelas
class ContextMenuHandler :
    public IContextMenu3,
    public IShellExtInit,
    public IObjectWithSite {
public:
    ContextMenuHandler();
    virtual ~ContextMenuHandler();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IShellExtInit
    STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidlFolder, IDataObject* pdtobj, HKEY hkeyProgID) override;

    // IObjectWithSite
    STDMETHODIMP SetSite(IUnknown* pUnkSite) override;
    STDMETHODIMP GetSite(REFIID riid, void** ppvSite) override;

    // IContextMenu
    STDMETHODIMP QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst,
        UINT idCmdLast, UINT uFlags) override;
    STDMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* pici) override;
    STDMETHODIMP GetCommandString(UINT_PTR idCmd, UINT uType, UINT* pReserved,
        CHAR* pszName, UINT cchMax) override;

    // IContextMenu2
    STDMETHODIMP HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    // IContextMenu3
    STDMETHODIMP HandleMenuMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) override;

    // Configuration for use within ShellFolder (namespace items)
    void SetDatabasePath(const std::wstring& path) { dbPath_ = path; }
    void SetTableName(const std::wstring& name) { tableName_ = name; }
    void SetItemType(ItemType type) { itemType_ = type; }
    void SetItemPidl(PCUITEMID_CHILD pidl);  // Set PIDL for navigation

private:
    LONG refCount_;
    std::wstring dbPath_;
    std::wstring tableName_;
    ItemType itemType_ = ItemType::Table;
    std::shared_ptr<Database> database_;
    UINT cmdFirst_;
    IUnknown* site_ = nullptr;  // Site from IObjectWithSite (weak ref, not AddRef'd by us)
    PIDLIST_RELATIVE pidlItem_ = nullptr;  // PIDL of the item (for navigation)

    // Command handlers
    void CopyCreateTable(HWND hwnd);
    void CopyAsCSV(HWND hwnd);
    void CopyAsJSON(HWND hwnd);
    void CopyAsInsert(HWND hwnd);
    void ExportToCSV(HWND hwnd);
    void ExportToJSON(HWND hwnd);
    void ExportToSQL(HWND hwnd);
    void ViewSchema(HWND hwnd);
    void ViewIndexes(HWND hwnd);
    void OpenWithExternalTool(HWND hwnd);
    void ShowProperties(HWND hwnd);

    // Helpers
    void CopyToClipboard(HWND hwnd, const std::wstring& text);
    std::wstring GenerateCSV(int maxRows = -1);
    std::wstring GenerateJSON(int maxRows = -1);
    std::wstring GenerateInsertStatements(int maxRows = -1);
    bool SaveToFile(HWND hwnd, const std::wstring& content, 
        const std::wstring& defaultName, const std::wstring& filter);
};

// Também pode atuar como handler para arquivos .db
class DatabaseContextMenuHandler :
    public IContextMenu3,
    public IShellExtInit {
public:
    DatabaseContextMenuHandler();
    virtual ~DatabaseContextMenuHandler();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IShellExtInit
    STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidlFolder, IDataObject* pdtobj, HKEY hkeyProgID) override;

    // IContextMenu
    STDMETHODIMP QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst,
        UINT idCmdLast, UINT uFlags) override;
    STDMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* pici) override;
    STDMETHODIMP GetCommandString(UINT_PTR idCmd, UINT uType, UINT* pReserved,
        CHAR* pszName, UINT cchMax) override;

    // IContextMenu2
    STDMETHODIMP HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    // IContextMenu3
    STDMETHODIMP HandleMenuMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) override;

private:
    LONG refCount_;
    std::vector<std::wstring> selectedFiles_;
    UINT cmdFirst_;

    void OpenAsFolder(HWND hwnd, const std::wstring& path);
    void ExportAllTables(HWND hwnd, const std::wstring& path);
    void ShowDatabaseInfo(HWND hwnd, const std::wstring& path);
    void ValidateDatabase(HWND hwnd, const std::wstring& path);
};

} // namespace SQLLocalDBView

#endif // SQLLocalDBView_CONTEXTMENU_H
