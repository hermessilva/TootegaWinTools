#pragma once
/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** Shell Folder Implementation
*/

#ifndef SQLITEVIEW_SHELLFOLDER_H
#define SQLITEVIEW_SHELLFOLDER_H

#include "Common.h"
#include "Database.h"

namespace SQLiteView {

// PIDL item data structure
#pragma pack(push, 1)
struct ItemData {
    USHORT cb;              // Total size including cb
    USHORT signature;       // 0x5351 ('SQ' for SQLite)
    ItemType type;
    WCHAR name[260];        // Table/record name
    WCHAR path[512];        // Full path: "tablename" or "tablename/Row_123"
    INT64 rowid;            // SQLite rowid (-1 for tables)
    INT64 recordCount;      // Record count (for tables)
    INT64 columnCount;      // Column count (for tables)
    FILETIME modifiedTime;  // Modification time
    BYTE reserved[16];
    
    static const USHORT SIGNATURE = 0x5351; // 'SQ'
    
    static UINT GetSize(const WCHAR* name) {
        return sizeof(ItemData) - sizeof(WCHAR) * (260 - (UINT)wcslen(name) - 1);
    }
};
#pragma pack(pop)

// Forward declarations
class ShellFolder;
class EnumIDList;

// Class Factory
class ClassFactory : public IClassFactory {
public:
    ClassFactory(REFCLSID clsid);
    virtual ~ClassFactory();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override;
    STDMETHODIMP LockServer(BOOL fLock) override;

private:
    LONG _RefCount;
    CLSID _Clsid;
};

// Shell Folder - Implements navigation inside the SQLite database
class ShellFolder : 
    public IShellFolder2,
    public IPersistFolder3,
    public IPersistFile,
    public IShellFolderViewCB,
    public IObjectWithSite {
public:
    ShellFolder();
    virtual ~ShellFolder();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IShellFolder
    STDMETHODIMP ParseDisplayName(HWND hwnd, IBindCtx* pbc, LPWSTR pszDisplayName,
        ULONG* pchEaten, PIDLIST_RELATIVE* ppidl, ULONG* pdwAttributes) override;
    STDMETHODIMP EnumObjects(HWND hwnd, SHCONTF grfFlags, IEnumIDList** ppenumIDList) override;
    STDMETHODIMP BindToObject(PCUIDLIST_RELATIVE pidl, IBindCtx* pbc, REFIID riid, void** ppv) override;
    STDMETHODIMP BindToStorage(PCUIDLIST_RELATIVE pidl, IBindCtx* pbc, REFIID riid, void** ppv) override;
    STDMETHODIMP CompareIDs(LPARAM lParam, PCUIDLIST_RELATIVE pidl1, PCUIDLIST_RELATIVE pidl2) override;
    STDMETHODIMP CreateViewObject(HWND hwndOwner, REFIID riid, void** ppv) override;
    STDMETHODIMP GetAttributesOf(UINT cidl, PCUITEMID_CHILD_ARRAY apidl, SFGAOF* rgfInOut) override;
    STDMETHODIMP GetUIObjectOf(HWND hwndOwner, UINT cidl, PCUITEMID_CHILD_ARRAY apidl,
        REFIID riid, UINT* rgfReserved, void** ppv) override;
    STDMETHODIMP GetDisplayNameOf(PCUITEMID_CHILD pidl, SHGDNF uFlags, STRRET* pName) override;
    STDMETHODIMP SetNameOf(HWND hwnd, PCUITEMID_CHILD pidl, LPCWSTR pszName,
        SHGDNF uFlags, PITEMID_CHILD* ppidlOut) override;

    // IShellFolder2
    STDMETHODIMP GetDefaultSearchGUID(GUID* pguid) override;
    STDMETHODIMP EnumSearches(IEnumExtraSearch** ppenum) override;
    STDMETHODIMP GetDefaultColumn(DWORD dwRes, ULONG* pSort, ULONG* pDisplay) override;
    STDMETHODIMP GetDefaultColumnState(UINT iColumn, SHCOLSTATEF* pcsFlags) override;
    STDMETHODIMP GetDetailsEx(PCUITEMID_CHILD pidl, const SHCOLUMNID* pscid, VARIANT* pv) override;
    STDMETHODIMP GetDetailsOf(PCUITEMID_CHILD pidl, UINT iColumn, SHELLDETAILS* psd) override;
    STDMETHODIMP MapColumnToSCID(UINT iColumn, SHCOLUMNID* pscid) override;

    // IPersist
    STDMETHODIMP GetClassID(CLSID* pClassID) override;

    // IPersistFolder
    STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidl) override;

    // IPersistFolder2
    STDMETHODIMP GetCurFolder(PIDLIST_ABSOLUTE* ppidl) override;

    // IPersistFolder3
    STDMETHODIMP InitializeEx(IBindCtx* pbc, PCIDLIST_ABSOLUTE pidlRoot,
        const PERSIST_FOLDER_TARGET_INFO* ppfti) override;
    STDMETHODIMP GetFolderTargetInfo(PERSIST_FOLDER_TARGET_INFO* ppfti) override;

    // IPersistFile - Windows uses this to pass the database file path
    STDMETHODIMP IsDirty() override;
    STDMETHODIMP Load(LPCOLESTR pszFileName, DWORD dwMode) override;
    STDMETHODIMP Save(LPCOLESTR pszFileName, BOOL fRemember) override;
    STDMETHODIMP SaveCompleted(LPCOLESTR pszFileName) override;
    STDMETHODIMP GetCurFile(LPOLESTR* ppszFileName) override;

    // IShellFolderViewCB
    STDMETHODIMP MessageSFVCB(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    // IObjectWithSite
    STDMETHODIMP SetSite(IUnknown* pUnkSite) override;
    STDMETHODIMP GetSite(REFIID riid, void** ppvSite) override;

    // Internal methods
    void SetDatabasePath(const std::wstring& path) { _DatabasePath = path; }
    const std::wstring& GetDatabasePath() const { return _DatabasePath; }
    void SetCurrentTable(const std::wstring& table) { _CurrentTable = table; }
    const std::wstring& GetCurrentTable() const { return _CurrentTable; }
    void SetDatabase(std::shared_ptr<Database> db) { _Database = db; }
    bool IsInsideTable() const { return !_CurrentTable.empty(); }
    
    // Friend for internal access
    friend class EnumIDList;

private:
    LONG _RefCount;
    PIDLIST_ABSOLUTE _PidlRoot;
    std::wstring _DatabasePath;
    std::wstring _CurrentTable;     // Current table being viewed (empty = root showing tables)
    std::shared_ptr<Database> _Database;
    ComPtr<IUnknown> _Site;
    
    // Column cache for current table (for dynamic columns)
    mutable std::vector<ColumnInfo> _CurrentColumns;
    mutable bool _ColumnsLoaded;
    
    // Record cache for GetDetailsOf - avoids repeated DB queries
    mutable std::unordered_map<INT64, DatabaseEntry> _RecordCache;
    mutable INT64 _LastCachedRowID;

    static const ItemData* GetItemData(PCUITEMID_CHILD pidl);
    PITEMID_CHILD CreateItemID(const DatabaseEntry& entry);
    PITEMID_CHILD CreateItemID(const std::wstring& name, ItemType type, 
                               const std::wstring& path, INT64 rowid,
                               INT64 recordCount, INT64 columnCount,
                               FILETIME mtime);
    bool OpenDatabase();
    void LoadColumns() const;
    const DatabaseEntry* GetCachedRecord(INT64 rowid) const;
    
    // Dynamic column support - columns change based on current table
    UINT GetColumnCount() const;
    std::wstring GetColumnName(UINT iColumn) const;
    SHCOLSTATEF GetColumnFlags(UINT iColumn) const;
};

// Item enumerator
class EnumIDList : public IEnumIDList {
public:
    EnumIDList(ShellFolder* folder, SHCONTF flags);
    virtual ~EnumIDList();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IEnumIDList
    STDMETHODIMP Next(ULONG celt, PITEMID_CHILD* rgelt, ULONG* pceltFetched) override;
    STDMETHODIMP Skip(ULONG celt) override;
    STDMETHODIMP Reset() override;
    STDMETHODIMP Clone(IEnumIDList** ppenum) override;

private:
    LONG _RefCount;
    ShellFolder* _Folder;
    SHCONTF _Flags;
    std::vector<PITEMID_CHILD> _Items;
    size_t _CurrentIndex;
    bool _Initialized;

    void Initialize();
};

// Data object for copy/export operations
class DatabaseDataObject : public IDataObject {
public:
    DatabaseDataObject();
    virtual ~DatabaseDataObject();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IDataObject
    STDMETHODIMP GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) override;
    STDMETHODIMP GetDataHere(FORMATETC* pformatetc, STGMEDIUM* pmedium) override;
    STDMETHODIMP QueryGetData(FORMATETC* pformatetc) override;
    STDMETHODIMP GetCanonicalFormatEtc(FORMATETC* pformatetcIn, FORMATETC* pformatetcOut) override;
    STDMETHODIMP SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease) override;
    STDMETHODIMP EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc) override;
    STDMETHODIMP DAdvise(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink, DWORD* pdwConnection) override;
    STDMETHODIMP DUnadvise(DWORD dwConnection) override;
    STDMETHODIMP EnumDAdvise(IEnumSTATDATA** ppenumAdvise) override;

    // Setup methods
    void SetDatabase(std::shared_ptr<Database> db) { _Database = db; }
    void SetTableName(const std::wstring& table) { _TableName = table; }
    void AddRowID(INT64 rowid) { _RowIDs.push_back(rowid); }
    void SetAllRecords(bool all) { _AllRecords = all; }

private:
    LONG _RefCount;
    std::shared_ptr<Database> _Database;
    std::wstring _TableName;
    std::vector<INT64> _RowIDs;
    bool _AllRecords;
    
    std::wstring GenerateCSVData() const;
    std::wstring GenerateJSONData() const;
    std::wstring GenerateSQLData() const;
};

} // namespace SQLiteView

#endif // SQLITEVIEW_SHELLFOLDER_H
