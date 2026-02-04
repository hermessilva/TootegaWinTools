/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** Preview Handler Implementation
*/

#include "PreviewHandler.h"
#include <windowsx.h>

namespace SQLiteView {

const wchar_t* PreviewHandler::PREVIEW_CLASS_NAME = L"SQLiteViewPreview";

PreviewHandler::PreviewHandler()
    : _RefCount(1)
    , _ParentHwnd(nullptr)
    , _PreviewHwnd(nullptr)
    , _BackColor(RGB(255, 255, 255))
    , _TextColor(RGB(0, 0, 0)) {
    ZeroMemory(&_Rect, sizeof(_Rect));
    ZeroMemory(&_Font, sizeof(_Font));
    
    // Default font
    _Font.lfHeight = -14;
    _Font.lfWeight = FW_NORMAL;
    StringCchCopyW(_Font.lfFaceName, LF_FACESIZE, L"Consolas");
}

PreviewHandler::~PreviewHandler() {
    DestroyPreviewWindow();
}

STDMETHODIMP PreviewHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown)) {
        *ppv = static_cast<IPreviewHandler*>(this);
    }
    else if (IsEqualIID(riid, IID_IPreviewHandler)) {
        *ppv = static_cast<IPreviewHandler*>(this);
    }
    else if (IsEqualIID(riid, IID_IPreviewHandlerVisuals)) {
        *ppv = static_cast<IPreviewHandlerVisuals*>(this);
    }
    else if (IsEqualIID(riid, IID_IOleWindow)) {
        *ppv = static_cast<IOleWindow*>(this);
    }
    else if (IsEqualIID(riid, IID_IObjectWithSite)) {
        *ppv = static_cast<IObjectWithSite*>(this);
    }
    else if (IsEqualIID(riid, IID_IInitializeWithFile)) {
        *ppv = static_cast<IInitializeWithFile*>(this);
    }
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
    LONG count = InterlockedDecrement(&_RefCount);
    if (count == 0) delete this;
    return count;
}

// IPreviewHandler
STDMETHODIMP PreviewHandler::SetWindow(HWND hwnd, const RECT* prc) {
    if (!hwnd || !prc) return E_INVALIDARG;
    
    _ParentHwnd = hwnd;
    _Rect = *prc;
    
    if (_PreviewHwnd) {
        SetParent(_PreviewHwnd, _ParentHwnd);
        SetWindowPos(_PreviewHwnd, nullptr, 
                     _Rect.left, _Rect.top,
                     _Rect.right - _Rect.left, _Rect.bottom - _Rect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    
    return S_OK;
}

STDMETHODIMP PreviewHandler::SetRect(const RECT* prc) {
    if (!prc) return E_INVALIDARG;
    
    _Rect = *prc;
    
    if (_PreviewHwnd) {
        SetWindowPos(_PreviewHwnd, nullptr,
                     _Rect.left, _Rect.top,
                     _Rect.right - _Rect.left, _Rect.bottom - _Rect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    
    return S_OK;
}

STDMETHODIMP PreviewHandler::DoPreview() {
    if (_FilePath.empty()) return E_FAIL;
    
    // Open database
    _Database = DatabasePool::Instance().GetDatabase(_FilePath);
    if (!_Database) return E_FAIL;
    
    CreatePreviewWindow();
    RenderPreview();
    
    return S_OK;
}

STDMETHODIMP PreviewHandler::Unload() {
    DestroyPreviewWindow();
    _Database.reset();
    _FilePath.clear();
    return S_OK;
}

STDMETHODIMP PreviewHandler::SetFocus() {
    if (_PreviewHwnd) {
        ::SetFocus(_PreviewHwnd);
    }
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
    _BackColor = color;
    if (_PreviewHwnd) InvalidateRect(_PreviewHwnd, nullptr, TRUE);
    return S_OK;
}

STDMETHODIMP PreviewHandler::SetFont(const LOGFONTW* plf) {
    if (!plf) return E_POINTER;
    _Font = *plf;
    if (_PreviewHwnd) InvalidateRect(_PreviewHwnd, nullptr, TRUE);
    return S_OK;
}

STDMETHODIMP PreviewHandler::SetTextColor(COLORREF color) {
    _TextColor = color;
    if (_PreviewHwnd) InvalidateRect(_PreviewHwnd, nullptr, TRUE);
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

// IInitializeWithFile
STDMETHODIMP PreviewHandler::Initialize(LPCWSTR pszFilePath, DWORD grfMode) {
    if (!pszFilePath) return E_POINTER;
    _FilePath = pszFilePath;
    return S_OK;
}

void PreviewHandler::CreatePreviewWindow() {
    if (_PreviewHwnd) return;
    if (!_ParentHwnd) return;
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = PreviewWndProc;
    wc.hInstance = g_hModule;
    wc.lpszClassName = PREVIEW_CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(_BackColor);
    
    RegisterClassExW(&wc);
    
    // Create window
    _PreviewHwnd = CreateWindowExW(
        0,
        PREVIEW_CLASS_NAME,
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL,
        _Rect.left, _Rect.top,
        _Rect.right - _Rect.left, _Rect.bottom - _Rect.top,
        _ParentHwnd,
        nullptr,
        g_hModule,
        this
    );
}

void PreviewHandler::DestroyPreviewWindow() {
    if (_PreviewHwnd) {
        DestroyWindow(_PreviewHwnd);
        _PreviewHwnd = nullptr;
    }
}

void PreviewHandler::RenderPreview() {
    if (!_PreviewHwnd) return;
    InvalidateRect(_PreviewHwnd, nullptr, TRUE);
}

void PreviewHandler::DrawDatabaseSummary(HDC hdc, const RECT& rc) {
    if (!_Database) return;
    
    auto stats = _Database->GetStatistics();
    
    // Create font
    HFONT hFont = CreateFontIndirectW(&_Font);
    HFONT hOldFont = static_cast<HFONT>(SelectObject(hdc, hFont));
    
    SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, _TextColor);
    
    int y = rc.top + 10;
    int lineHeight = 20;
    int x = rc.left + 10;
    
    // Title
    LOGFONTW titleFont = _Font;
    titleFont.lfHeight = -18;
    titleFont.lfWeight = FW_BOLD;
    HFONT hTitleFont = CreateFontIndirectW(&titleFont);
    SelectObject(hdc, hTitleFont);
    
    std::wstring title = L"SQLite Database";
    TextOutW(hdc, x, y, title.c_str(), static_cast<int>(title.length()));
    y += lineHeight + 10;
    
    SelectObject(hdc, hFont);
    
    // File info
    std::wstring info;
    
    info = L"Tables: " + std::to_wstring(stats.TableCount);
    TextOutW(hdc, x, y, info.c_str(), static_cast<int>(info.length()));
    y += lineHeight;
    
    info = L"Views: " + std::to_wstring(stats.ViewCount);
    TextOutW(hdc, x, y, info.c_str(), static_cast<int>(info.length()));
    y += lineHeight;
    
    info = L"Indexes: " + std::to_wstring(stats.IndexCount);
    TextOutW(hdc, x, y, info.c_str(), static_cast<int>(info.length()));
    y += lineHeight;
    
    info = L"Triggers: " + std::to_wstring(stats.TriggerCount);
    TextOutW(hdc, x, y, info.c_str(), static_cast<int>(info.length()));
    y += lineHeight;
    
    info = L"Total Records: " + std::to_wstring(stats.TotalRecords);
    TextOutW(hdc, x, y, info.c_str(), static_cast<int>(info.length()));
    y += lineHeight + 10;
    
    info = L"File Size: " + FormatFileSize(stats.FileSize);
    TextOutW(hdc, x, y, info.c_str(), static_cast<int>(info.length()));
    y += lineHeight;
    
    info = L"Page Size: " + std::to_wstring(stats.PageSize) + L" bytes";
    TextOutW(hdc, x, y, info.c_str(), static_cast<int>(info.length()));
    y += lineHeight;
    
    info = L"Encoding: " + stats.Encoding;
    TextOutW(hdc, x, y, info.c_str(), static_cast<int>(info.length()));
    y += lineHeight;
    
    info = L"SQLite Version: " + _Database->GetSQLiteVersion();
    TextOutW(hdc, x, y, info.c_str(), static_cast<int>(info.length()));
    y += lineHeight + 20;
    
    // Table list header
    SelectObject(hdc, hTitleFont);
    title = L"Tables";
    TextOutW(hdc, x, y, title.c_str(), static_cast<int>(title.length()));
    y += lineHeight + 5;
    
    SelectObject(hdc, hFont);
    
    // Draw horizontal line
    MoveToEx(hdc, x, y, nullptr);
    LineTo(hdc, rc.right - 10, y);
    y += 5;
    
    // Table list
    DrawTableList(hdc, RECT{ x, y, rc.right - 10, rc.bottom - 10 });
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hTitleFont);
}

void PreviewHandler::DrawTableList(HDC hdc, const RECT& rc) {
    if (!_Database) return;
    
    auto tables = _Database->GetTables(false); // Exclude system tables
    auto views = _Database->GetViews();
    
    int y = rc.top;
    int lineHeight = 18;
    
    // Column headers
    std::wstring header = L"Name                    Type      Records   Columns";
    TextOutW(hdc, rc.left, y, header.c_str(), static_cast<int>(header.length()));
    y += lineHeight;
    
    // Tables
    for (const auto& table : tables) {
        if (y > rc.bottom) break;
        
        wchar_t line[256];
        StringCchPrintfW(line, 256, L"%-22s  %-8s  %8lld  %7zu",
            table.Name.c_str(),
            L"Table",
            _Database->GetRecordCount(table.Name),
            table.Columns.size());
        
        TextOutW(hdc, rc.left, y, line, static_cast<int>(wcslen(line)));
        y += lineHeight;
    }
    
    // Views
    for (const auto& view : views) {
        if (y > rc.bottom) break;
        
        wchar_t line[256];
        StringCchPrintfW(line, 256, L"%-22s  %-8s  %8lld  %7zu",
            view.Name.c_str(),
            L"View",
            _Database->GetRecordCount(view.Name),
            view.Columns.size());
        
        TextOutW(hdc, rc.left, y, line, static_cast<int>(wcslen(line)));
        y += lineHeight;
    }
}

LRESULT CALLBACK PreviewHandler::PreviewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PreviewHandler* pThis = nullptr;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* pcs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = static_cast<PreviewHandler*>(pcs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<PreviewHandler*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    
    switch (msg) {
        case WM_PAINT: {
            if (pThis) {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                
                RECT rc;
                GetClientRect(hwnd, &rc);
                
                // Fill background
                HBRUSH hBrush = CreateSolidBrush(pThis->_BackColor);
                FillRect(hdc, &rc, hBrush);
                DeleteObject(hBrush);
                
                pThis->DrawDatabaseSummary(hdc, rc);
                
                EndPaint(hwnd, &ps);
            }
            return 0;
        }
        
        case WM_ERASEBKGND:
            return 1;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace SQLiteView
