#pragma once
// ============================================================================
// XPreviewHandler.h - Shell Preview Handler Infrastructure
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================
//
// Provides base classes for implementing Windows preview handlers that
// display file content in Explorer's preview pane.
//
// ============================================================================

#include "XShellExtension.h"

namespace Tootega::Shell
{
    // ========================================================================
    // Preview Visual Settings
    // ========================================================================

    struct XPreviewVisuals
    {
        COLORREF    BackgroundColor;
        COLORREF    TextColor;
        LOGFONTW    Font;
        bool        FontSet;

        XPreviewVisuals() noexcept
            : BackgroundColor(RGB(255, 255, 255))
            , TextColor(RGB(0, 0, 0))
            , FontSet(false)
        {
            ZeroMemory(&Font, sizeof(Font));
        }
    };

    // ========================================================================
    // XPreviewHandlerBase - Base class for preview handlers
    // ========================================================================

    class XPreviewHandlerBase : 
        public XComObject<
            IPreviewHandler,
            IPreviewHandlerVisuals,
            IOleWindow,
            IObjectWithSite,
            IInitializeWithFile>
    {
    public:
        XPreviewHandlerBase();
        virtual ~XPreviewHandlerBase();

        // IUnknown
        STDMETHODIMP QueryInterface(REFIID pRiid, void** pPpv) override;

        // IPreviewHandler
        STDMETHODIMP SetWindow(HWND pHwnd, const RECT* pPrc) override;
        STDMETHODIMP SetRect(const RECT* pPrc) override;
        STDMETHODIMP DoPreview() override;
        STDMETHODIMP Unload() override;
        STDMETHODIMP SetFocus() override;
        STDMETHODIMP QueryFocus(HWND* pPhwnd) override;
        STDMETHODIMP TranslateAccelerator(MSG* pPmsg) override;

        // IPreviewHandlerVisuals
        STDMETHODIMP SetBackgroundColor(COLORREF pColor) override;
        STDMETHODIMP SetFont(const LOGFONTW* pPlf) override;
        STDMETHODIMP SetTextColor(COLORREF pColor) override;

        // IOleWindow
        STDMETHODIMP GetWindow(HWND* pPhwnd) override;
        STDMETHODIMP ContextSensitiveHelp(BOOL pFEnterMode) override;

        // IObjectWithSite
        STDMETHODIMP SetSite(IUnknown* pUnkSite) override;
        STDMETHODIMP GetSite(REFIID pRiid, void** pPvSite) override;

        // IInitializeWithFile
        STDMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD pGrfMode) override;

    protected:
        // Override in derived classes
        virtual bool OnInitialize(std::wstring_view pFilePath) = 0;
        virtual bool OnCreatePreviewWindow(HWND pParent, const RECT& pRect) = 0;
        virtual void OnDestroyPreviewWindow() = 0;
        virtual void OnRenderPreview(HDC pHdc, const RECT& pRect) = 0;
        virtual void OnResize(const RECT& pRect);
        virtual void OnVisualsChanged(const XPreviewVisuals& pVisuals);

        // Helpers
        [[nodiscard]] const std::wstring& GetFilePath() const noexcept { return _FilePath; }
        [[nodiscard]] HWND GetParentWindow() const noexcept { return _ParentHwnd; }
        [[nodiscard]] HWND GetPreviewWindow() const noexcept { return _PreviewHwnd; }
        [[nodiscard]] const RECT& GetRect() const noexcept { return _Rect; }
        [[nodiscard]] const XPreviewVisuals& GetVisuals() const noexcept { return _Visuals; }

        void SetPreviewWindow(HWND pHwnd) noexcept { _PreviewHwnd = pHwnd; }

    private:
        std::wstring        _FilePath;
        HWND                _ParentHwnd;
        HWND                _PreviewHwnd;
        RECT                _Rect;
        XPreviewVisuals     _Visuals;
        XComPtr<IUnknown>   _Site;
    };

} // namespace Tootega::Shell

