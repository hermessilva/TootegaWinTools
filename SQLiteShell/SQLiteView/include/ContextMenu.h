#pragma once
/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** Context Menu Handler - Provides right-click context menu options
*/

#ifndef SQLITEVIEW_CONTEXTMENU_H
#define SQLITEVIEW_CONTEXTMENU_H

#include "Common.h"
#include "Database.h"

namespace SQLiteView {

// Context Menu Handler - Right-click menu for database files and items
class ContextMenuHandler : 
    public IContextMenu3,
    public IShellExtInit {
public:
    ContextMenuHandler();
    virtual ~ContextMenuHandler();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IContextMenu
    STDMETHODIMP QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst,
        UINT idCmdLast, UINT uFlags) override;
    STDMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* pici) override;
    STDMETHODIMP GetCommandString(UINT_PTR idCmd, UINT uType, UINT* pReserved,
        LPSTR pszName, UINT cchMax) override;

    // IContextMenu2
    STDMETHODIMP HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    // IContextMenu3
    STDMETHODIMP HandleMenuMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) override;

    // IShellExtInit
    STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidlFolder, IDataObject* pdtobj, HKEY hkeyProgID) override;

private:
    LONG _RefCount;
    std::wstring _FilePath;
    std::wstring _TableName;
    std::vector<INT64> _SelectedRowIDs;
    std::shared_ptr<Database> _Database;
    UINT _FirstCmdID;

    enum CommandID {
        CMD_OPEN = 0,
        CMD_EXPORT_CSV,
        CMD_EXPORT_JSON,
        CMD_EXPORT_SQL,
        CMD_COPY_RECORD,
        CMD_VIEW_SCHEMA,
        CMD_VACUUM,
        CMD_INTEGRITY_CHECK,
        CMD_ANALYZE,
        CMD_SEPARATOR,
        CMD_PROPERTIES,
        CMD_MAX
    };

    void DoExportCSV(HWND hwnd);
    void DoExportJSON(HWND hwnd);
    void DoExportSQL(HWND hwnd);
    void DoCopyRecord(HWND hwnd);
    void DoViewSchema(HWND hwnd);
    void DoVacuum(HWND hwnd);
    void DoIntegrityCheck(HWND hwnd);
    void DoAnalyze(HWND hwnd);
    void DoProperties(HWND hwnd);
};

} // namespace SQLiteView

#endif // SQLITEVIEW_CONTEXTMENU_H
