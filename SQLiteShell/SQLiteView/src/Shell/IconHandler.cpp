/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** Icon Handler Implementation
*/

#include "IconHandler.h"
#include <shlobj.h>
#include <strsafe.h>
#include <memory>

namespace SQLiteView {

IconHandler::IconHandler()
    : _RefCount(1) {
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
    else if (IsEqualIID(riid, IID_IPersistFile)) {
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
    return InterlockedIncrement(&_RefCount);
}

STDMETHODIMP_(ULONG) IconHandler::Release() {
    LONG count = InterlockedDecrement(&_RefCount);
    if (count == 0) delete this;
    return count;
}

// IPersistFile
STDMETHODIMP IconHandler::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    *pClassID = CLSID_SQLiteViewIcon;
    return S_OK;
}

STDMETHODIMP IconHandler::IsDirty() {
    return S_FALSE;
}

STDMETHODIMP IconHandler::Load(LPCOLESTR pszFileName, DWORD dwMode) {
    if (!pszFileName) return E_POINTER;
    _FilePath = pszFileName;
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
    
    SIZE_T size = (_FilePath.length() + 1) * sizeof(wchar_t);
    *ppszFileName = static_cast<LPOLESTR>(CoTaskMemAlloc(size));
    
    if (!*ppszFileName) return E_OUTOFMEMORY;
    
    StringCchCopyW(*ppszFileName, _FilePath.length() + 1, _FilePath.c_str());
    return S_OK;
}

// IExtractIconW
STDMETHODIMP IconHandler::GetIconLocation(UINT uFlags, LPWSTR pszIconFile, UINT cchMax, int* piIndex, UINT* pwFlags) {
    if (!pszIconFile || !piIndex || !pwFlags) return E_POINTER;
    
    // Get our DLL path to extract icon from it
    wchar_t modulePath[MAX_PATH];
    if (GetModuleFileNameW(g_hModule, modulePath, MAX_PATH) == 0) {
        return E_FAIL;
    }
    
    StringCchCopyW(pszIconFile, cchMax, modulePath);
    
    // Determine icon index based on item type
    *piIndex = 0;  // Default database icon
    
    // Check if we're dealing with a table, view, or record
    if (!_FilePath.empty()) {
        // Use different icons for different file sizes or database states
        // For now, we use a single icon
        if (uFlags & GIL_OPENICON) {
            *piIndex = 1;  // Open database icon
        }
    }
    
    *pwFlags = GIL_PERINSTANCE;
    return S_OK;
}

STDMETHODIMP IconHandler::Extract(LPCWSTR pszFile, UINT nIconIndex, HICON* phiconLarge, HICON* phiconSmall, UINT nIconSize) {
    UNREFERENCED_PARAMETER(pszFile);
    UNREFERENCED_PARAMETER(nIconIndex);
    UNREFERENCED_PARAMETER(phiconLarge);
    UNREFERENCED_PARAMETER(phiconSmall);
    UNREFERENCED_PARAMETER(nIconSize);
    // Let the shell extract the icon using GetIconLocation info
    return S_FALSE;
}

} // namespace SQLiteView
