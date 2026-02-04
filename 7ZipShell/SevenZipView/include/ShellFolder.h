#pragma once
/*
** SevenZipView - Windows Explorer Shell Extension
** Shell Folder Implementation
*/

#ifndef SEVENZIPVIEW_SHELLFOLDER_H
#define SEVENZIPVIEW_SHELLFOLDER_H

#include "Common.h"
#include "Archive.h"

namespace SevenZipView {

// PIDL item data structure
#pragma pack(push, 1)
struct ItemData {
    USHORT cb;              // Total size including cb
    USHORT signature;       // 0x375A ('7Z' for 7-Zip)
    ItemType type;
    WCHAR name[260];        // File/folder name
    WCHAR path[512];        // Full path inside archive
    UINT64 size;            // Uncompressed size
    UINT64 compressedSize;  // Compressed size
    UINT32 archiveIndex;    // Index in archive
    UINT32 crc;             // CRC32
    UINT32 attributes;      // File attributes
    FILETIME modifiedTime;  // Modification time
    BYTE reserved[16];
    
    static const USHORT SIGNATURE = 0x375A; // '7Z'
    
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

// Shell Folder - Implements navigation inside the 7z archive
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

    // IPersistFile - Windows uses this to pass the archive file path
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
    void SetArchivePath(const std::wstring& path) { _ArchivePath = path; }
    const std::wstring& GetArchivePath() const { return _ArchivePath; }
    void SetCurrentFolder(const std::wstring& path) { _CurrentFolder = path; }
    const std::wstring& GetCurrentFolder() const { return _CurrentFolder; }
    void SetArchive(std::shared_ptr<Archive> archive) { _Archive = archive; }
    bool IsSubfolder() const { return !_CurrentFolder.empty(); }
    
    // Friend for internal access
    friend class EnumIDList;

private:
    LONG _RefCount;
    PIDLIST_ABSOLUTE _PidlRoot;
    std::wstring _ArchivePath;
    std::wstring _CurrentFolder;    // Current folder path inside archive (empty = root)
    std::shared_ptr<Archive> _Archive;
    ComPtr<IUnknown> _Site;

    static const ItemData* GetItemData(PCUITEMID_CHILD pidl);
    PITEMID_CHILD CreateItemID(const ArchiveEntry& entry);
    PITEMID_CHILD CreateItemID(const std::wstring& name, ItemType type, 
                                const std::wstring& path, UINT64 size, 
                                UINT64 compressedSize, UINT32 index,
                                UINT32 crc, UINT32 attrs, FILETIME mtime);
    bool OpenArchive();
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

// Data object for drag-drop and clipboard operations
class ArchiveDataObject : public IDataObject {
public:
    ArchiveDataObject();
    virtual ~ArchiveDataObject();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IDataObject
    STDMETHODIMP GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) override;
    STDMETHODIMP GetDataHere(FORMATETC* pformatetc, STGMEDIUM* pmedium) override;
    STDMETHODIMP QueryGetData(FORMATETC* pformatetc) override;
    STDMETHODIMP GetCanonicalFormatEtc(FORMATETC* pformatectIn, FORMATETC* pformatetcOut) override;
    STDMETHODIMP SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease) override;
    STDMETHODIMP EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc) override;
    STDMETHODIMP DAdvise(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink, DWORD* pdwConnection) override;
    STDMETHODIMP DUnadvise(DWORD dwConnection) override;
    STDMETHODIMP EnumDAdvise(IEnumSTATDATA** ppenumAdvise) override;

    // Setup
    void SetArchive(const std::wstring& archivePath, std::shared_ptr<Archive> archive,
                    std::vector<std::pair<UINT32, std::wstring>>&& items);

private:
    LONG _RefCount;
    std::wstring _ArchivePath;
    std::shared_ptr<Archive> _Archive;
    std::vector<std::pair<UINT32, std::wstring>> _Items;  // archiveIndex, path
    std::wstring _TempFolder;
    std::vector<std::wstring> _ExtractedFiles;
    bool _Extracted;

    bool ExtractToTemp();
    HGLOBAL CreateHDrop();
};

} // namespace SevenZipView

#endif // SEVENZIPVIEW_SHELLFOLDER_H
