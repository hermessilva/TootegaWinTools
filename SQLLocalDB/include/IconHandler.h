#pragma once
/*
** SQLLocalDBView - Windows Explorer Shell Extension
** Icon Overlay and Thumbnail Provider
*/

#ifndef SQLLocalDBView_ICONHANDLER_H
#define SQLLocalDBView_ICONHANDLER_H

#include "Common.h"
#include "Database.h"

namespace SQLLocalDBView {

// Tipo de ícone
enum class IconType {
    Database,
    Table,
    View,
    Index,
    SystemTable,
    Trigger,
    Column,
    PrimaryKey,
    ForeignKey,
    Error
};

// Icon Provider - Fornece ícones para os itens
class IconHandler : 
    public IExtractIconW,
    public IExtractIconA,
    public IPersistFile {
public:
    IconHandler();
    virtual ~IconHandler();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IExtractIconW
    STDMETHODIMP GetIconLocation(UINT uFlags, LPWSTR pszIconFile, UINT cchMax,
        int* piIndex, UINT* pwFlags) override;
    STDMETHODIMP Extract(LPCWSTR pszFile, UINT nIconIndex,
        HICON* phiconLarge, HICON* phiconSmall, UINT nIconSize) override;

    // IExtractIconA
    STDMETHODIMP GetIconLocation(UINT uFlags, LPSTR pszIconFile, UINT cchMax,
        int* piIndex, UINT* pwFlags) override;
    STDMETHODIMP Extract(LPCSTR pszFile, UINT nIconIndex,
        HICON* phiconLarge, HICON* phiconSmall, UINT nIconSize) override;

    // IPersist
    STDMETHODIMP GetClassID(CLSID* pClassID) override;

    // IPersistFile
    STDMETHODIMP IsDirty() override;
    STDMETHODIMP Load(LPCOLESTR pszFileName, DWORD dwMode) override;
    STDMETHODIMP Save(LPCOLESTR pszFileName, BOOL fRemember) override;
    STDMETHODIMP SaveCompleted(LPCOLESTR pszFileName) override;
    STDMETHODIMP GetCurFile(LPOLESTR* ppszFileName) override;

    // Definir tipo manualmente
    void SetIconType(IconType type) { iconType_ = type; }
    void SetItemName(const std::wstring& name) { itemName_ = name; }

private:
    LONG refCount_;
    std::wstring filePath_;
    std::wstring itemName_;
    IconType iconType_ = IconType::Database;

    HICON CreateDatabaseIcon(int size);
    HICON CreateTableIcon(int size);
    HICON CreateViewIcon(int size);
    HICON CreateIndexIcon(int size);
    
    void DrawDatabaseIcon(HDC hdc, int size, bool isDark);
    void DrawTableIcon(HDC hdc, int size, bool isDark);
    void DrawViewIcon(HDC hdc, int size, bool isDark);
    void DrawIndexIcon(HDC hdc, int size, bool isDark);
};

// Thumbnail Provider - Gera miniaturas para arquivos .db
class ThumbnailProvider :
    public IThumbnailProvider,
    public IInitializeWithFile,
    public IInitializeWithStream {
public:
    ThumbnailProvider();
    virtual ~ThumbnailProvider();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IThumbnailProvider
    STDMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) override;

    // IInitializeWithFile
    STDMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;

    // IInitializeWithStream
    STDMETHODIMP Initialize(IStream* pstream, DWORD grfMode) override;

private:
    LONG refCount_;
    std::wstring filePath_;

    HBITMAP CreateThumbnail(UINT size);
    void DrawThumbnail(HDC hdc, UINT size, const DatabaseInfo& info);
};

// Utilitários de desenho
namespace IconDraw {
    void FillRoundRect(HDC hdc, const RECT& rc, int radius, COLORREF color);
    void DrawRoundRect(HDC hdc, const RECT& rc, int radius, int width, COLORREF color);
    void DrawText(HDC hdc, const RECT& rc, const wchar_t* text, COLORREF color,
        int fontSize, const wchar_t* fontName, UINT format);
    HBITMAP CreateAlphaBitmap(int width, int height);
    bool IsDarkTheme();
}

} // namespace SQLLocalDBView

#endif // SQLLocalDBView_ICONHANDLER_H
