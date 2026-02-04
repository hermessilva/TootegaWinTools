#pragma once
// ============================================================================
// XIconHandler.h - Shell Icon Handler Infrastructure
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================
//
// Provides base classes for implementing Windows icon handlers that
// provide custom icons for file types in Explorer.
//
// ============================================================================

#include "XShellExtension.h"

namespace Tootega::Shell
{
    // ========================================================================
    // Icon Extraction Options
    // ========================================================================

    struct XIconOptions
    {
        int     DefaultIconIndex;   // Index in module resources for default icon
        bool    UseFileTypeIcon;    // Use system icon for file type
        HICON   CachedIcon;         // Pre-loaded icon handle

        XIconOptions() noexcept
            : DefaultIconIndex(0)
            , UseFileTypeIcon(false)
            , CachedIcon(nullptr)
        {
        }
    };

    // ========================================================================
    // XIconHandlerBase - Base class for icon handlers
    // ========================================================================

    class XIconHandlerBase : 
        public XComObject<IExtractIconW, IPersistFile>
    {
    public:
        XIconHandlerBase();
        virtual ~XIconHandlerBase();

        // IUnknown
        STDMETHODIMP QueryInterface(REFIID pRiid, void** pPpv) override;

        // IPersistFile
        STDMETHODIMP GetClassID(CLSID* pClassID) override;
        STDMETHODIMP IsDirty() override;
        STDMETHODIMP Load(LPCOLESTR pszFileName, DWORD pDwMode) override;
        STDMETHODIMP Save(LPCOLESTR pszFileName, BOOL pFRemember) override;
        STDMETHODIMP SaveCompleted(LPCOLESTR pszFileName) override;
        STDMETHODIMP GetCurFile(LPOLESTR* ppszFileName) override;

        // IExtractIconW
        STDMETHODIMP GetIconLocation(
            UINT   pUFlags,
            LPWSTR pszIconFile,
            UINT   pCchMax,
            int*   pPiIndex,
            UINT*  pPwFlags) override;

        STDMETHODIMP Extract(
            LPCWSTR pszFile,
            UINT    pNIconIndex,
            HICON*  phiconLarge,
            HICON*  phiconSmall,
            UINT    pNIconSize) override;

    protected:
        // Override in derived classes
        virtual const GUID& GetClassGUID() const = 0;
        virtual bool OnLoad(std::wstring_view pFilePath) = 0;
        virtual bool OnGetIconLocation(XIconOptions& pOptions) = 0;
        virtual bool OnExtractIcon(int pSize, HICON* pIcon);

        // Helpers
        [[nodiscard]] const std::wstring& GetFilePath() const noexcept { return _FilePath; }

    private:
        std::wstring _FilePath;
    };

} // namespace Tootega::Shell

