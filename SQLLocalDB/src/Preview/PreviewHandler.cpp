/*
** SQLLocalDBView - Windows Explorer Shell Extension
** Preview Handler Implementation
*/

// Define NOMINMAX before including Windows headers to prevent min/max macro conflicts
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "PreviewHandler.h"
#include <dwmapi.h>
#include <vssym32.h>
#include <algorithm>

using std::min;
using std::max;

namespace SQLLocalDBView {

const wchar_t* PreviewHandler::WINDOW_CLASS = L"SQLLocalDBViewPreview";
bool PreviewHandler::classRegistered_ = false;

PreviewHandler::PreviewHandler()
    : refCount_(1)
    , hwndParent_(nullptr)
    , hwndPreview_(nullptr)
    , hFont_(nullptr)
    , hFontHeader_(nullptr)
    , hFontMono_(nullptr) {
    
    ZeroMemory(&previewRect_, sizeof(previewRect_));
    InitializeTheme();
}

PreviewHandler::~PreviewHandler() {
    DestroyPreviewWindow();
    
    if (hFont_) DeleteObject(hFont_);
    if (hFontHeader_) DeleteObject(hFontHeader_);
    if (hFontMono_) DeleteObject(hFontMono_);
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
    else if (IsEqualIID(riid, IID_IInitializeWithStream)) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    }
    else if (IsEqualIID(riid, IID_IInitializeWithItem)) {
        *ppv = static_cast<IInitializeWithItem*>(this);
    }
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) PreviewHandler::AddRef() {
    return InterlockedIncrement(&refCount_);
}

STDMETHODIMP_(ULONG) PreviewHandler::Release() {
    LONG count = InterlockedDecrement(&refCount_);
    if (count == 0) {
        delete this;
    }
    return count;
}

// IInitializeWithFile
STDMETHODIMP PreviewHandler::Initialize(LPCWSTR pszFilePath, DWORD grfMode) {
    if (!pszFilePath) return E_POINTER;
    
    filePath_ = pszFilePath;
    
    // Verificar se é um arquivo .db direto ou uma referência a tabela
    // Formato: "path\to\file.db::tablename"
    size_t pos = filePath_.find(L"::");
    if (pos != std::wstring::npos) {
        tableName_ = filePath_.substr(pos + 2);
        filePath_ = filePath_.substr(0, pos);
    }
    
    SQLLOCALDB_LOG(L"PreviewHandler::Initialize file=%s table=%s", 
        filePath_.c_str(), tableName_.c_str());
    
    return S_OK;
}

// IInitializeWithStream
STDMETHODIMP PreviewHandler::Initialize(IStream* pstream, DWORD grfMode) {
    // SQL LocalDB não suporta streams diretamente, precisamos de arquivo físico
    return E_NOTIMPL;
}

// IInitializeWithItem
STDMETHODIMP PreviewHandler::Initialize(IShellItem* psi, DWORD grfMode) {
    if (!psi) return E_POINTER;

    PWSTR path = nullptr;
    HRESULT hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &path);
    if (SUCCEEDED(hr) && path) {
        filePath_ = path;
        CoTaskMemFree(path);
    }

    return hr;
}

// IPreviewHandler
STDMETHODIMP PreviewHandler::SetWindow(HWND hwnd, const RECT* prc) {
    hwndParent_ = hwnd;
    if (prc) {
        previewRect_ = *prc;
    }
    
    if (hwndPreview_) {
        SetParent(hwndPreview_, hwndParent_);
        UpdateLayout();
    }
    
    return S_OK;
}

STDMETHODIMP PreviewHandler::SetRect(const RECT* prc) {
    if (prc) {
        previewRect_ = *prc;
        UpdateLayout();
    }
    return S_OK;
}

STDMETHODIMP PreviewHandler::DoPreview() {
    if (!hwndParent_) return E_FAIL;
    
    if (!CreatePreviewWindow()) {
        return E_FAIL;
    }
    
    if (!LoadData()) {
        // Mostrar erro
    }
    
    InvalidateRect(hwndPreview_, nullptr, TRUE);
    return S_OK;
}

STDMETHODIMP PreviewHandler::Unload() {
    DestroyPreviewWindow();
    database_.reset();
    queryResult_ = QueryResult();
    filePath_.clear();
    tableName_.clear();
    return S_OK;
}

STDMETHODIMP PreviewHandler::SetFocus() {
    if (hwndPreview_) {
        ::SetFocus(hwndPreview_);
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
    colors_.background = color;
    if (hwndPreview_) {
        InvalidateRect(hwndPreview_, nullptr, TRUE);
    }
    return S_OK;
}

STDMETHODIMP PreviewHandler::SetFont(const LOGFONTW* plf) {
    if (plf) {
        if (hFont_) DeleteObject(hFont_);
        hFont_ = CreateFontIndirectW(plf);
        
        if (hwndPreview_) {
            InvalidateRect(hwndPreview_, nullptr, TRUE);
        }
    }
    return S_OK;
}

STDMETHODIMP PreviewHandler::SetTextColor(COLORREF color) {
    colors_.foreground = color;
    if (hwndPreview_) {
        InvalidateRect(hwndPreview_, nullptr, TRUE);
    }
    return S_OK;
}

// IOleWindow
STDMETHODIMP PreviewHandler::GetWindow(HWND* phwnd) {
    if (!phwnd) return E_POINTER;
    *phwnd = hwndPreview_;
    return hwndPreview_ ? S_OK : E_FAIL;
}

STDMETHODIMP PreviewHandler::ContextSensitiveHelp(BOOL fEnterMode) {
    return E_NOTIMPL;
}

// IObjectWithSite
STDMETHODIMP PreviewHandler::SetSite(IUnknown* pUnkSite) {
    site_ = pUnkSite;
    return S_OK;
}

STDMETHODIMP PreviewHandler::GetSite(REFIID riid, void** ppvSite) {
    if (!ppvSite) return E_POINTER;
    
    if (site_) {
        return site_->QueryInterface(riid, ppvSite);
    }
    
    *ppvSite = nullptr;
    return E_FAIL;
}

// Private methods
bool PreviewHandler::CreatePreviewWindow() {
    if (hwndPreview_) return true;
    
    // Registrar classe se necessário
    if (!classRegistered_) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = WINDOW_CLASS;
        
        if (RegisterClassExW(&wc)) {
            classRegistered_ = true;
        }
    }
    
    // Criar janela
    hwndPreview_ = CreateWindowExW(
        0,
        WINDOW_CLASS,
        L"SQL LocalDB Preview",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL,
        previewRect_.left, previewRect_.top,
        previewRect_.right - previewRect_.left,
        previewRect_.bottom - previewRect_.top,
        hwndParent_,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );
    
    if (!hwndPreview_) {
        SQLLOCALDB_LOG(L"Failed to create preview window: %d", GetLastError());
        return false;
    }
    
    // Criar fontes
    if (!hFont_) {
        hFont_ = CreateFontW(
            -config_.fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
            config_.fontName.c_str()
        );
    }
    
    if (!hFontHeader_) {
        hFontHeader_ = CreateFontW(
            -config_.headerFontSize, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
            config_.fontName.c_str()
        );
    }
    
    if (!hFontMono_) {
        hFontMono_ = CreateFontW(
            -config_.fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
            config_.monoFontName.c_str()
        );
    }
    
    UpdateTheme();
    
    return true;
}

void PreviewHandler::DestroyPreviewWindow() {
    if (hwndPreview_) {
        DestroyWindow(hwndPreview_);
        hwndPreview_ = nullptr;
    }
}

void PreviewHandler::UpdateLayout() {
    if (hwndPreview_) {
        MoveWindow(
            hwndPreview_,
            previewRect_.left, previewRect_.top,
            previewRect_.right - previewRect_.left,
            previewRect_.bottom - previewRect_.top,
            TRUE
        );
        UpdateScrollBars();
    }
}

void PreviewHandler::InitializeTheme() {
    // Valores padrão (tema claro)
    colors_.background = RGB(255, 255, 255);
    colors_.foreground = RGB(30, 30, 30);
    colors_.headerBackground = RGB(240, 240, 240);
    colors_.headerForeground = RGB(50, 50, 50);
    colors_.gridLines = RGB(230, 230, 230);
    colors_.alternateRow = RGB(250, 250, 252);
    colors_.selection = RGB(0, 120, 215);
    colors_.selectionText = RGB(255, 255, 255);
    colors_.nullValue = RGB(180, 180, 180);
    colors_.blobValue = RGB(128, 0, 128);
    colors_.numberValue = RGB(0, 100, 0);
    colors_.textValue = RGB(0, 0, 0);
    colors_.border = RGB(200, 200, 200);
    colors_.scrollbar = RGB(240, 240, 240);
    colors_.scrollbarThumb = RGB(180, 180, 180);
}

void PreviewHandler::UpdateTheme() {
    if (IsDarkMode()) {
        colors_.background = RGB(30, 30, 30);
        colors_.foreground = RGB(230, 230, 230);
        colors_.headerBackground = RGB(45, 45, 45);
        colors_.headerForeground = RGB(220, 220, 220);
        colors_.gridLines = RGB(60, 60, 60);
        colors_.alternateRow = RGB(35, 35, 38);
        colors_.selection = RGB(0, 90, 158);
        colors_.selectionText = RGB(255, 255, 255);
        colors_.nullValue = RGB(128, 128, 128);
        colors_.blobValue = RGB(200, 150, 200);
        colors_.numberValue = RGB(150, 220, 150);
        colors_.textValue = RGB(230, 230, 230);
        colors_.border = RGB(70, 70, 70);
        colors_.scrollbar = RGB(50, 50, 50);
        colors_.scrollbarThumb = RGB(100, 100, 100);
    } else {
        InitializeTheme();
    }
}

bool PreviewHandler::IsDarkMode() {
    // Verificar configuração do sistema
    DWORD value = 0;
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

bool PreviewHandler::LoadData() {
    if (filePath_.empty()) return false;
    
    database_ = DatabasePool::Instance().GetDatabase(filePath_);
    if (!database_ || !database_->IsOpen()) {
        queryResult_.error = L"Failed to open database";
        return false;
    }
    
    // Se não temos nome de tabela, mostrar lista de tabelas do banco
    if (tableName_.empty()) {
        return LoadTablesList();
    }
    
    // Temos uma tabela específica - mostrar seus dados
    showingTablesList_ = false;
    
    // Carregar dados da tabela
    queryResult_ = database_->GetTablePreview(tableName_, config_.maxRows);
    
    if (!queryResult_.error.empty()) {
        return false;
    }
    
    // Preparar colunas para renderização
    columns_.clear();
    auto tableInfo = database_->GetTableInfo(tableName_);
    
    for (size_t i = 0; i < queryResult_.columnNames.size(); i++) {
        RenderColumn col;
        col.name = queryResult_.columnNames[i];
        col.type = i < queryResult_.columnTypes.size() ? queryResult_.columnTypes[i] : L"";
        col.width = config_.minColumnWidth;
        
        // Verificar se é primary key
        for (const auto& colInfo : tableInfo.columns) {
            if (colInfo.name == col.name) {
                col.isPrimaryKey = colInfo.primaryKey;
                col.isNullable = !colInfo.notNull;
                break;
            }
        }
        
        columns_.push_back(col);
    }
    
    // Calcular largura das colunas
    if (hwndPreview_) {
        HDC hdc = GetDC(hwndPreview_);
        CalculateColumnWidths(hdc);
        ReleaseDC(hwndPreview_, hdc);
    }
    
    return true;
}

// Carrega lista de tabelas para mostrar visão geral do banco
bool PreviewHandler::LoadTablesList() {
    showingTablesList_ = true;
    tablesList_.clear();
    
    // Obter todas as tabelas
    tablesList_ = database_->GetTables();
    
    // Adicionar views também
    auto views = database_->GetViews();
    for (auto& view : views) {
        view.type = DatabaseObjectType::View;
        tablesList_.push_back(view);
    }
    
    if (tablesList_.empty()) {
        queryResult_.error = L"No tables found in database";
        return false;
    }
    
    SQLLOCALDB_LOG(L"LoadTablesList: Found %d tables/views", (int)tablesList_.size());
    
    // Preparar colunas para a lista de tabelas
    columns_.clear();
    
    RenderColumn colName;
    colName.name = L"Table Name";
    colName.width = 200;
    columns_.push_back(colName);
    
    RenderColumn colType;
    colType.name = L"Type";
    colType.width = 80;
    columns_.push_back(colType);
    
    RenderColumn colRows;
    colRows.name = L"Rows";
    colRows.width = 100;
    columns_.push_back(colRows);
    
    RenderColumn colCols;
    colCols.name = L"Columns";
    colCols.width = 80;
    columns_.push_back(colCols);
    
    // Calcular tamanho do conteúdo
    contentWidth_ = 0;
    for (const auto& col : columns_) {
        contentWidth_ += col.width;
    }
    if (config_.showRowNumbers) {
        contentWidth_ += 50;
    }
    
    contentHeight_ = config_.headerHeight + (int)tablesList_.size() * config_.rowHeight;
    
    return true;
}

void PreviewHandler::CalculateColumnWidths(HDC hdc) {
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont_);
    
    // Calcular largura baseada no conteúdo
    for (size_t col = 0; col < columns_.size(); col++) {
        int maxWidth = 0;
        
        // Largura do cabeçalho
        SIZE sz;
        GetTextExtentPoint32W(hdc, columns_[col].name.c_str(), 
            (int)columns_[col].name.length(), &sz);
        maxWidth = sz.cx + config_.padding * 2;
        
        // Largura dos dados
        for (const auto& row : queryResult_.rows) {
            if (col < row.size()) {
                std::wstring text = row[col].ToString();
                if (text.length() > 100) text = text.substr(0, 100) + L"...";
                
                GetTextExtentPoint32W(hdc, text.c_str(), (int)text.length(), &sz);
                int width = sz.cx + config_.padding * 2;
                if (width > maxWidth) maxWidth = width;
            }
        }
        
        // Limitar
        columns_[col].width = min(max(maxWidth, config_.minColumnWidth), config_.maxColumnWidth);
    }
    
    SelectObject(hdc, oldFont);
    
    // Calcular tamanho total do conteúdo
    contentWidth_ = 0;
    for (const auto& col : columns_) {
        contentWidth_ += col.width;
    }
    if (config_.showRowNumbers) {
        contentWidth_ += 50; // Largura da coluna de números
    }
    
    contentHeight_ = config_.headerHeight + (int)queryResult_.rows.size() * config_.rowHeight;
}

void PreviewHandler::UpdateScrollBars() {
    if (!hwndPreview_) return;
    
    RECT rc;
    GetClientRect(hwndPreview_, &rc);
    
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    
    // Scroll horizontal
    si.nMin = 0;
    si.nMax = contentWidth_;
    si.nPage = rc.right - rc.left;
    si.nPos = scrollX_;
    SetScrollInfo(hwndPreview_, SB_HORZ, &si, TRUE);
    
    // Scroll vertical
    si.nMax = contentHeight_;
    si.nPage = rc.bottom - rc.top;
    si.nPos = scrollY_;
    SetScrollInfo(hwndPreview_, SB_VERT, &si, TRUE);
}

void PreviewHandler::HandleScroll(int scrollType, int nPos, int nBar) {
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwndPreview_, nBar, &si);
    
    int& scroll = (nBar == SB_HORZ) ? scrollX_ : scrollY_;
    int oldPos = scroll;
    
    switch (scrollType) {
        case SB_LINEUP:
            scroll -= config_.rowHeight;
            break;
        case SB_LINEDOWN:
            scroll += config_.rowHeight;
            break;
        case SB_PAGEUP:
            scroll -= si.nPage;
            break;
        case SB_PAGEDOWN:
            scroll += si.nPage;
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            scroll = si.nTrackPos;
            break;
        case SB_TOP:
            scroll = si.nMin;
            break;
        case SB_BOTTOM:
            scroll = si.nMax;
            break;
    }
    
    // Limitar
    int maxScroll = (nBar == SB_HORZ) ? 
        (contentWidth_ - (int)si.nPage) : 
        (contentHeight_ - (int)si.nPage);
    
    scroll = max(0, min(scroll, maxScroll));
    
    if (scroll != oldPos) {
        si.fMask = SIF_POS;
        si.nPos = scroll;
        SetScrollInfo(hwndPreview_, nBar, &si, TRUE);
        InvalidateRect(hwndPreview_, nullptr, TRUE);
    }
}

void PreviewHandler::HandleMouseWheel(int delta) {
    int lines = delta / WHEEL_DELTA * 3;
    scrollY_ -= lines * config_.rowHeight;
    
    RECT rc;
    GetClientRect(hwndPreview_, &rc);
    int maxScroll = contentHeight_ - (rc.bottom - rc.top);
    scrollY_ = max(0, min(scrollY_, maxScroll));
    
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_POS;
    si.nPos = scrollY_;
    SetScrollInfo(hwndPreview_, SB_VERT, &si, TRUE);
    
    InvalidateRect(hwndPreview_, nullptr, TRUE);
}

void PreviewHandler::Render(HDC hdc, const RECT& rc) {
    // Fundo
    HBRUSH bgBrush = CreateSolidBrush(colors_.background);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
    
    if (!queryResult_.error.empty()) {
        RenderError(hdc, rc, queryResult_.error);
        return;
    }
    
    // Se estamos mostrando lista de tabelas
    if (showingTablesList_) {
        RenderTablesList(hdc, rc);
        return;
    }
    
    if (queryResult_.rows.empty() && queryResult_.columnNames.empty()) {
        RenderEmpty(hdc, rc);
        return;
    }
    
    // Renderizar cabeçalho
    RECT headerRect = rc;
    headerRect.bottom = headerRect.top + config_.headerHeight;
    RenderHeader(hdc, headerRect);
    
    // Renderizar grade de dados
    RECT gridRect = rc;
    gridRect.top += config_.headerHeight;
    RenderGrid(hdc, gridRect);
    
    // Borda
    HPEN borderPen = CreatePen(PS_SOLID, 1, colors_.border);
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    MoveToEx(hdc, rc.left, rc.top + config_.headerHeight - 1, nullptr);
    LineTo(hdc, rc.right, rc.top + config_.headerHeight - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
}

// Renderiza lista de tabelas do banco (visão geral)
void PreviewHandler::RenderTablesList(HDC hdc, const RECT& rc) {
    // Título do banco
    HFONT oldFont = (HFONT)SelectObject(hdc, hFontHeader_);
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, colors_.headerForeground);
    
    // Extrair nome do arquivo do caminho
    std::wstring dbName = filePath_;
    size_t pos = dbName.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        dbName = dbName.substr(pos + 1);
    }
    
    // Cabeçalho com nome do banco
    RECT titleRect = rc;
    titleRect.bottom = titleRect.top + config_.headerHeight;
    
    HBRUSH headerBrush = CreateSolidBrush(colors_.headerBackground);
    FillRect(hdc, &titleRect, headerBrush);
    DeleteObject(headerBrush);
    
    std::wstring title = L"SQL LocalDB Database: " + dbName + L" (" + 
        std::to_wstring(tablesList_.size()) + L" tables)";
    
    RECT textRect = titleRect;
    textRect.left += config_.padding;
    DrawTextW(hdc, title.c_str(), -1, &textRect, 
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    
    // Linha separadora
    HPEN borderPen = CreatePen(PS_SOLID, 1, colors_.border);
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    MoveToEx(hdc, rc.left, titleRect.bottom - 1, nullptr);
    LineTo(hdc, rc.right, titleRect.bottom - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
    
    // Cabeçalho das colunas
    RECT colHeaderRect = rc;
    colHeaderRect.top = titleRect.bottom;
    colHeaderRect.bottom = colHeaderRect.top + config_.rowHeight;
    
    HBRUSH colHeaderBrush = CreateSolidBrush(colors_.alternateRow);
    FillRect(hdc, &colHeaderRect, colHeaderBrush);
    DeleteObject(colHeaderBrush);
    
    SelectObject(hdc, hFontHeader_);
    int x = config_.padding - scrollX_;
    
    for (const auto& col : columns_) {
        RECT cellRect = colHeaderRect;
        cellRect.left = x;
        cellRect.right = x + col.width;
        cellRect.left += config_.padding;
        
        DrawTextW(hdc, col.name.c_str(), -1, &cellRect, 
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        
        x += col.width;
    }
    
    // Renderizar lista de tabelas
    SelectObject(hdc, hFont_);
    ::SetTextColor(hdc, colors_.foreground);
    
    int y = colHeaderRect.bottom - scrollY_;
    int rowNum = 0;
    
    for (const auto& table : tablesList_) {
        if (y > rc.bottom) break;
        if (y + config_.rowHeight > colHeaderRect.bottom) {
            // Fundo alternado
            RECT rowRect = { rc.left, y, rc.right, y + config_.rowHeight };
            if (rowNum % 2 == 1) {
                HBRUSH altBrush = CreateSolidBrush(colors_.alternateRow);
                FillRect(hdc, &rowRect, altBrush);
                DeleteObject(altBrush);
            }
            
            x = config_.padding - scrollX_;
            
            // Nome da tabela
            RECT nameRect = { x, y, x + columns_[0].width, y + config_.rowHeight };
            nameRect.left += config_.padding;
            
            // Ícone baseado no tipo
            std::wstring icon = (table.type == DatabaseObjectType::View) ? L"📊 " : L"📋 ";
            std::wstring displayName = icon + table.name;
            
            DrawTextW(hdc, displayName.c_str(), -1, &nameRect, 
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            x += columns_[0].width;
            
            // Tipo
            RECT typeRect = { x, y, x + columns_[1].width, y + config_.rowHeight };
            typeRect.left += config_.padding;
            std::wstring typeStr = (table.type == DatabaseObjectType::View) ? L"View" : L"Table";
            DrawTextW(hdc, typeStr.c_str(), -1, &typeRect, 
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            x += columns_[1].width;
            
            // Linhas
            RECT rowsRect = { x, y, x + columns_[2].width, y + config_.rowHeight };
            rowsRect.left += config_.padding;
            std::wstring rowsStr = (table.rowCount >= 0) ? 
                std::to_wstring(table.rowCount) : L"-";
            DrawTextW(hdc, rowsStr.c_str(), -1, &rowsRect, 
                DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            x += columns_[2].width;
            
            // Colunas
            RECT colsRect = { x, y, x + columns_[3].width, y + config_.rowHeight };
            colsRect.left += config_.padding;
            std::wstring colsStr = std::to_wstring(table.columnCount);
            DrawTextW(hdc, colsStr.c_str(), -1, &colsRect, 
                DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        
        y += config_.rowHeight;
        rowNum++;
    }
    
    SelectObject(hdc, oldFont);
}

void PreviewHandler::RenderHeader(HDC hdc, const RECT& rc) {
    // Fundo do cabeçalho
    HBRUSH headerBrush = CreateSolidBrush(colors_.headerBackground);
    FillRect(hdc, &rc, headerBrush);
    DeleteObject(headerBrush);
    
    HFONT oldFont = (HFONT)SelectObject(hdc, hFontHeader_);
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, colors_.headerForeground);
    
    int x = -scrollX_;
    
    // Coluna de números de linha
    if (config_.showRowNumbers) {
        RECT cellRect = rc;
        cellRect.left = x;
        cellRect.right = x + 50;
        
        DrawTextW(hdc, L"#", -1, &cellRect, 
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        
        x += 50;
    }
    
    // Colunas de dados
    for (const auto& col : columns_) {
        RECT cellRect = rc;
        cellRect.left = x + config_.padding;
        cellRect.right = x + col.width - config_.padding;
        
        // Ícone de chave primária
        std::wstring name = col.name;
        if (col.isPrimaryKey) {
            name = L"🔑 " + name;
        }
        
        DrawTextW(hdc, name.c_str(), -1, &cellRect, 
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        
        // Linha separadora
        HPEN linePen = CreatePen(PS_SOLID, 1, colors_.gridLines);
        HPEN oldPen = (HPEN)SelectObject(hdc, linePen);
        MoveToEx(hdc, x + col.width - 1, rc.top, nullptr);
        LineTo(hdc, x + col.width - 1, rc.bottom);
        SelectObject(hdc, oldPen);
        DeleteObject(linePen);
        
        x += col.width;
    }
    
    SelectObject(hdc, oldFont);
}

void PreviewHandler::RenderGrid(HDC hdc, const RECT& rc) {
    HFONT oldFont = (HFONT)SelectObject(hdc, hFontMono_);
    ::SetBkMode(hdc, TRANSPARENT);
    
    int y = rc.top - scrollY_;
    int rowNum = 0;
    
    for (const auto& row : queryResult_.rows) {
        if (y + config_.rowHeight < rc.top) {
            y += config_.rowHeight;
            rowNum++;
            continue;
        }
        
        if (y > rc.bottom) break;
        
        RECT rowRect = rc;
        rowRect.top = y;
        rowRect.bottom = y + config_.rowHeight;
        
        // Cor de fundo alternada
        COLORREF bgColor = (rowNum % 2 == 0) ? colors_.background : colors_.alternateRow;
        
        // Seleção
        if (rowNum == selectedRow_) {
            bgColor = colors_.selection;
        }
        
        HBRUSH rowBrush = CreateSolidBrush(bgColor);
        FillRect(hdc, &rowRect, rowBrush);
        DeleteObject(rowBrush);
        
        int x = -scrollX_;
        
        // Número da linha
        if (config_.showRowNumbers) {
            RECT cellRect = rowRect;
            cellRect.left = x;
            cellRect.right = x + 50;
            
            ::SetTextColor(hdc, colors_.nullValue);
            wchar_t numBuf[16];
            StringCchPrintfW(numBuf, 16, L"%d", rowNum + 1);
            DrawTextW(hdc, numBuf, -1, &cellRect, 
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            x += 50;
        }
        
        // Células de dados
        for (size_t col = 0; col < columns_.size() && col < row.size(); col++) {
            RECT cellRect = rowRect;
            cellRect.left = x + config_.padding;
            cellRect.right = x + columns_[col].width - config_.padding;
            
            RenderCell(hdc, cellRect, row[col], columns_[col]);
            
            x += columns_[col].width;
        }
        
        // Linha horizontal
        HPEN linePen = CreatePen(PS_SOLID, 1, colors_.gridLines);
        HPEN oldPen = (HPEN)SelectObject(hdc, linePen);
        MoveToEx(hdc, rc.left, y + config_.rowHeight - 1, nullptr);
        LineTo(hdc, rc.right, y + config_.rowHeight - 1);
        SelectObject(hdc, oldPen);
        DeleteObject(linePen);
        
        y += config_.rowHeight;
        rowNum++;
    }
    
    SelectObject(hdc, oldFont);
    
    // Informações de resumo no rodapé
    if (queryResult_.truncated || queryResult_.totalRows > 0) {
        RECT footerRect = rc;
        footerRect.top = rc.bottom - 24;
        
        HBRUSH footerBrush = CreateSolidBrush(colors_.headerBackground);
        FillRect(hdc, &footerRect, footerBrush);
        DeleteObject(footerBrush);
        
        SelectObject(hdc, hFont_);
        ::SetTextColor(hdc, colors_.foreground);
        
        std::wstring info = StringUtil::Format(
            L"  Showing %d of %lld rows  |  Table: %s  |  Query time: %.2f ms",
            (int)queryResult_.rows.size(),
            queryResult_.totalRows,
            tableName_.c_str(),
            queryResult_.executionTimeMs
        );
        
        DrawTextW(hdc, info.c_str(), -1, &footerRect, 
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

void PreviewHandler::RenderCell(HDC hdc, const RECT& rc, const CellValue& value, 
    const RenderColumn& col) {
    
    COLORREF textColor = colors_.textValue;
    std::wstring text;
    UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
    
    switch (value.type) {
        case CellValue::Type::Null:
            text = L"NULL";
            textColor = colors_.nullValue;
            format |= DT_CENTER;
            break;
            
        case CellValue::Type::Integer:
        case CellValue::Type::Real:
            text = value.ToString();
            textColor = colors_.numberValue;
            format = DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
            break;
            
        case CellValue::Type::Blob:
            text = value.ToString();
            textColor = colors_.blobValue;
            break;
            
        case CellValue::Type::Text:
            text = value.textValue;
            if (text.length() > 200) {
                text = text.substr(0, 200) + L"...";
            }
            textColor = colors_.textValue;
            break;
    }
    
    ::SetTextColor(hdc, textColor);
    DrawTextW(hdc, text.c_str(), -1, (LPRECT)&rc, format);
}

void PreviewHandler::RenderError(HDC hdc, const RECT& rc, const std::wstring& message) {
    HFONT oldFont = (HFONT)SelectObject(hdc, hFontHeader_);
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, RGB(200, 50, 50));
    
    std::wstring errorText = L"⚠️ Error\n\n" + message;
    
    RECT textRect = rc;
    textRect.left += 20;
    textRect.right -= 20;
    textRect.top += 40;
    
    DrawTextW(hdc, errorText.c_str(), -1, &textRect, 
        DT_CENTER | DT_WORDBREAK);
    
    SelectObject(hdc, oldFont);
}

void PreviewHandler::RenderEmpty(HDC hdc, const RECT& rc) {
    HFONT oldFont = (HFONT)SelectObject(hdc, hFontHeader_);
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, colors_.nullValue);
    
    DrawTextW(hdc, L"📭 No data to display", -1, (LPRECT)&rc, 
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    
    SelectObject(hdc, oldFont);
}

LRESULT CALLBACK PreviewHandler::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    PreviewHandler* handler = nullptr;
    
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        handler = static_cast<PreviewHandler*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(handler));
    } else {
        handler = reinterpret_cast<PreviewHandler*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    
    if (handler) {
        return handler->HandleMessage(uMsg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT PreviewHandler::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwndPreview_, &ps);
            
            // Double buffering
            RECT rc;
            GetClientRect(hwndPreview_, &rc);
            
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
            
            Render(memDC, rc);
            
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
            
            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);
            
            EndPaint(hwndPreview_, &ps);
            return 0;
        }
        
        case WM_SIZE:
            UpdateScrollBars();
            InvalidateRect(hwndPreview_, nullptr, TRUE);
            return 0;
            
        case WM_HSCROLL:
            HandleScroll(LOWORD(wParam), HIWORD(wParam), SB_HORZ);
            return 0;
            
        case WM_VSCROLL:
            HandleScroll(LOWORD(wParam), HIWORD(wParam), SB_VERT);
            return 0;
            
        case WM_MOUSEWHEEL:
            HandleMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
            
        case WM_LBUTTONDOWN: {
            int y = GET_Y_LPARAM(lParam) + scrollY_ - config_.headerHeight;
            if (y >= 0) {
                int newRow = y / config_.rowHeight;
                if (newRow != selectedRow_ && newRow < (int)queryResult_.rows.size()) {
                    selectedRow_ = newRow;
                    InvalidateRect(hwndPreview_, nullptr, TRUE);
                }
            }
            ::SetFocus(hwndPreview_);
            return 0;
        }
        
        case WM_KEYDOWN:
            switch (wParam) {
                case VK_UP:
                    if (selectedRow_ > 0) {
                        selectedRow_--;
                        InvalidateRect(hwndPreview_, nullptr, TRUE);
                    }
                    break;
                case VK_DOWN:
                    if (selectedRow_ < (int)queryResult_.rows.size() - 1) {
                        selectedRow_++;
                        InvalidateRect(hwndPreview_, nullptr, TRUE);
                    }
                    break;
                case VK_PRIOR: // Page Up
                    HandleScroll(SB_PAGEUP, 0, SB_VERT);
                    break;
                case VK_NEXT: // Page Down
                    HandleScroll(SB_PAGEDOWN, 0, SB_VERT);
                    break;
                case VK_HOME:
                    scrollY_ = 0;
                    selectedRow_ = 0;
                    UpdateScrollBars();
                    InvalidateRect(hwndPreview_, nullptr, TRUE);
                    break;
                case VK_END:
                    selectedRow_ = (int)queryResult_.rows.size() - 1;
                    InvalidateRect(hwndPreview_, nullptr, TRUE);
                    break;
            }
            return 0;
            
        case WM_ERASEBKGND:
            return 1; // Handled in WM_PAINT
            
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            UpdateTheme();
            InvalidateRect(hwndPreview_, nullptr, TRUE);
            return 0;
    }
    
    return DefWindowProcW(hwndPreview_, uMsg, wParam, lParam);
}

} // namespace SQLLocalDBView
