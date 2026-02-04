#pragma once
// ============================================================================
// XShellFolder.h - Shell Namespace Extension Infrastructure
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================
//
// Provides base classes for implementing Windows Shell Namespace Extensions
// that appear as browsable folders in Explorer, including:
//   - IShellFolder/IShellFolder2 implementation
//   - IPersistFolder implementation
//   - PIDL management utilities
//   - IEnumIDList implementation
//
// ============================================================================

#include "XShellExtension.h"

namespace Tootega::Shell
{
    // ========================================================================
    // PIDL Item Data Template
    // ========================================================================

    // Base structure for PIDL item data - derive from this for custom data
    #pragma pack(push, 1)
    struct XPidlItemBase
    {
        USHORT  cb;             // Total size including cb
        USHORT  Signature;      // Magic signature to identify our PIDLs
        BYTE    Type;           // Item type (user-defined)
        // Derived structures add more fields here

        static constexpr USHORT TERMINATOR_SIZE = sizeof(USHORT);
    };
    #pragma pack(pop)

    // ========================================================================
    // PIDL Utilities
    // ========================================================================

    class XPidlManager final
    {
    public:
        XPidlManager() = delete;

        // Create a PIDL from raw data
        [[nodiscard]] static PITEMID_CHILD CreateItemID(const void* pData, size_t pSize);
        
        // Clone a PIDL
        [[nodiscard]] static PITEMID_CHILD CloneItemID(PCUITEMID_CHILD pPidl);
        [[nodiscard]] static PIDLIST_ABSOLUTE CloneAbsoluteIDList(PCIDLIST_ABSOLUTE pPidl);
        
        // Concatenate PIDLs
        [[nodiscard]] static PIDLIST_ABSOLUTE AppendItemID(
            PCIDLIST_ABSOLUTE pParent, 
            PCUITEMID_CHILD pChild);
        
        // Get PIDL size
        [[nodiscard]] static UINT GetPidlSize(PCUIDLIST_RELATIVE pPidl) noexcept;
        
        // Check if PIDL is empty
        [[nodiscard]] static bool IsEmpty(PCUIDLIST_RELATIVE pPidl) noexcept;
        
        // Get next item in PIDL
        [[nodiscard]] static PCUIDLIST_RELATIVE GetNextItem(PCUIDLIST_RELATIVE pPidl) noexcept;
        
        // Get last item in PIDL
        [[nodiscard]] static PCUITEMID_CHILD GetLastItem(PCIDLIST_ABSOLUTE pPidl) noexcept;
        
        // Validate PIDL signature
        template<typename TItemData>
        [[nodiscard]] static const TItemData* GetItemData(PCUITEMID_CHILD pPidl, USHORT pSignature) noexcept
        {
            if (!pPidl || pPidl->mkid.cb < sizeof(TItemData))
                return nullptr;
            
            auto data = reinterpret_cast<const TItemData*>(pPidl->mkid.abID);
            if (data->Signature != pSignature)
                return nullptr;
            
            return data;
        }
    };

    // ========================================================================
    // Shell Folder Column Definition
    // ========================================================================

    struct XShellColumn
    {
        SHCOLUMNID      ID;
        std::wstring    Title;
        SHCOLSTATEF     State;          // SHCOLSTATE_* flags
        int             DefaultWidth;   // In characters
        int             Format;         // LVCFMT_* flags

        XShellColumn() noexcept
            : ID{}
            , State(SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT)
            , DefaultWidth(15)
            , Format(LVCFMT_LEFT)
        {
        }
    };

    // ========================================================================
    // Shell Folder Item Attributes
    // ========================================================================

    namespace XShellAttributes
    {
        // Common attribute combinations
        constexpr SFGAOF Folder = SFGAO_FOLDER | SFGAO_HASSUBFOLDER | SFGAO_BROWSABLE;
        constexpr SFGAOF File = SFGAO_STREAM;
        constexpr SFGAOF BrowsableFolder = SFGAO_FOLDER | SFGAO_BROWSABLE | SFGAO_DROPTARGET;
        constexpr SFGAOF ReadOnlyFile = SFGAO_STREAM | SFGAO_READONLY;
    }

    // ========================================================================
    // IEnumIDList Implementation Helper
    // ========================================================================

    class XEnumIDList : public IEnumIDList
    {
    public:
        XEnumIDList();
        virtual ~XEnumIDList();

        // IUnknown
        STDMETHODIMP QueryInterface(REFIID pRiid, void** pPpv) override;
        STDMETHODIMP_(ULONG) AddRef() override;
        STDMETHODIMP_(ULONG) Release() override;

        // IEnumIDList
        STDMETHODIMP Next(ULONG pCelt, PITEMID_CHILD* pRgelt, ULONG* pPceltFetched) override;
        STDMETHODIMP Skip(ULONG pCelt) override;
        STDMETHODIMP Reset() override;
        STDMETHODIMP Clone(IEnumIDList** pPenum) override;

        // Add items to enumerate
        void AddItem(PITEMID_CHILD pPidl);
        void SetItems(std::vector<PITEMID_CHILD>&& pItems);

    private:
        LONG                            _RefCount;
        std::vector<PITEMID_CHILD>      _Items;
        size_t                          _CurrentIndex;
    };

    // ========================================================================
    // XShellFolderBase - Base class for shell folder implementations
    // ========================================================================

    class XShellFolderBase : 
        public XComObject<
            IShellFolder2,
            IPersistFolder3,
            IPersistFile,
            IShellFolderViewCB,
            IObjectWithSite>
    {
    public:
        XShellFolderBase();
        virtual ~XShellFolderBase();

        // IUnknown
        STDMETHODIMP QueryInterface(REFIID pRiid, void** pPpv) override;

        // IPersist
        STDMETHODIMP GetClassID(CLSID* pClassID) override;

        // IPersistFolder
        STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE pPidl) override;

        // IPersistFolder2
        STDMETHODIMP GetCurFolder(PIDLIST_ABSOLUTE* pPidl) override;

        // IPersistFolder3
        STDMETHODIMP InitializeEx(
            IBindCtx* pBc, 
            PCIDLIST_ABSOLUTE pPidlRoot,
            const PERSIST_FOLDER_TARGET_INFO* pPfti) override;
        STDMETHODIMP GetFolderTargetInfo(PERSIST_FOLDER_TARGET_INFO* pPfti) override;

        // IPersistFile
        STDMETHODIMP IsDirty() override;
        STDMETHODIMP Load(LPCOLESTR pszFileName, DWORD pDwMode) override;
        STDMETHODIMP Save(LPCOLESTR pszFileName, BOOL pFRemember) override;
        STDMETHODIMP SaveCompleted(LPCOLESTR pszFileName) override;
        STDMETHODIMP GetCurFile(LPOLESTR* ppszFileName) override;

        // IShellFolder
        STDMETHODIMP ParseDisplayName(
            HWND pHwnd, 
            IBindCtx* pBc, 
            LPWSTR pszDisplayName,
            ULONG* pChEaten, 
            PIDLIST_RELATIVE* pPidl, 
            ULONG* pDwAttributes) override;
        
        STDMETHODIMP EnumObjects(
            HWND pHwnd, 
            SHCONTF pGrfFlags, 
            IEnumIDList** pEnumIDList) override;
        
        STDMETHODIMP BindToObject(
            PCUIDLIST_RELATIVE pPidl, 
            IBindCtx* pBc, 
            REFIID pRiid, 
            void** pPv) override;
        
        STDMETHODIMP BindToStorage(
            PCUIDLIST_RELATIVE pPidl, 
            IBindCtx* pBc, 
            REFIID pRiid, 
            void** pPv) override;
        
        STDMETHODIMP CompareIDs(
            LPARAM pLParam, 
            PCUIDLIST_RELATIVE pPidl1, 
            PCUIDLIST_RELATIVE pPidl2) override;
        
        STDMETHODIMP CreateViewObject(
            HWND pHwndOwner, 
            REFIID pRiid, 
            void** pPv) override;
        
        STDMETHODIMP GetAttributesOf(
            UINT pCidl, 
            PCUITEMID_CHILD_ARRAY pApidl, 
            SFGAOF* pRgfInOut) override;
        
        STDMETHODIMP GetUIObjectOf(
            HWND pHwndOwner, 
            UINT pCidl, 
            PCUITEMID_CHILD_ARRAY pApidl,
            REFIID pRiid, 
            UINT* pRgfReserved, 
            void** pPv) override;
        
        STDMETHODIMP GetDisplayNameOf(
            PCUITEMID_CHILD pPidl, 
            SHGDNF pUFlags, 
            STRRET* pName) override;
        
        STDMETHODIMP SetNameOf(
            HWND pHwnd, 
            PCUITEMID_CHILD pPidl, 
            LPCWSTR pszName,
            SHGDNF pUFlags, 
            PITEMID_CHILD* pPidlOut) override;

        // IShellFolder2
        STDMETHODIMP GetDefaultSearchGUID(GUID* pGuid) override;
        STDMETHODIMP EnumSearches(IEnumExtraSearch** pEnum) override;
        STDMETHODIMP GetDefaultColumn(DWORD pDwRes, ULONG* pSort, ULONG* pDisplay) override;
        STDMETHODIMP GetDefaultColumnState(UINT pColumn, SHCOLSTATEF* pCsFlags) override;
        STDMETHODIMP GetDetailsEx(PCUITEMID_CHILD pPidl, const SHCOLUMNID* pScid, VARIANT* pV) override;
        STDMETHODIMP GetDetailsOf(PCUITEMID_CHILD pPidl, UINT pColumn, SHELLDETAILS* pSd) override;
        STDMETHODIMP MapColumnToSCID(UINT pColumn, SHCOLUMNID* pScid) override;

        // IShellFolderViewCB
        STDMETHODIMP MessageSFVCB(UINT pMsg, WPARAM pWParam, LPARAM pLParam) override;

        // IObjectWithSite
        STDMETHODIMP SetSite(IUnknown* pUnkSite) override;
        STDMETHODIMP GetSite(REFIID pRiid, void** pPvSite) override;

    protected:
        // Override in derived classes
        virtual const GUID& GetClassGUID() const = 0;
        virtual bool OnLoad(std::wstring_view pFilePath) = 0;
        virtual bool OnInitialize(PCIDLIST_ABSOLUTE pPidlRoot) = 0;
        virtual void OnGetColumns(std::vector<XShellColumn>& pColumns) = 0;
        virtual bool OnEnumObjects(SHCONTF pFlags, XEnumIDList* pEnum) = 0;
        virtual bool OnBindToObject(PCUIDLIST_RELATIVE pPidl, REFIID pRiid, void** pPv) = 0;
        virtual SFGAOF OnGetAttributes(PCUITEMID_CHILD pPidl) = 0;
        virtual std::wstring OnGetDisplayName(PCUITEMID_CHILD pPidl, SHGDNF pFlags) = 0;
        virtual bool OnGetDetailsOf(PCUITEMID_CHILD pPidl, UINT pColumn, SHELLDETAILS* pDetails) = 0;
        virtual short OnCompareItems(PCUITEMID_CHILD pPidl1, PCUITEMID_CHILD pPidl2, UINT pColumn);

        // Helpers
        [[nodiscard]] const std::wstring& GetFilePath() const noexcept { return _FilePath; }
        [[nodiscard]] PIDLIST_ABSOLUTE GetRootPidl() const noexcept { return _PidlRoot; }
        [[nodiscard]] IUnknown* GetSite() const noexcept { return _Site.Get(); }

        // STRRET helper
        static HRESULT SetStrRet(STRRET* pSr, const wchar_t* pStr);

    private:
        std::wstring                    _FilePath;
        PIDLIST_ABSOLUTE                _PidlRoot;
        std::vector<XShellColumn>       _Columns;
        XComPtr<IUnknown>               _Site;
    };

} // namespace Tootega::Shell

