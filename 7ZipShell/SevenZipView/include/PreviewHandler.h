#pragma once
/*
** SevenZipView - Windows Explorer Shell Extension
** Preview Handler
*/

#ifndef SEVENZIPVIEW_PREVIEWHANDLER_H
#define SEVENZIPVIEW_PREVIEWHANDLER_H

#include "Common.h"
#include "Archive.h"

namespace SevenZipView {

// Preview handler for files inside the archive
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

    // Internal
    void SetArchivePath(const std::wstring& path) { _ArchivePath = path; }
    void SetItemPath(const std::wstring& path) { _ItemPath = path; }
    void SetItemIndex(UINT32 index) { _ItemIndex = index; }

private:
    LONG _RefCount;
    HWND _ParentHwnd;
    HWND _PreviewHwnd;
    RECT _Rect;
    std::wstring _ArchivePath;
    std::wstring _ItemPath;
    UINT32 _ItemIndex;
    
    COLORREF _BackgroundColor;
    COLORREF _TextColor;
    LOGFONTW _Font;
    
    ComPtr<IUnknown> _Site;
    
    static LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void CreatePreviewWindow();
    void DestroyPreviewWindow();
    void RenderPreview(HDC hdc);
    bool LoadPreviewContent(std::vector<BYTE>& content);
    
    std::vector<BYTE> _PreviewContent;
    bool _ContentLoaded;
};

} // namespace SevenZipView

#endif // SEVENZIPVIEW_PREVIEWHANDLER_H
