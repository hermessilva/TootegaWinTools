/*
** SQLiteView - Windows Explorer Shell Extension
** Icon Handler Implementation
*/

#include "IconHandler.h"

namespace SQLiteView {

// ============================================================================
// IconHandler Implementation
// ============================================================================

IconHandler::IconHandler() : refCount_(1), iconType_(IconType::Database) {
}

IconHandler::~IconHandler() {
}

STDMETHODIMP IconHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown)) {
        *ppv = static_cast<IExtractIconW*>(this);
    }
    else if (IsEqualIID(riid, IID_IExtractIconW)) {
        *ppv = static_cast<IExtractIconW*>(this);
    }
    else if (IsEqualIID(riid, IID_IExtractIconA)) {
        *ppv = static_cast<IExtractIconA*>(this);
    }
    else if (IsEqualIID(riid, IID_IPersist) || IsEqualIID(riid, IID_IPersistFile)) {
        *ppv = static_cast<IPersistFile*>(this);
    }
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) IconHandler::AddRef() {
    return InterlockedIncrement(&refCount_);
}

STDMETHODIMP_(ULONG) IconHandler::Release() {
    LONG count = InterlockedDecrement(&refCount_);
    if (count == 0) {
        delete this;
    }
    return count;
}

// IExtractIconW
STDMETHODIMP IconHandler::GetIconLocation(UINT uFlags, LPWSTR pszIconFile, UINT cchMax,
    int* piIndex, UINT* pwFlags) {
    
    if (!piIndex || !pwFlags) return E_POINTER;
    
    // Usar ícones embutidos na DLL
    GetModuleFileNameW(GetModuleHandle(L"SQLiteView.dll"), pszIconFile, cchMax);
    
    switch (iconType_) {
        case IconType::Database:
            *piIndex = 0;
            break;
        case IconType::Table:
            *piIndex = 1;
            break;
        case IconType::View:
            *piIndex = 2;
            break;
        case IconType::Index:
            *piIndex = 3;
            break;
        case IconType::SystemTable:
            *piIndex = 4;
            break;
        default:
            *piIndex = 0;
            break;
    }
    
    *pwFlags = GIL_PERINSTANCE;
    
    return S_OK;
}

STDMETHODIMP IconHandler::Extract(LPCWSTR pszFile, UINT nIconIndex,
    HICON* phiconLarge, HICON* phiconSmall, UINT nIconSize) {
    
    int largeSize = LOWORD(nIconSize);
    int smallSize = HIWORD(nIconSize);
    
    if (phiconLarge) {
        *phiconLarge = CreateTableIcon(largeSize > 0 ? largeSize : 32);
    }
    
    if (phiconSmall) {
        *phiconSmall = CreateTableIcon(smallSize > 0 ? smallSize : 16);
    }
    
    return S_OK;
}

// IExtractIconA
STDMETHODIMP IconHandler::GetIconLocation(UINT uFlags, LPSTR pszIconFile, UINT cchMax,
    int* piIndex, UINT* pwFlags) {
    
    wchar_t wideFile[MAX_PATH];
    HRESULT hr = GetIconLocation(uFlags, wideFile, MAX_PATH, piIndex, pwFlags);
    
    if (SUCCEEDED(hr)) {
        WideCharToMultiByte(CP_ACP, 0, wideFile, -1, pszIconFile, cchMax, nullptr, nullptr);
    }
    
    return hr;
}

STDMETHODIMP IconHandler::Extract(LPCSTR pszFile, UINT nIconIndex,
    HICON* phiconLarge, HICON* phiconSmall, UINT nIconSize) {
    
    wchar_t wideFile[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, pszFile, -1, wideFile, MAX_PATH);
    
    return Extract(wideFile, nIconIndex, phiconLarge, phiconSmall, nIconSize);
}

// IPersist
STDMETHODIMP IconHandler::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    *pClassID = CLSID_SQLiteViewFolder;
    return S_OK;
}

// IPersistFile
STDMETHODIMP IconHandler::IsDirty() {
    return S_FALSE;
}

STDMETHODIMP IconHandler::Load(LPCOLESTR pszFileName, DWORD dwMode) {
    if (!pszFileName) return E_POINTER;
    filePath_ = pszFileName;
    return S_OK;
}

STDMETHODIMP IconHandler::Save(LPCOLESTR pszFileName, BOOL fRemember) {
    return E_NOTIMPL;
}

STDMETHODIMP IconHandler::SaveCompleted(LPCOLESTR pszFileName) {
    return E_NOTIMPL;
}

STDMETHODIMP IconHandler::GetCurFile(LPOLESTR* ppszFileName) {
    if (!ppszFileName) return E_POINTER;
    return SHStrDupW(filePath_.c_str(), ppszFileName);
}

HICON IconHandler::CreateDatabaseIcon(int size) {
    return CreateTableIcon(size); // Usar mesmo ícone por enquanto
}

HICON IconHandler::CreateTableIcon(int size) {
    // Criar bitmap para o ícone
    HDC hdc = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(hdc);
    
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);
    
    // Limpar com transparência
    memset(bits, 0, size * size * 4);
    
    bool isDark = IconDraw::IsDarkTheme();
    DrawTableIcon(memDC, size, isDark);
    
    SelectObject(memDC, oldBitmap);
    
    // Criar máscara
    HBITMAP hMask = CreateBitmap(size, size, 1, 1, nullptr);
    
    ICONINFO iconInfo = {};
    iconInfo.fIcon = TRUE;
    iconInfo.hbmMask = hMask;
    iconInfo.hbmColor = hBitmap;
    
    HICON hIcon = CreateIconIndirect(&iconInfo);
    
    DeleteObject(hMask);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, hdc);
    
    return hIcon;
}

HICON IconHandler::CreateViewIcon(int size) {
    return CreateTableIcon(size); // TODO: Ícone diferente para views
}

HICON IconHandler::CreateIndexIcon(int size) {
    return CreateTableIcon(size); // TODO: Ícone diferente para índices
}

void IconHandler::DrawDatabaseIcon(HDC hdc, int size, bool isDark) {
    DrawTableIcon(hdc, size, isDark);
}

void IconHandler::DrawTableIcon(HDC hdc, int size, bool isDark) {
    // Cores
    COLORREF bgColor = isDark ? RGB(45, 45, 48) : RGB(240, 240, 240);
    COLORREF fgColor = isDark ? RGB(220, 220, 220) : RGB(60, 60, 60);
    COLORREF accentColor = RGB(0, 120, 215);
    
    int margin = size / 8;
    int headerHeight = size / 4;
    
    RECT rc = { margin, margin, size - margin, size - margin };
    
    // Fundo da tabela
    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    HBRUSH accentBrush = CreateSolidBrush(accentColor);
    
    // Cabeçalho (azul)
    RECT headerRect = { rc.left, rc.top, rc.right, rc.top + headerHeight };
    IconDraw::FillRoundRect(hdc, headerRect, size / 16, accentColor);
    
    // Corpo
    RECT bodyRect = { rc.left, rc.top + headerHeight, rc.right, rc.bottom };
    IconDraw::FillRoundRect(hdc, bodyRect, size / 16, bgColor);
    
    // Linhas horizontais
    HPEN linePen = CreatePen(PS_SOLID, 1, isDark ? RGB(80, 80, 80) : RGB(200, 200, 200));
    HPEN oldPen = (HPEN)SelectObject(hdc, linePen);
    
    int rowHeight = (bodyRect.bottom - bodyRect.top) / 3;
    for (int i = 1; i < 3; i++) {
        int y = bodyRect.top + i * rowHeight;
        MoveToEx(hdc, bodyRect.left + 2, y, nullptr);
        LineTo(hdc, bodyRect.right - 2, y);
    }
    
    // Linha vertical
    int colX = rc.left + (rc.right - rc.left) / 3;
    MoveToEx(hdc, colX, bodyRect.top + 2, nullptr);
    LineTo(hdc, colX, bodyRect.bottom - 2);
    
    SelectObject(hdc, oldPen);
    DeleteObject(linePen);
    
    DeleteObject(bgBrush);
    DeleteObject(accentBrush);
}

void IconHandler::DrawViewIcon(HDC hdc, int size, bool isDark) {
    // Ícone similar à tabela mas com indicador de "view"
    DrawTableIcon(hdc, size, isDark);
    
    // Adicionar símbolo de "olho" ou similar
    COLORREF color = RGB(0, 150, 0);
    int eyeSize = size / 4;
    int x = size - eyeSize - size / 8;
    int y = size - eyeSize - size / 8;
    
    Ellipse(hdc, x, y, x + eyeSize, y + eyeSize);
}

void IconHandler::DrawIndexIcon(HDC hdc, int size, bool isDark) {
    // Ícone de índice (tipo lista ordenada)
    COLORREF fgColor = isDark ? RGB(220, 220, 220) : RGB(60, 60, 60);
    COLORREF accentColor = RGB(200, 150, 0);
    
    int margin = size / 8;
    int lineHeight = (size - margin * 2) / 4;
    
    HPEN linePen = CreatePen(PS_SOLID, size / 16 + 1, fgColor);
    HPEN accentPen = CreatePen(PS_SOLID, size / 16 + 1, accentColor);
    HPEN oldPen = (HPEN)SelectObject(hdc, linePen);
    
    for (int i = 0; i < 3; i++) {
        int y = margin + i * lineHeight + lineHeight / 2;
        
        // Número/marcador
        SelectObject(hdc, accentPen);
        MoveToEx(hdc, margin, y, nullptr);
        LineTo(hdc, margin + size / 5, y);
        
        // Linha
        SelectObject(hdc, linePen);
        MoveToEx(hdc, margin + size / 4, y, nullptr);
        LineTo(hdc, size - margin, y);
    }
    
    SelectObject(hdc, oldPen);
    DeleteObject(linePen);
    DeleteObject(accentPen);
}

// ============================================================================
// ThumbnailProvider Implementation
// ============================================================================

ThumbnailProvider::ThumbnailProvider() : refCount_(1) {
}

ThumbnailProvider::~ThumbnailProvider() {
}

STDMETHODIMP ThumbnailProvider::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown)) {
        *ppv = static_cast<IThumbnailProvider*>(this);
    }
    else if (IsEqualIID(riid, IID_IThumbnailProvider)) {
        *ppv = static_cast<IThumbnailProvider*>(this);
    }
    else if (IsEqualIID(riid, IID_IInitializeWithFile)) {
        *ppv = static_cast<IInitializeWithFile*>(this);
    }
    else if (IsEqualIID(riid, IID_IInitializeWithStream)) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    }
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) ThumbnailProvider::AddRef() {
    return InterlockedIncrement(&refCount_);
}

STDMETHODIMP_(ULONG) ThumbnailProvider::Release() {
    LONG count = InterlockedDecrement(&refCount_);
    if (count == 0) {
        delete this;
    }
    return count;
}

STDMETHODIMP ThumbnailProvider::Initialize(LPCWSTR pszFilePath, DWORD grfMode) {
    if (!pszFilePath) return E_POINTER;
    filePath_ = pszFilePath;
    return S_OK;
}

STDMETHODIMP ThumbnailProvider::Initialize(IStream* pstream, DWORD grfMode) {
    return E_NOTIMPL;
}

STDMETHODIMP ThumbnailProvider::GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) {
    if (!phbmp) return E_POINTER;
    
    *phbmp = CreateThumbnail(cx);
    if (pdwAlpha) *pdwAlpha = WTSAT_ARGB;
    
    return *phbmp ? S_OK : E_FAIL;
}

HBITMAP ThumbnailProvider::CreateThumbnail(UINT size) {
    // Obter informações do banco
    auto db = DatabasePool::Instance().GetDatabase(filePath_);
    DatabaseInfo info;
    if (db && db->IsOpen()) {
        info = db->GetDatabaseInfo();
    }
    
    // Criar bitmap
    HDC hdc = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(hdc);
    
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -(int)size;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);
    
    DrawThumbnail(memDC, size, info);
    
    SelectObject(memDC, oldBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, hdc);
    
    return hBitmap;
}

void ThumbnailProvider::DrawThumbnail(HDC hdc, UINT size, const DatabaseInfo& info) {
    bool isDark = IconDraw::IsDarkTheme();
    
    COLORREF bgColor = isDark ? RGB(30, 30, 30) : RGB(255, 255, 255);
    COLORREF fgColor = isDark ? RGB(230, 230, 230) : RGB(30, 30, 30);
    COLORREF accentColor = RGB(0, 120, 215);
    
    // Fundo
    RECT rc = { 0, 0, (int)size, (int)size };
    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
    
    // Ícone de banco de dados
    int iconSize = size / 3;
    int iconX = (size - iconSize) / 2;
    int iconY = size / 8;
    
    // Desenhar cilindro estilizado (banco de dados)
    HBRUSH accentBrush = CreateSolidBrush(accentColor);
    HBRUSH darkBrush = CreateSolidBrush(RGB(0, 80, 150));
    
    // Parte inferior do cilindro
    RECT baseRect = { iconX, iconY + iconSize / 3, iconX + iconSize, iconY + iconSize };
    IconDraw::FillRoundRect(hdc, baseRect, iconSize / 8, RGB(0, 80, 150));
    
    // Topo do cilindro
    Ellipse(hdc, iconX, iconY, iconX + iconSize, iconY + iconSize / 2);
    
    DeleteObject(accentBrush);
    DeleteObject(darkBrush);
    
    // Texto informativo
    if (!info.fileName.empty()) {
        int fontSize = size / 12;
        HFONT hFont = CreateFontW(-fontSize, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, fgColor);
        
        int textY = iconY + iconSize + size / 16;
        
        // Nome do arquivo
        RECT nameRect = { 4, textY, (int)size - 4, textY + fontSize * 2 };
        DrawTextW(hdc, info.fileName.c_str(), -1, &nameRect, 
            DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        
        // Resumo
        std::wstring summary = StringUtil::Format(L"%d tables", (int)info.tables.size());
        textY += fontSize * 2;
        RECT summaryRect = { 4, textY, (int)size - 4, textY + fontSize * 2 };
        SetTextColor(hdc, isDark ? RGB(150, 150, 150) : RGB(100, 100, 100));
        DrawTextW(hdc, summary.c_str(), -1, &summaryRect, 
            DT_CENTER | DT_SINGLELINE);
        
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
    }
}

// ============================================================================
// IconDraw Utilities
// ============================================================================

namespace IconDraw {

void FillRoundRect(HDC hdc, const RECT& rc, int radius, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawRoundRect(HDC hdc, const RECT& rc, int radius, int width, COLORREF color) {
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void DrawText(HDC hdc, const RECT& rc, const wchar_t* text, COLORREF color,
    int fontSize, const wchar_t* fontName, UINT format) {
    
    HFONT hFont = CreateFontW(-fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, fontName);
    
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    
    DrawTextW(hdc, text, -1, (LPRECT)&rc, format);
    
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
}

HBITMAP CreateAlphaBitmap(int width, int height) {
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* bits = nullptr;
    return CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
}

bool IsDarkTheme() {
    DWORD value = 1;
    DWORD size = sizeof(value);
    
    if (RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_DWORD,
        nullptr,
        &value,
        &size) == ERROR_SUCCESS) {
        return value == 0;
    }
    
    return false;
}

} // namespace IconDraw

} // namespace SQLiteView
