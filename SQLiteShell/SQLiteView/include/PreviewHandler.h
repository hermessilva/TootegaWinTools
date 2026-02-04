#pragma once
/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** Preview Handler - Displays table/record content in preview pane
*/

#ifndef SQLITEVIEW_PREVIEWHANDLER_H
#define SQLITEVIEW_PREVIEWHANDLER_H

#include "Common.h"
#include "Database.h"

namespace SQLiteView {

// Preview Handler - Shows database content in Explorer preview pane
class PreviewHandler : 
    public IPreviewHandler,
    public IPreviewHandlerVisuals,
    public IOleWindow,
    public IObjectWithSite,
    public IInitializeWithFile {
public:
    PreviewHandler();
    virtual ~PreviewHandler();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IPreviewHandler
    STDMETHODIMP SetWindow(HWND hwnd, const RECT* prc) override;
    STDMETHODIMP SetRect(const RECT* prc) override;
    STDMETHODIMP DoPreview() override;
    STDMETHODIMP Unload() override;
    STDMETHODIMP SetFocus() override;
    STDMETHODIMP QueryFocus(HWND* phwnd) override;
    STDMETHODIMP TranslateAccelerator(MSG* pmsg) override;

    // IPreviewHandlerVisuals
    STDMETHODIMP SetBackgroundColor(COLORREF color) override;
    STDMETHODIMP SetFont(const LOGFONTW* plf) override;
    STDMETHODIMP SetTextColor(COLORREF color) override;

    // IOleWindow
    STDMETHODIMP GetWindow(HWND* phwnd) override;
    STDMETHODIMP ContextSensitiveHelp(BOOL fEnterMode) override;

    // IObjectWithSite
    STDMETHODIMP SetSite(IUnknown* pUnkSite) override;
    STDMETHODIMP GetSite(REFIID riid, void** ppvSite) override;

    // IInitializeWithFile
    STDMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;

private:
    LONG _RefCount;
    HWND _ParentHwnd;
    HWND _PreviewHwnd;
    RECT _Rect;
    std::wstring _FilePath;
    std::shared_ptr<Database> _Database;
    ComPtr<IUnknown> _Site;
    
    COLORREF _BackColor;
    COLORREF _TextColor;
    LOGFONTW _Font;

    void CreatePreviewWindow();
    void DestroyPreviewWindow();
    void RenderPreview();
    void DrawDatabaseSummary(HDC hdc, const RECT& rc);
    void DrawTableList(HDC hdc, const RECT& rc);
    
    static LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static const wchar_t* PREVIEW_CLASS_NAME;
};

} // namespace SQLiteView

#endif // SQLITEVIEW_PREVIEWHANDLER_H
