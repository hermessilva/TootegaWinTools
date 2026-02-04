/*
** SevenZipView - Windows Explorer Shell Extension for 7z Archives
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
**
** Icon Handler Implementation
*/

#include "IconHandler.h"
#include <strsafe.h>

namespace SevenZipView {

//==============================================================================
// IconHandler - Icon for .7z files
//==============================================================================

IconHandler::IconHandler()
    : _RefCount(1) {
    InterlockedIncrement(&g_DllRefCount);
}

IconHandler::~IconHandler() {
    InterlockedDecrement(&g_DllRefCount);
}

// IUnknown
STDMETHODIMP IconHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    
    if (riid == IID_IUnknown)
        *ppv = static_cast<IUnknown*>(static_cast<IExtractIconW*>(this));
    else if (riid == IID_IExtractIconW)
        *ppv = static_cast<IExtractIconW*>(this);
    else if (riid == IID_IPersist)
        *ppv = static_cast<IPersist*>(static_cast<IPersistFile*>(this));
    else if (riid == IID_IPersistFile)
        *ppv = static_cast<IPersistFile*>(this);
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) IconHandler::AddRef() {
    return InterlockedIncrement(&_RefCount);
}

STDMETHODIMP_(ULONG) IconHandler::Release() {
    LONG ref = InterlockedDecrement(&_RefCount);
    if (ref == 0)
        delete this;
    return ref;
}

// IPersist
STDMETHODIMP IconHandler::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    *pClassID = CLSID_SevenZipViewIcon;
    return S_OK;
}

// IPersistFile
STDMETHODIMP IconHandler::IsDirty() {
    return S_FALSE;
}

STDMETHODIMP IconHandler::Load(LPCOLESTR pszFileName, DWORD /*dwMode*/) {
    if (!pszFileName) return E_POINTER;
    _FilePath = pszFileName;
    return S_OK;
}

STDMETHODIMP IconHandler::Save(LPCOLESTR /*pszFileName*/, BOOL /*fRemember*/) {
    return E_NOTIMPL;
}

STDMETHODIMP IconHandler::SaveCompleted(LPCOLESTR /*pszFileName*/) {
    return E_NOTIMPL;
}

STDMETHODIMP IconHandler::GetCurFile(LPOLESTR* ppszFileName) {
    if (!ppszFileName) return E_POINTER;
    
    *ppszFileName = static_cast<LPOLESTR>(CoTaskMemAlloc((_FilePath.size() + 1) * sizeof(WCHAR)));
    if (!*ppszFileName) return E_OUTOFMEMORY;
    
    StringCchCopyW(*ppszFileName, _FilePath.size() + 1, _FilePath.c_str());
    return S_OK;
}

// IExtractIconW
STDMETHODIMP IconHandler::GetIconLocation(
    UINT /*uFlags*/,
    LPWSTR pszIconFile,
    UINT cchMax,
    int* piIndex,
    UINT* pwFlags) {
    
    if (!pszIconFile || !piIndex || !pwFlags) return E_POINTER;
    
    // Use shell32.dll's archive icon (icon index 54 is typically a compressed folder)
    StringCchCopyW(pszIconFile, cchMax, L"shell32.dll");
    *piIndex = 54;
    *pwFlags = GIL_NOTFILENAME;
    
    return S_OK;
}

STDMETHODIMP IconHandler::Extract(
    LPCWSTR /*pszFile*/,
    UINT /*nIconIndex*/,
    HICON* phiconLarge,
    HICON* phiconSmall,
    UINT nIconSize) {
    
    // Let shell handle extraction based on GetIconLocation
    return S_FALSE;
}

//==============================================================================
// ItemIconExtractor - Icons for items inside archive
//==============================================================================

ItemIconExtractor::ItemIconExtractor()
    : _RefCount(1)
    , _ItemType(ItemType::File) {
    InterlockedIncrement(&g_DllRefCount);
}

ItemIconExtractor::~ItemIconExtractor() {
    InterlockedDecrement(&g_DllRefCount);
}

// IUnknown
STDMETHODIMP ItemIconExtractor::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    
    if (riid == IID_IUnknown || riid == IID_IExtractIconW)
        *ppv = static_cast<IExtractIconW*>(this);
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) ItemIconExtractor::AddRef() {
    return InterlockedIncrement(&_RefCount);
}

STDMETHODIMP_(ULONG) ItemIconExtractor::Release() {
    LONG ref = InterlockedDecrement(&_RefCount);
    if (ref == 0)
        delete this;
    return ref;
}

void ItemIconExtractor::SetItemInfo(const std::wstring& name, ItemType type) {
    _ItemName = name;
    _ItemType = type;
}

// IExtractIconW
STDMETHODIMP ItemIconExtractor::GetIconLocation(
    UINT /*uFlags*/,
    LPWSTR pszIconFile,
    UINT cchMax,
    int* piIndex,
    UINT* pwFlags) {
    
    if (!pszIconFile || !piIndex || !pwFlags) return E_POINTER;
    
    if (_ItemType == ItemType::Folder) {
        // Folder icon
        StringCchCopyW(pszIconFile, cchMax, L"shell32.dll");
        *piIndex = 3;
        *pwFlags = GIL_NOTFILENAME;
        return S_OK;
    }
    
    // Get extension
    std::wstring ext;
    size_t dotPos = _ItemName.find_last_of(L'.');
    if (dotPos != std::wstring::npos)
        ext = _ItemName.substr(dotPos);
    
    // Map extension to shell icon
    int iconIndex = GetIconIndexForExtension(ext);
    
    StringCchCopyW(pszIconFile, cchMax, L"shell32.dll");
    *piIndex = iconIndex;
    *pwFlags = GIL_NOTFILENAME;
    
    return S_OK;
}

STDMETHODIMP ItemIconExtractor::Extract(
    LPCWSTR /*pszFile*/,
    UINT /*nIconIndex*/,
    HICON* phiconLarge,
    HICON* phiconSmall,
    UINT nIconSize) {
    
    // Let shell handle extraction
    return S_FALSE;
}

int ItemIconExtractor::GetIconIndexForExtension(const std::wstring& ext) const {
    // Map common extensions to shell32.dll icon indices
    if (ext.empty()) return 0; // Generic file
    
    std::wstring lowerExt = ext;
    for (auto& c : lowerExt) c = towlower(c);
    
    if (lowerExt == L".txt" || lowerExt == L".log" || lowerExt == L".ini")
        return 70; // Text file
    if (lowerExt == L".exe" || lowerExt == L".com")
        return 2;  // Application
    if (lowerExt == L".dll")
        return 72; // DLL
    if (lowerExt == L".bat" || lowerExt == L".cmd")
        return 71; // Batch file
    if (lowerExt == L".doc" || lowerExt == L".docx")
        return 1;  // Document
    if (lowerExt == L".htm" || lowerExt == L".html")
        return 242; // HTML
    if (lowerExt == L".jpg" || lowerExt == L".jpeg" || lowerExt == L".png" || 
        lowerExt == L".gif" || lowerExt == L".bmp")
        return 325; // Image
    if (lowerExt == L".mp3" || lowerExt == L".wav" || lowerExt == L".wma")
        return 116; // Audio
    if (lowerExt == L".avi" || lowerExt == L".mp4" || lowerExt == L".wmv")
        return 115; // Video
    if (lowerExt == L".zip" || lowerExt == L".7z" || lowerExt == L".rar")
        return 54;  // Archive
    
    return 0; // Generic file
}

//==============================================================================
// Helper functions
//==============================================================================

HICON GetShellIconForFile(const std::wstring& fileName, bool largeIcon) {
    SHFILEINFOW sfi = {};
    UINT flags = SHGFI_USEFILEATTRIBUTES | SHGFI_ICON;
    flags |= largeIcon ? SHGFI_LARGEICON : SHGFI_SMALLICON;
    
    if (SHGetFileInfoW(fileName.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags))
        return sfi.hIcon;
    
    return nullptr;
}

HICON GetShellFolderIcon(bool largeIcon) {
    SHFILEINFOW sfi = {};
    UINT flags = SHGFI_USEFILEATTRIBUTES | SHGFI_ICON;
    flags |= largeIcon ? SHGFI_LARGEICON : SHGFI_SMALLICON;
    
    if (SHGetFileInfoW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi), flags))
        return sfi.hIcon;
    
    return nullptr;
}

} // namespace SevenZipView
