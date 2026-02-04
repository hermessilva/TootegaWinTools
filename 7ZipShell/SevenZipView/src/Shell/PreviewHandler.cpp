/*
** SevenZipView - Windows Explorer Shell Extension for 7z Archives
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
**
** Preview Handler Implementation
*/

#include "PreviewHandler.h"
#include <strsafe.h>

namespace SevenZipView {

//==============================================================================
// PreviewHandler
//==============================================================================

PreviewHandler::PreviewHandler()
    : _RefCount(1)
    , _ParentHwnd(nullptr)
    , _PreviewHwnd(nullptr)
    , _Rect{}
    , _ItemIndex(0)
    , _BackgroundColor(RGB(255, 255, 255))
    , _TextColor(RGB(0, 0, 0))
    , _Font{}
    , _ContentLoaded(false) {
    
    InterlockedIncrement(&g_DllRefCount);
    
    // Default font
    _Font.lfHeight = -12;
    _Font.lfWeight = FW_NORMAL;
    StringCchCopyW(_Font.lfFaceName, LF_FACESIZE, L"Segoe UI");
}

PreviewHandler::~PreviewHandler() {
    DestroyPreviewWindow();
    InterlockedDecrement(&g_DllRefCount);
}

// IUnknown
STDMETHODIMP PreviewHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    
    if (riid == IID_IUnknown)
        *ppv = static_cast<IUnknown*>(static_cast<IPreviewHandler*>(this));
    else if (riid == IID_IPreviewHandler)
        *ppv = static_cast<IPreviewHandler*>(this);
    else if (riid == IID_IPreviewHandlerVisuals)
        *ppv = static_cast<IPreviewHandlerVisuals*>(this);
    else if (riid == IID_IOleWindow)
        *ppv = static_cast<IOleWindow*>(this);
    else if (riid == IID_IObjectWithSite)
        *ppv = static_cast<IObjectWithSite*>(this);
    else if (riid == IID_IInitializeWithFile)
        *ppv = static_cast<IInitializeWithFile*>(this);
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) PreviewHandler::AddRef() {
    return InterlockedIncrement(&_RefCount);
}

STDMETHODIMP_(ULONG) PreviewHandler::Release() {
    LONG ref = InterlockedDecrement(&_RefCount);
    if (ref == 0)
        delete this;
    return ref;
}

// IInitializeWithFile
STDMETHODIMP PreviewHandler::Initialize(LPCWSTR pszFilePath, DWORD grfMode) {
    if (!pszFilePath) return E_POINTER;
    _ArchivePath = pszFilePath;
    return S_OK;
}

// IPreviewHandler
STDMETHODIMP PreviewHandler::SetWindow(HWND hwnd, const RECT* prc) {
    _ParentHwnd = hwnd;
    if (prc)
        _Rect = *prc;
    
    if (_PreviewHwnd) {
        SetParent(_PreviewHwnd, _ParentHwnd);
        SetWindowPos(_PreviewHwnd, nullptr, _Rect.left, _Rect.top,
            _Rect.right - _Rect.left, _Rect.bottom - _Rect.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
    
    return S_OK;
}

STDMETHODIMP PreviewHandler::SetRect(const RECT* prc) {
    if (!prc) return E_POINTER;
    _Rect = *prc;
    
    if (_PreviewHwnd) {
        SetWindowPos(_PreviewHwnd, nullptr, _Rect.left, _Rect.top,
            _Rect.right - _Rect.left, _Rect.bottom - _Rect.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
    
    return S_OK;
}

STDMETHODIMP PreviewHandler::DoPreview() {
    if (!_ParentHwnd) return E_FAIL;
    
    CreatePreviewWindow();
    
    // Load archive info
    if (!_ArchivePath.empty() && !_ContentLoaded) {
        LoadPreviewContent(_PreviewContent);
        _ContentLoaded = true;
    }
    
    if (_PreviewHwnd)
        InvalidateRect(_PreviewHwnd, nullptr, TRUE);
    
    return S_OK;
}

STDMETHODIMP PreviewHandler::Unload() {
    DestroyPreviewWindow();
    _ArchivePath.clear();
    _ItemPath.clear();
    _PreviewContent.clear();
    _ContentLoaded = false;
    return S_OK;
}

STDMETHODIMP PreviewHandler::SetFocus() {
    if (_PreviewHwnd)
        ::SetFocus(_PreviewHwnd);
    return S_OK;
}

STDMETHODIMP PreviewHandler::QueryFocus(HWND* phwnd) {
    if (!phwnd) return E_POINTER;
    *phwnd = GetFocus();
    return S_OK;
}

STDMETHODIMP PreviewHandler::TranslateAccelerator(MSG* pmsg) {
    return S_FALSE;
}

// IPreviewHandlerVisuals
STDMETHODIMP PreviewHandler::SetBackgroundColor(COLORREF color) {
    _BackgroundColor = color;
    if (_PreviewHwnd)
        InvalidateRect(_PreviewHwnd, nullptr, TRUE);
    return S_OK;
}

STDMETHODIMP PreviewHandler::SetFont(const LOGFONTW* plf) {
    if (plf)
        _Font = *plf;
    if (_PreviewHwnd)
        InvalidateRect(_PreviewHwnd, nullptr, TRUE);
    return S_OK;
}

STDMETHODIMP PreviewHandler::SetTextColor(COLORREF color) {
    _TextColor = color;
    if (_PreviewHwnd)
        InvalidateRect(_PreviewHwnd, nullptr, TRUE);
    return S_OK;
}

// IOleWindow
STDMETHODIMP PreviewHandler::GetWindow(HWND* phwnd) {
    if (!phwnd) return E_POINTER;
    *phwnd = _PreviewHwnd;
    return _PreviewHwnd ? S_OK : E_FAIL;
}

STDMETHODIMP PreviewHandler::ContextSensitiveHelp(BOOL fEnterMode) {
    return E_NOTIMPL;
}

// IObjectWithSite
STDMETHODIMP PreviewHandler::SetSite(IUnknown* pUnkSite) {
    _Site.Release();
    if (pUnkSite)
        _Site = pUnkSite;
    return S_OK;
}

STDMETHODIMP PreviewHandler::GetSite(REFIID riid, void** ppvSite) {
    if (!ppvSite) return E_POINTER;
    if (!_Site) {
        *ppvSite = nullptr;
        return E_FAIL;
    }
    return _Site->QueryInterface(riid, ppvSite);
}

// Window procedure
LRESULT CALLBACK PreviewHandler::PreviewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PreviewHandler* pThis = reinterpret_cast<PreviewHandler*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    
    switch (msg) {
    case WM_CREATE:
        {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        }
        return 0;
        
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (pThis)
                pThis->RenderPreview(hdc);
            EndPaint(hwnd, &ps);
        }
        return 0;
        
    case WM_ERASEBKGND:
        return 1;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void PreviewHandler::CreatePreviewWindow() {
    if (_PreviewHwnd) return;
    
    // Register window class
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = PreviewWndProc;
        wc.hInstance = g_hModule;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"SevenZipViewPreview";
        RegisterClassExW(&wc);
        classRegistered = true;
    }
    
    _PreviewHwnd = CreateWindowExW(
        0,
        L"SevenZipViewPreview",
        nullptr,
        WS_CHILD | WS_VISIBLE,
        _Rect.left, _Rect.top,
        _Rect.right - _Rect.left, _Rect.bottom - _Rect.top,
        _ParentHwnd, nullptr, g_hModule, this);
}

void PreviewHandler::DestroyPreviewWindow() {
    if (_PreviewHwnd) {
        DestroyWindow(_PreviewHwnd);
        _PreviewHwnd = nullptr;
    }
}

void PreviewHandler::RenderPreview(HDC hdc) {
    RECT rc;
    GetClientRect(_PreviewHwnd, &rc);
    
    // Fill background
    HBRUSH hBrush = CreateSolidBrush(_BackgroundColor);
    FillRect(hdc, &rc, hBrush);
    DeleteObject(hBrush);
    
    // Create font
    HFONT hFont = CreateFontIndirectW(&_Font);
    HFONT hOldFont = static_cast<HFONT>(SelectObject(hdc, hFont));
    
    // Set text color
    ::SetTextColor(hdc, _TextColor);
    ::SetBkMode(hdc, TRANSPARENT);
    
    // Build info text
    std::wstringstream ss;
    if (!_ArchivePath.empty()) {
        auto archive = ArchivePool::Instance().GetArchive(_ArchivePath);
        if (archive && archive->IsOpen()) {
            ss << L"Archive: " << _ArchivePath << L"\n\n";
            ss << L"Files: " << archive->GetFileCount() << L"\n";
            ss << L"Folders: " << archive->GetFolderCount() << L"\n";
            ss << L"Total Size: " << (archive->GetTotalUncompressedSize() / 1024) << L" KB\n";
            ss << L"Compressed: " << (archive->GetTotalCompressedSize() / 1024) << L" KB\n";
        } else {
            ss << L"Unable to open archive:\n" << _ArchivePath;
        }
    } else {
        ss << L"No archive loaded";
    }
    
    std::wstring info = ss.str();
    
    // Draw text with padding
    RECT textRect = rc;
    textRect.left += 20;
    textRect.top += 20;
    textRect.right -= 20;
    textRect.bottom -= 20;
    
    DrawTextW(hdc, info.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

bool PreviewHandler::LoadPreviewContent(std::vector<BYTE>& content) {
    // For archive preview, we don't load file content
    // Just display metadata
    content.clear();
    return true;
}

} // namespace SevenZipView
