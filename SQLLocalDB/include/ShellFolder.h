#pragma once
/*
** SQLLocalDBView - Windows Explorer Shell Extension
** Shell Folder Implementation
*/

#ifndef SQLLocalDBView_SHELLFOLDER_H
#define SQLLocalDBView_SHELLFOLDER_H

#include "Common.h"
#include "Database.h"

namespace SQLLocalDBView {

// ItemType definido em Common.h

// PIDL item data structure
#pragma pack(push, 1)
struct ItemData {
    USHORT cb;              // Tamanho total incluindo cb
    USHORT signature;       // 0x5153 ('QS' for SQL LocalDB)
    ItemType type;
    WCHAR name[256];        // Nome da tabela ou ID do registro
    INT64 rowCount;         // Para tabelas: contagem de linhas. Para rows: rowid
    USHORT columnCount;
    WCHAR tableName[128];   // Para rows: nome da tabela pai
    BYTE reserved[16];
    
    static const USHORT SIGNATURE = 0x5153;
    
    static UINT GetSize(const WCHAR* name) {
        return sizeof(ItemData) - sizeof(WCHAR) * (256 - (UINT)wcslen(name) - 1);
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
    LONG refCount_;
    CLSID clsid_;
};

// Shell Folder - Implementa navegação dentro do arquivo DB
// IMPORTANTE: Implementa IPersistFile para o Windows poder passar o caminho do arquivo
// quando o usuário dá double-click para "navegar dentro" do arquivo .db (como .zip)
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

    // IPersistFile - CRÍTICO: Windows usa isso para passar o caminho do arquivo!
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

    // Internal
    void SetDatabasePath(const std::wstring& path) { dbPath_ = path; }
    const std::wstring& GetDatabasePath() const { return dbPath_; }
    void SetTableName(const std::wstring& name) { tableName_ = name; }
    const std::wstring& GetTableName() const { return tableName_; }
    bool IsTableView() const { return !tableName_.empty(); }

    // Friend para acesso aos membros privados
    friend class EnumIDList;

private:
    LONG refCount_;
    PIDLIST_ABSOLUTE pidlRoot_;
    std::wstring dbPath_;
    std::wstring tableName_;  // Se não vazio, estamos mostrando registros desta tabela
    std::shared_ptr<Database> database_;
    ComPtr<IUnknown> site_;

    static const ItemData* GetItemData(PCUITEMID_CHILD pidl);
    PITEMID_CHILD CreateItemID(const TableInfo& table);
    PITEMID_CHILD CreateItemID(const std::wstring& name, ItemType type, int64_t rowCount, int columnCount);
    PITEMID_CHILD CreateRowItemID(int64_t rowid, const std::wstring& displayName, const std::wstring& tableName);
    bool OpenDatabase();

    // Cache dos dados da tabela atual (colunas dinamicas). Indexado por ordinal
    // (SQL Server nao tem rowid): item->rowCount guarda o ordinal 1-based da linha.
    mutable bool tableDataLoaded_ = false;
    mutable QueryResult tableData_;
    // Mantem a conexao viva enquanto esta view existir (pool usa weak_ptr; sem esta
    // ref forte cada GetDetailsOf reconectaria/reanexaria o .mdf).
    mutable std::shared_ptr<Database> tableDb_;
    void LoadTableData() const;
    UINT ColumnCount() const;  // Colunas de detalhe (root=4, tabela=colunas reais)
};

// Enumerador de itens
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
    LONG refCount_;
    ShellFolder* folder_;
    SHCONTF flags_;
    std::vector<PITEMID_CHILD> items_;
    size_t currentIndex_;
    bool initialized_;

    void Initialize();
};

} // namespace SQLLocalDBView

#endif // SQLLocalDBView_SHELLFOLDER_H
