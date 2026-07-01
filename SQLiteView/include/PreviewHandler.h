#pragma once
/*
** SQLiteView - Windows Explorer Shell Extension
** Preview Handler Implementation
*/

#ifndef SQLITEVIEW_PREVIEWHANDLER_H
#define SQLITEVIEW_PREVIEWHANDLER_H

#include "Common.h"
#include "Database.h"

namespace SQLiteView {

// Cores do tema
struct ThemeColors {
    COLORREF background;
    COLORREF foreground;
    COLORREF headerBackground;
    COLORREF headerForeground;
    COLORREF gridLines;
    COLORREF alternateRow;
    COLORREF selection;
    COLORREF selectionText;
    COLORREF nullValue;
    COLORREF blobValue;
    COLORREF numberValue;
    COLORREF textValue;
    COLORREF border;
    COLORREF scrollbar;
    COLORREF scrollbarThumb;
};

// Configuração visual
struct PreviewConfig {
    int maxRows = 100;
    int maxColumnWidth = 300;
    int minColumnWidth = 60;
    int rowHeight = 24;
    int headerHeight = 32;
    int padding = 8;
    int fontSize = 13;
    int headerFontSize = 14;
    std::wstring fontName = L"Segoe UI";
    std::wstring monoFontName = L"Cascadia Code";
    bool showRowNumbers = true;
    bool showSchema = true;
    bool alternateRowColors = true;
    bool wordWrap = false;
};

// Informações de coluna para renderização
struct RenderColumn {
    std::wstring name;
    std::wstring type;
    int width = 100;
    int minWidth = 60;
    int maxWidth = 400;
    bool isPrimaryKey = false;
    bool isNullable = true;
};

// Preview Handler - Mostra dados da tabela no painel de preview
class PreviewHandler : 
    public IPreviewHandler,
    public IPreviewHandlerVisuals,
    public IOleWindow,
    public IObjectWithSite,
    public IInitializeWithFile,
    public IInitializeWithStream,
    public IInitializeWithItem {
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

    // IInitializeWithStream
    STDMETHODIMP Initialize(IStream* pstream, DWORD grfMode) override;

    // IInitializeWithItem
    STDMETHODIMP Initialize(IShellItem* psi, DWORD grfMode) override;

    // Direct configuration for namespace items
    void SetDatabasePath(const std::wstring& pPath) { filePath_ = pPath; }
    void SetTableName(const std::wstring& pName) { tableName_ = pName; }

private:
    LONG refCount_;
    HWND hwndParent_;
    HWND hwndPreview_;
    RECT previewRect_;
    std::wstring filePath_;
    std::wstring tableName_;
    std::shared_ptr<Database> database_;
    QueryResult queryResult_;
    std::vector<RenderColumn> columns_;
    ComPtr<IUnknown> site_;
    
    ThemeColors colors_;
    PreviewConfig config_;
    HFONT hFont_;
    HFONT hFontHeader_;
    HFONT hFontMono_;
    
    int scrollX_ = 0;
    int scrollY_ = 0;
    int contentWidth_ = 0;
    int contentHeight_ = 0;
    int selectedRow_ = -1;

    // Window management
    bool CreatePreviewWindow();
    void DestroyPreviewWindow();
    void UpdateLayout();
    void Render(HDC hdc, const RECT& rc);
    void RenderHeader(HDC hdc, const RECT& rc);
    void RenderGrid(HDC hdc, const RECT& rc);
    void RenderCell(HDC hdc, const RECT& rc, const CellValue& value, const RenderColumn& col);
    void RenderSchema(HDC hdc, const RECT& rc);
    void RenderError(HDC hdc, const RECT& rc, const std::wstring& message);
    void RenderLoading(HDC hdc, const RECT& rc);
    void RenderEmpty(HDC hdc, const RECT& rc);
    void RenderTablesList(HDC hdc, const RECT& rc);  // Renderiza lista de tabelas

    // Data loading
    bool LoadData();
    bool LoadTablesList();  // Carrega lista de tabelas para visão geral
    void CalculateColumnWidths(HDC hdc);
    
    // Theme
    void InitializeTheme();
    void UpdateTheme();
    bool IsDarkMode();

    // Scrolling
    void UpdateScrollBars();
    void HandleScroll(int scrollType, int nPos, int nBar);
    void HandleMouseWheel(int delta);

    // Helpers
    std::wstring FormatCellValue(const CellValue& value);
    COLORREF GetCellColor(const CellValue& value);
    
    // State
    bool showingTablesList_ = false;  // Indica se estamos mostrando lista de tabelas
    std::vector<TableInfo> tablesList_;  // Lista de tabelas do banco
    
    // Window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    static const wchar_t* WINDOW_CLASS;
    static bool classRegistered_;
};

} // namespace SQLiteView

#endif // SQLITEVIEW_PREVIEWHANDLER_H
