#pragma once
// ============================================================================
// XPropertyHandler.h - Shell Property Handler Infrastructure
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================
//
// Provides base classes for implementing Windows property handlers that
// expose custom properties for files in Explorer's Details pane and columns.
//
// ============================================================================

#include "XShellExtension.h"

namespace Tootega::Shell
{
    // ========================================================================
    // Property Definition
    // ========================================================================

    struct XPropertyDefinition
    {
        PROPERTYKEY     Key;
        std::wstring    DisplayName;
        VARTYPE         Type;           // VT_LPWSTR, VT_UI8, VT_BOOL, etc.
        bool            IsReadOnly;
        SHCOLSTATEF     ColumnState;    // SHCOLSTATE_* flags

        XPropertyDefinition() noexcept
            : Key{}
            , Type(VT_EMPTY)
            , IsReadOnly(true)
            , ColumnState(SHCOLSTATE_TYPE_STR)
        {
        }
    };

    // ========================================================================
    // XPropertyHandlerBase - Base class for property handlers
    // ========================================================================

    class XPropertyHandlerBase : 
        public XComObject<IPropertyStore, IPropertyStoreCapabilities, IInitializeWithFile>
    {
    public:
        XPropertyHandlerBase();
        virtual ~XPropertyHandlerBase();

        // IUnknown
        STDMETHODIMP QueryInterface(REFIID pRiid, void** pPpv) override;

        // IInitializeWithFile
        STDMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD pGrfMode) override;

        // IPropertyStore
        STDMETHODIMP GetCount(DWORD* pCProps) override;
        STDMETHODIMP GetAt(DWORD pIProp, PROPERTYKEY* pKey) override;
        STDMETHODIMP GetValue(REFPROPERTYKEY pKey, PROPVARIANT* pPv) override;
        STDMETHODIMP SetValue(REFPROPERTYKEY pKey, REFPROPVARIANT pPropvar) override;
        STDMETHODIMP Commit() override;

        // IPropertyStoreCapabilities
        STDMETHODIMP IsPropertyWritable(REFPROPERTYKEY pKey) override;

    protected:
        // Override in derived classes
        virtual bool OnInitialize(std::wstring_view pFilePath) = 0;
        virtual void OnGetPropertyDefinitions(std::vector<XPropertyDefinition>& pDefinitions) = 0;
        virtual bool OnGetPropertyValue(const PROPERTYKEY& pKey, PROPVARIANT* pValue) = 0;
        virtual bool OnSetPropertyValue(const PROPERTYKEY& pKey, const PROPVARIANT& pValue);
        virtual bool OnCommit();

        // Helpers for derived classes
        [[nodiscard]] const std::wstring& GetFilePath() const noexcept { return _FilePath; }

        // PROPVARIANT helpers
        static void InitPropVariantFromString(PROPVARIANT* pPv, std::wstring_view pStr);
        static void InitPropVariantFromUInt64(PROPVARIANT* pPv, UINT64 pValue);
        static void InitPropVariantFromUInt32(PROPVARIANT* pPv, UINT32 pValue);
        static void InitPropVariantFromBool(PROPVARIANT* pPv, bool pValue);
        static void InitPropVariantFromFileTime(PROPVARIANT* pPv, const FILETIME& pValue);

    private:
        std::wstring                        _FilePath;
        std::vector<XPropertyDefinition>    _Properties;
        bool                                _Initialized;
    };

} // namespace Tootega::Shell

