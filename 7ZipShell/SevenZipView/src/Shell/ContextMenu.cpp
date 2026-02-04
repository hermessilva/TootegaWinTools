/*
** SevenZipView - Windows Explorer Shell Extension for 7z Archives
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
**
** Context Menu Handler Implementation
*/

#include "ContextMenu.h"
#include "Archive.h"
#include "Extractor.h"
#include "ShellFolder.h"
#include <strsafe.h>
#include <shlobj.h>

namespace SevenZipView {

//==============================================================================
// ArchiveContextMenuHandler - Context menu for .7z files
//==============================================================================

ArchiveContextMenuHandler::ArchiveContextMenuHandler()
    : _RefCount(1) {
    InterlockedIncrement(&g_DllRefCount);
}

ArchiveContextMenuHandler::~ArchiveContextMenuHandler() {
    InterlockedDecrement(&g_DllRefCount);
}

// IUnknown
STDMETHODIMP ArchiveContextMenuHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    
    if (riid == IID_IUnknown)
        *ppv = static_cast<IUnknown*>(static_cast<IContextMenu*>(this));
    else if (riid == IID_IContextMenu)
        *ppv = static_cast<IContextMenu*>(this);
    else if (riid == IID_IShellExtInit)
        *ppv = static_cast<IShellExtInit*>(this);
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) ArchiveContextMenuHandler::AddRef() {
    return InterlockedIncrement(&_RefCount);
}

STDMETHODIMP_(ULONG) ArchiveContextMenuHandler::Release() {
    LONG ref = InterlockedDecrement(&_RefCount);
    if (ref == 0)
        delete this;
    return ref;
}

// IShellExtInit
STDMETHODIMP ArchiveContextMenuHandler::Initialize(
    PCIDLIST_ABSOLUTE pidlFolder,
    IDataObject* pDataObj,
    HKEY hkeyProgID) {
    
    if (!pDataObj) return E_INVALIDARG;
    
    FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium;
    
    HRESULT hr = pDataObj->GetData(&fmt, &medium);
    if (FAILED(hr)) return hr;
    
    UINT count = DragQueryFileW(static_cast<HDROP>(medium.hGlobal), 0xFFFFFFFF, nullptr, 0);
    if (count >= 1) {
        WCHAR path[MAX_PATH];
        if (DragQueryFileW(static_cast<HDROP>(medium.hGlobal), 0, path, MAX_PATH))
            _ArchivePath = path;
    }
    
    ReleaseStgMedium(&medium);
    return _ArchivePath.empty() ? E_FAIL : S_OK;
}

// IContextMenu
STDMETHODIMP ArchiveContextMenuHandler::QueryContextMenu(
    HMENU hmenu,
    UINT indexMenu,
    UINT idCmdFirst,
    UINT idCmdLast,
    UINT uFlags) {
    
    if (uFlags & CMF_DEFAULTONLY) return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    
    // Create submenu for 7-Zip options
    HMENU hSubMenu = CreatePopupMenu();
    if (!hSubMenu) return E_FAIL;
    
    InsertMenuW(hSubMenu, 0, MF_BYPOSITION | MF_STRING, idCmdFirst + CMD_EXTRACT_HERE, L"Extract Here");
    InsertMenuW(hSubMenu, 1, MF_BYPOSITION | MF_STRING, idCmdFirst + CMD_EXTRACT_TO_FOLDER, L"Extract to Folder...");
    InsertMenuW(hSubMenu, 2, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hSubMenu, 3, MF_BYPOSITION | MF_STRING, idCmdFirst + CMD_TEST_ARCHIVE, L"Test Archive");
    
    // Insert the submenu
    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask = MIIM_STRING | MIIM_SUBMENU | MIIM_ID;
    mii.wID = idCmdFirst + CMD_COUNT;
    mii.hSubMenu = hSubMenu;
    mii.dwTypeData = const_cast<LPWSTR>(L"7-Zip");
    InsertMenuItemW(hmenu, indexMenu, TRUE, &mii);
    
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, CMD_COUNT + 1);
}

STDMETHODIMP ArchiveContextMenuHandler::InvokeCommand(CMINVOKECOMMANDINFO* pici) {
    if (!pici) return E_POINTER;
    
    // Handle string verb
    if (HIWORD(pici->lpVerb) != 0) return E_INVALIDARG;
    
    UINT cmd = LOWORD(pici->lpVerb);
    
    switch (cmd) {
    case CMD_EXTRACT_HERE:
        return ExtractHere() ? S_OK : E_FAIL;
    case CMD_EXTRACT_TO_FOLDER:
        return ExtractToFolder() ? S_OK : E_FAIL;
    case CMD_TEST_ARCHIVE:
        return TestArchive() ? S_OK : E_FAIL;
    case CMD_OPEN_WITH_7ZIP:
        return OpenWith7Zip() ? S_OK : E_FAIL;
    default:
        return E_INVALIDARG;
    }
}

STDMETHODIMP ArchiveContextMenuHandler::GetCommandString(
    UINT_PTR idCmd,
    UINT uType,
    UINT* pReserved,
    CHAR* pszName,
    UINT cchMax) {
    
    if (uType != GCS_HELPTEXT && uType != GCS_HELPTEXTW) return E_NOTIMPL;
    
    LPCWSTR help = nullptr;
    switch (idCmd) {
    case CMD_EXTRACT_HERE:
        help = L"Extract files to the current folder";
        break;
    case CMD_EXTRACT_TO_FOLDER:
        help = L"Extract files to a subfolder";
        break;
    case CMD_TEST_ARCHIVE:
        help = L"Test archive integrity";
        break;
    default:
        return E_INVALIDARG;
    }
    
    if (uType == GCS_HELPTEXTW)
        StringCchCopyW(reinterpret_cast<LPWSTR>(pszName), cchMax, help);
    else
        WideCharToMultiByte(CP_ACP, 0, help, -1, pszName, cchMax, nullptr, nullptr);
    
    return S_OK;
}

bool ArchiveContextMenuHandler::ExtractHere() {
    if (_ArchivePath.empty()) return false;
    
    // Get the directory of the archive
    std::wstring destDir = _ArchivePath;
    size_t lastSlash = destDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
        destDir = destDir.substr(0, lastSlash);
    
    ExtractOptions opts;
    opts.DestinationPath = destDir;
    opts.PreservePaths = true;
    opts.OverwriteExisting = false;
    
    Extractor ext;
    ExtractResult result = ext.Extract(_ArchivePath, opts, nullptr);
    
    if (!result.Success) {
        MessageBoxW(nullptr, result.ErrorMessage.c_str(), L"Extraction Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    return true;
}

bool ArchiveContextMenuHandler::ExtractToFolder() {
    if (_ArchivePath.empty()) return false;
    
    // Get archive name without extension for default folder name
    std::wstring archiveName = _ArchivePath;
    size_t lastSlash = archiveName.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
        archiveName = archiveName.substr(lastSlash + 1);
    size_t lastDot = archiveName.find_last_of(L'.');
    if (lastDot != std::wstring::npos)
        archiveName = archiveName.substr(0, lastDot);
    
    // Get base directory
    std::wstring baseDir = _ArchivePath;
    lastSlash = baseDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
        baseDir = baseDir.substr(0, lastSlash);
    
    std::wstring destDir = baseDir + L"\\" + archiveName;
    
    ExtractOptions opts;
    opts.DestinationPath = destDir;
    opts.PreservePaths = true;
    opts.OverwriteExisting = false;
    
    Extractor ext;
    ExtractResult result = ext.Extract(_ArchivePath, opts, nullptr);
    
    if (!result.Success) {
        MessageBoxW(nullptr, result.ErrorMessage.c_str(), L"Extraction Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    return true;
}

bool ArchiveContextMenuHandler::TestArchive() {
    if (_ArchivePath.empty()) return false;
    
    Extractor ext;
    bool ok = ext.TestArchive(_ArchivePath, nullptr);
    
    if (ok)
        MessageBoxW(nullptr, L"Archive integrity test passed.", L"Test Archive", MB_OK | MB_ICONINFORMATION);
    else
        MessageBoxW(nullptr, L"Archive integrity test failed!", L"Test Archive", MB_OK | MB_ICONERROR);
    
    return ok;
}

bool ArchiveContextMenuHandler::OpenWith7Zip() {
    // Try to open with 7-Zip FM if installed
    HKEY hKey;
    WCHAR path7z[MAX_PATH] = {};
    DWORD size = sizeof(path7z);
    
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\7-Zip", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"Path", nullptr, nullptr, reinterpret_cast<LPBYTE>(path7z), &size);
        RegCloseKey(hKey);
    }
    
    if (path7z[0]) {
        std::wstring exe = path7z;
        exe += L"\\7zFM.exe";
        ShellExecuteW(nullptr, L"open", exe.c_str(), _ArchivePath.c_str(), nullptr, SW_SHOW);
        return true;
    }
    
    return false;
}

//==============================================================================
// ItemContextMenuHandler - Context menu for items inside archive
//==============================================================================

ItemContextMenuHandler::ItemContextMenuHandler()
    : _RefCount(1), _FolderPIDL(nullptr), _Site(nullptr) {
    InterlockedIncrement(&g_DllRefCount);
}

ItemContextMenuHandler::~ItemContextMenuHandler() {
    if (_FolderPIDL) {
        CoTaskMemFree(_FolderPIDL);
        _FolderPIDL = nullptr;
    }
    InterlockedDecrement(&g_DllRefCount);
}

// IUnknown
STDMETHODIMP ItemContextMenuHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    
    if (riid == IID_IUnknown || riid == IID_IContextMenu)
        *ppv = static_cast<IContextMenu*>(this);
    else if (riid == IID_IObjectWithSite)
        *ppv = static_cast<IObjectWithSite*>(this);
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) ItemContextMenuHandler::AddRef() {
    return InterlockedIncrement(&_RefCount);
}

STDMETHODIMP_(ULONG) ItemContextMenuHandler::Release() {
    LONG ref = InterlockedDecrement(&_RefCount);
    if (ref == 0)
        delete this;
    return ref;
}

void ItemContextMenuHandler::SetArchive(
    const std::wstring& archivePath,
    UINT32 itemIndex,
    const std::wstring& itemPath) {
    _ArchivePath = archivePath;
    _Items.clear();
    _Items.push_back({itemIndex, itemPath});
}

void ItemContextMenuHandler::SetArchiveMultiple(
    const std::wstring& archivePath,
    std::vector<std::pair<UINT32, std::wstring>>&& items) {
    _ArchivePath = archivePath;
    _Items = std::move(items);
}

void ItemContextMenuHandler::SetFolderPIDL(PCIDLIST_ABSOLUTE pidlFolder) {
    if (_FolderPIDL) {
        CoTaskMemFree(_FolderPIDL);
        _FolderPIDL = nullptr;
    }
    if (pidlFolder) {
        _FolderPIDL = ILClone(pidlFolder);
    }
}

// IObjectWithSite
STDMETHODIMP ItemContextMenuHandler::SetSite(IUnknown* pUnkSite) {
    SEVENZIPVIEW_LOG(L"ItemContextMenuHandler::SetSite called with site=%p", pUnkSite);
    _Site = pUnkSite;  // Weak reference - do not AddRef (prevent circular ref)
    return S_OK;
}

STDMETHODIMP ItemContextMenuHandler::GetSite(REFIID riid, void** ppvSite) {
    if (!ppvSite) return E_POINTER;
    if (!_Site) {
        *ppvSite = nullptr;
        return E_FAIL;
    }
    return _Site->QueryInterface(riid, ppvSite);
}

// IContextMenu
STDMETHODIMP ItemContextMenuHandler::QueryContextMenu(
    HMENU hmenu,
    UINT indexMenu,
    UINT idCmdFirst,
    UINT idCmdLast,
    UINT uFlags) {
    
    SEVENZIPVIEW_LOG(L"QueryContextMenu: START - items=%zu uFlags=0x%X", _Items.size(), uFlags);
    
    for (const auto& item : _Items) {
        SEVENZIPVIEW_LOG(L"  Item: index=%u path='%s'", item.first, item.second.c_str());
    }
    
    // Check if all items are folders (for default command decision)
    bool allFolders = true;
    for (const auto& item : _Items) {
        if (item.first != ArchiveEntry::SYNTHETIC_FOLDER_INDEX) {
            // This could be a file or a real folder entry - check later if needed
            allFolders = false;
            break;
        }
    }
    
    SEVENZIPVIEW_LOG(L"QueryContextMenu: allFolders=%d CMF_DEFAULTONLY=%d", 
        allFolders ? 1 : 0, (uFlags & CMF_DEFAULTONLY) ? 1 : 0);
    
    // For CMF_DEFAULTONLY (double-click), we need to provide the default verb
    if (uFlags & CMF_DEFAULTONLY) {
        // Add Open command for both files and folders
        // For folders, OpenItem will navigate; for files, it will extract and open
        SEVENZIPVIEW_LOG(L"QueryContextMenu: CMF_DEFAULTONLY - adding Open command");
        InsertMenuW(hmenu, indexMenu, MF_BYPOSITION | MF_STRING, idCmdFirst + CMD_OPEN, L"Open");
        SetMenuDefaultItem(hmenu, idCmdFirst + CMD_OPEN, FALSE);
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, CMD_COUNT);
    }
    
    // Build menu text based on item count
    std::wstring openText = L"Open";
    std::wstring copyText = L"Copy";
    std::wstring extractText = L"Extract To...";
    
    if (_Items.size() > 1) {
        openText = L"Open " + std::to_wstring(_Items.size()) + L" items";
        copyText = L"Copy " + std::to_wstring(_Items.size()) + L" items";
        extractText = L"Extract " + std::to_wstring(_Items.size()) + L" items To...";
    }
    
    // Open is the default command for files
    InsertMenuW(hmenu, indexMenu++, MF_BYPOSITION | MF_STRING, idCmdFirst + CMD_OPEN, openText.c_str());
    InsertMenuW(hmenu, indexMenu++, MF_BYPOSITION | MF_STRING, idCmdFirst + CMD_COPY, copyText.c_str());
    InsertMenuW(hmenu, indexMenu++, MF_BYPOSITION | MF_STRING, idCmdFirst + CMD_EXTRACT_TO, extractText.c_str());
    InsertMenuW(hmenu, indexMenu++, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hmenu, indexMenu++, MF_BYPOSITION | MF_STRING, idCmdFirst + CMD_PROPERTIES, L"Properties");
    
    // Set Open as the default menu item
    SetMenuDefaultItem(hmenu, idCmdFirst + CMD_OPEN, FALSE);
    
    SEVENZIPVIEW_LOG(L"QueryContextMenu: Full menu added");
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, CMD_COUNT);
}

STDMETHODIMP ItemContextMenuHandler::InvokeCommand(CMINVOKECOMMANDINFO* pici) {
    if (!pici) return E_POINTER;
    
    HWND hwnd = pici->hwnd;
    
    SEVENZIPVIEW_LOG(L"InvokeCommand: START - lpVerb=%p hwnd=%p", pici->lpVerb, hwnd);
    
    // Check if verb is a string (HIWORD != 0)
    if (HIWORD(pici->lpVerb) != 0) {
        // String verb - check for "open"
        const char* verb = pici->lpVerb;
        SEVENZIPVIEW_LOG(L"InvokeCommand: String verb='%S'", verb);
        if (_stricmp(verb, "open") == 0) {
            // For files, extract and open with ShellExecute
            return OpenItem(hwnd) ? S_OK : E_FAIL;
        }
        if (_stricmp(verb, "copy") == 0) {
            return CopyItem(hwnd) ? S_OK : E_FAIL;
        }
        SEVENZIPVIEW_LOG(L"InvokeCommand: Unknown string verb");
        return E_INVALIDARG;
    }
    
    UINT cmd = LOWORD(pici->lpVerb);
    SEVENZIPVIEW_LOG(L"InvokeCommand: Numeric cmd=%u", cmd);
    
    switch (cmd) {
    case CMD_OPEN:
        SEVENZIPVIEW_LOG(L"InvokeCommand: CMD_OPEN");
        return OpenItem(hwnd) ? S_OK : E_FAIL;
    case CMD_COPY:
        SEVENZIPVIEW_LOG(L"InvokeCommand: CMD_COPY");
        return CopyItem(hwnd) ? S_OK : E_FAIL;
    case CMD_EXTRACT_TO:
        SEVENZIPVIEW_LOG(L"InvokeCommand: CMD_EXTRACT_TO");
        return ExtractTo(hwnd) ? S_OK : E_FAIL;
    case CMD_PROPERTIES:
        SEVENZIPVIEW_LOG(L"InvokeCommand: CMD_PROPERTIES");
        return ShowProperties(hwnd) ? S_OK : E_FAIL;
    default:
        SEVENZIPVIEW_LOG(L"InvokeCommand: Unknown cmd=%u", cmd);
        return E_INVALIDARG;
    }
}

STDMETHODIMP ItemContextMenuHandler::GetCommandString(
    UINT_PTR idCmd,
    UINT uType,
    UINT* pReserved,
    CHAR* pszName,
    UINT cchMax) {
    
    return E_NOTIMPL;
}

bool ItemContextMenuHandler::CopyItem(HWND hwnd) {
    SEVENZIPVIEW_LOG(L"CopyItem: START - archive='%s' items=%zu", _ArchivePath.c_str(), _Items.size());
    
    // Extract to temp and put on clipboard
    if (_ArchivePath.empty() || _Items.empty()) {
        SEVENZIPVIEW_LOG(L"CopyItem: FAIL - empty archive path or items");
        return false;
    }
    
    auto archive = ArchivePool::Instance().GetArchive(_ArchivePath);
    if (!archive || !archive->IsOpen()) {
        SEVENZIPVIEW_LOG(L"CopyItem: FAIL - cannot open archive");
        return false;
    }
    
    // Get temp path with hash for stability
    std::wstring baseTempPath = GetTempCachePath();
    SEVENZIPVIEW_LOG(L"CopyItem: baseTempPath='%s'", baseTempPath.c_str());
    
    Extractor ext;
    std::vector<std::wstring> extractedPaths;  // Files and folders to put on clipboard
    
    // Get all entries from archive for folder expansion
    auto allEntries = archive->GetAllEntries();
    SEVENZIPVIEW_LOG(L"CopyItem: allEntries count=%zu", allEntries.size());
    
    // Extract all items
    for (const auto& item : _Items) {
        UINT32 itemIndex = item.first;
        const std::wstring& itemPath = item.second;
        
        SEVENZIPVIEW_LOG(L"CopyItem: Processing item index=%u path='%s'", itemIndex, itemPath.c_str());
        
        // Check if this is a folder (synthetic or real)
        bool isFolder = false;
        if (itemIndex == ArchiveEntry::SYNTHETIC_FOLDER_INDEX) {
            isFolder = true;
            SEVENZIPVIEW_LOG(L"CopyItem: Item is SYNTHETIC folder");
        } else {
            ArchiveEntry entry;
            if (archive->GetEntry(itemIndex, entry)) {
                isFolder = entry.IsDirectory();
                SEVENZIPVIEW_LOG(L"CopyItem: Item isDir=%d", isFolder ? 1 : 0);
            } else {
                SEVENZIPVIEW_LOG(L"CopyItem: GetEntry FAILED for index=%u", itemIndex);
            }
        }
        
        if (isFolder) {
            // For folders: extract all files that are inside this folder
            std::wstring folderPrefix = itemPath;
            if (!folderPrefix.empty() && folderPrefix.back() != L'\\' && folderPrefix.back() != L'/') {
                folderPrefix += L"\\";
            }
            
            // Create the folder in temp
            std::wstring folderName = itemPath;
            size_t pos = itemPath.find_last_of(L"\\/");
            if (pos != std::wstring::npos) {
                folderName = itemPath.substr(pos + 1);
            }
            std::wstring tempFolder = baseTempPath + L"\\" + folderName;
            CreateDirectoryW(tempFolder.c_str(), nullptr);
            SEVENZIPVIEW_LOG(L"CopyItem: Created folder '%s'", tempFolder.c_str());
            
            bool hasContent = false;
            
            // Find and extract all files inside this folder
            for (const auto& entry : allEntries) {
                // Check if this entry is inside the folder
                if (entry.FullPath.length() > folderPrefix.length() &&
                    _wcsnicmp(entry.FullPath.c_str(), folderPrefix.c_str(), folderPrefix.length()) == 0) {
                    
                    if (!entry.IsDirectory() && entry.ArchiveIndex != ArchiveEntry::SYNTHETIC_FOLDER_INDEX) {
                        // Calculate relative path from the folder
                        std::wstring relativePath = entry.FullPath.substr(folderPrefix.length());
                        std::wstring destPath = tempFolder + L"\\" + SanitizePath(relativePath);
                        
                        // Create parent directories
                        size_t lastSlash = destPath.find_last_of(L'\\');
                        if (lastSlash != std::wstring::npos) {
                            std::wstring parentDir = destPath.substr(0, lastSlash);
                            for (size_t i = tempFolder.length() + 1; i < parentDir.length(); i++) {
                                if (parentDir[i] == L'\\') {
                                    CreateDirectoryW(parentDir.substr(0, i).c_str(), nullptr);
                                }
                            }
                            CreateDirectoryW(parentDir.c_str(), nullptr);
                        }
                        
                        // Extract the file
                        if (ext.ExtractToFile(_ArchivePath, entry.ArchiveIndex, destPath)) {
                            hasContent = true;
                            SEVENZIPVIEW_LOG(L"CopyItem: Extracted '%s'", destPath.c_str());
                        } else {
                            SEVENZIPVIEW_LOG(L"CopyItem: FAILED to extract '%s'", destPath.c_str());
                        }
                    }
                }
            }
            
            // Add folder to clipboard list (even if empty, to maintain folder structure)
            if (hasContent || true) {
                extractedPaths.push_back(tempFolder);
            }
        } else {
            // For files: extract the individual file
            std::wstring safePath = SanitizePath(itemPath);
            std::wstring tempFile = baseTempPath + L"\\" + safePath;
            
            // Create parent directories if needed
            size_t lastSlash = tempFile.find_last_of(L'\\');
            if (lastSlash != std::wstring::npos) {
                std::wstring parentDir = tempFile.substr(0, lastSlash);
                for (size_t i = baseTempPath.length() + 1; i < parentDir.length(); i++) {
                    if (parentDir[i] == L'\\') {
                        CreateDirectoryW(parentDir.substr(0, i).c_str(), nullptr);
                    }
                }
                CreateDirectoryW(parentDir.c_str(), nullptr);
            }
            
            SEVENZIPVIEW_LOG(L"CopyItem: FILE - extracting to '%s'", tempFile.c_str());
            
            // Extract the file
            if (ext.ExtractToFile(_ArchivePath, itemIndex, tempFile)) {
                extractedPaths.push_back(tempFile);
                SEVENZIPVIEW_LOG(L"CopyItem: FILE - extracted OK");
            } else {
                SEVENZIPVIEW_LOG(L"CopyItem: FILE - extraction FAILED");
            }
        }
    }
    
    SEVENZIPVIEW_LOG(L"CopyItem: extractedPaths count=%zu", extractedPaths.size());
    
    if (extractedPaths.empty()) {
        SEVENZIPVIEW_LOG(L"CopyItem: FAIL - no files extracted");
        return false;
    }
    
    // Put on clipboard using HDROP format
    if (!OpenClipboard(hwnd)) {
        SEVENZIPVIEW_LOG(L"CopyItem: FAIL - cannot open clipboard");
        return false;
    }
    EmptyClipboard();
    
    // Calculate size needed
    SIZE_T size = sizeof(DROPFILES);
    for (const auto& path : extractedPaths) {
        size += (path.size() + 1) * sizeof(WCHAR);
    }
    size += sizeof(WCHAR);  // Double null terminator
    
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, size);
    if (hMem) {
        DROPFILES* pDrop = static_cast<DROPFILES*>(GlobalLock(hMem));
        pDrop->pFiles = sizeof(DROPFILES);
        pDrop->fWide = TRUE;
        
        WCHAR* pPath = reinterpret_cast<WCHAR*>(pDrop + 1);
        for (const auto& path : extractedPaths) {
            wcscpy_s(pPath, path.size() + 1, path.c_str());
            pPath += path.size() + 1;
            SEVENZIPVIEW_LOG(L"CopyItem: Adding to clipboard '%s'", path.c_str());
        }
        *pPath = L'\0';  // Double null terminator
        
        GlobalUnlock(hMem);
        SetClipboardData(CF_HDROP, hMem);
        SEVENZIPVIEW_LOG(L"CopyItem: SetClipboardData OK");
    }
    
    CloseClipboard();
    SEVENZIPVIEW_LOG(L"CopyItem: SUCCESS");
    return true;
}

bool ItemContextMenuHandler::ExtractTo(HWND hwnd) {
    if (_ArchivePath.empty() || _Items.empty()) return false;
    
    // Show folder picker
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        IID_IFileDialog, reinterpret_cast<void**>(&pfd));
    
    if (SUCCEEDED(hr)) {
        DWORD opts;
        pfd->GetOptions(&opts);
        pfd->SetOptions(opts | FOS_PICKFOLDERS);
        
        if (SUCCEEDED(pfd->Show(hwnd))) {
            IShellItem* psi = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psi))) {
                LPWSTR pszPath = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                    Extractor ext;
                    bool allOk = true;
                    
                    for (const auto& item : _Items) {
                        UINT32 itemIndex = item.first;
                        const std::wstring& itemPath = item.second;
                        
                        // Get just the filename
                        std::wstring fileName = itemPath;
                        size_t pos = itemPath.find_last_of(L"\\/");
                        if (pos != std::wstring::npos) {
                            fileName = itemPath.substr(pos + 1);
                        }
                        
                        std::wstring destPath = std::wstring(pszPath) + L"\\" + fileName;
                        if (!ext.ExtractToFile(_ArchivePath, itemIndex, destPath)) {
                            allOk = false;
                        }
                    }
                    
                    CoTaskMemFree(pszPath);
                    psi->Release();
                    pfd->Release();
                    return allOk;
                }
                psi->Release();
            }
        }
        pfd->Release();
    }
    
    return false;
}

bool ItemContextMenuHandler::ShowProperties(HWND hwnd) {
    if (_Items.empty()) return false;
    
    // Show a simple property dialog
    auto archive = ArchivePool::Instance().GetArchive(_ArchivePath);
    if (!archive || !archive->IsOpen()) return false;
    
    std::wstringstream ss;
    
    if (_Items.size() == 1) {
        ArchiveEntry entry;
        if (!archive->GetEntry(_Items[0].first, entry)) return false;
        
        ss << L"Name: " << entry.Name << L"\n";
        ss << L"Size: " << entry.Size << L" bytes\n";
        ss << L"Compressed: " << entry.CompressedSize << L" bytes\n";
        ss << L"CRC: " << std::hex << entry.CRC << L"\n";
        ss << L"Type: " << (entry.IsDirectory() ? L"Folder" : L"File");
    } else {
        ss << L"Selected items: " << _Items.size() << L"\n\n";
        UINT64 totalSize = 0;
        UINT64 totalCompressed = 0;
        
        for (const auto& item : _Items) {
            ArchiveEntry entry;
            if (archive->GetEntry(item.first, entry)) {
                totalSize += entry.Size;
                totalCompressed += entry.CompressedSize;
            }
        }
        
        ss << L"Total size: " << totalSize << L" bytes\n";
        ss << L"Total compressed: " << totalCompressed << L" bytes";
    }
    
    MessageBoxW(hwnd, ss.str().c_str(), L"Properties", MB_OK | MB_ICONINFORMATION);
    return true;
}

std::wstring ItemContextMenuHandler::GetTempCachePath() {
    // Create a stable temp path based on archive path hash
    WCHAR tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    
    // Hash archive path for unique folder name
    std::hash<std::wstring> hasher;
    size_t hashValue = hasher(_ArchivePath);
    
    WCHAR cachePath[MAX_PATH];
    StringCchPrintfW(cachePath, MAX_PATH, L"%sSevenZipView\\%zx", tempPath, hashValue);
    
    // Create directory structure
    std::wstring baseDir = tempPath;
    baseDir += L"SevenZipView";
    CreateDirectoryW(baseDir.c_str(), nullptr);
    CreateDirectoryW(cachePath, nullptr);
    
    return cachePath;
}

std::wstring ItemContextMenuHandler::SanitizePath(const std::wstring& path) {
    std::wstring result = path;
    
    // Normalize separators
    for (auto& c : result) {
        if (c == L'/') c = L'\\';
    }
    
    // Remove dangerous path traversal sequences
    size_t pos;
    while ((pos = result.find(L"..\\")) != std::wstring::npos) {
        result.erase(pos, 3);
    }
    while ((pos = result.find(L"..")) != std::wstring::npos) {
        result.erase(pos, 2);
    }
    
    // Remove leading backslashes
    while (!result.empty() && result[0] == L'\\') {
        result.erase(0, 1);
    }
    
    // Replace invalid characters
    for (auto& c : result) {
        if (c == L':' || c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|') {
            c = L'_';
        }
    }
    
    return result;
}

bool ItemContextMenuHandler::NavigateToFolder(HWND /*hwnd*/, const std::wstring& folderPath) {
    SEVENZIPVIEW_LOG(L"NavigateToFolder: START - folder='%s' FolderPIDL=%p Site=%p", 
        folderPath.c_str(), _FolderPIDL, _Site);
    
    if (!_FolderPIDL) {
        SEVENZIPVIEW_LOG(L"NavigateToFolder: FAIL - no folder PIDL");
        return false;
    }
    
    // Create a PIDL for the folder item
    std::wstring folderName = folderPath;
    
    // Extract just the folder name (last component)
    size_t lastSep = folderPath.find_last_of(L"\\/");
    if (lastSep != std::wstring::npos) {
        folderName = folderPath.substr(lastSep + 1);
    }
    
    SEVENZIPVIEW_LOG(L"NavigateToFolder: folderName='%s'", folderName.c_str());
    
    // Allocate PIDL with ItemData structure (same as ShellFolder::CreateItemID)
    size_t itemSize = sizeof(ItemData);
    size_t totalSize = itemSize + sizeof(USHORT); // Terminator
    
    PITEMID_CHILD itemPidl = static_cast<PITEMID_CHILD>(CoTaskMemAlloc(totalSize));
    if (!itemPidl) {
        SEVENZIPVIEW_LOG(L"NavigateToFolder: FAIL - CoTaskMemAlloc failed");
        return false;
    }
    
    ZeroMemory(itemPidl, totalSize);
    
    ItemData* data = reinterpret_cast<ItemData*>(&itemPidl->mkid);
    data->cb = static_cast<USHORT>(itemSize);
    data->signature = ItemData::SIGNATURE;
    data->type = ItemType::Folder;
    data->archiveIndex = ArchiveEntry::SYNTHETIC_FOLDER_INDEX;
    data->size = 0;
    data->compressedSize = 0;
    data->crc = 0;
    data->attributes = FILE_ATTRIBUTE_DIRECTORY;
    GetSystemTimeAsFileTime(&data->modifiedTime);
    
    StringCchCopyW(data->name, ARRAYSIZE(data->name), folderName.c_str());
    StringCchCopyW(data->path, ARRAYSIZE(data->path), folderPath.c_str());
    
    // Method 1: Use IShellBrowser::BrowseObject for in-place navigation (preferred)
    if (_Site) {
        IShellBrowser* pShellBrowser = nullptr;
        
        // Try to get IShellBrowser via IServiceProvider
        IServiceProvider* pServiceProvider = nullptr;
        if (SUCCEEDED(_Site->QueryInterface(IID_IServiceProvider, (void**)&pServiceProvider))) {
            pServiceProvider->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&pShellBrowser);
            pServiceProvider->Release();
        }
        
        // Fallback: try direct QueryInterface
        if (!pShellBrowser) {
            _Site->QueryInterface(IID_IShellBrowser, (void**)&pShellBrowser);
        }
        
        if (pShellBrowser) {
            SEVENZIPVIEW_LOG(L"NavigateToFolder: Using IShellBrowser::BrowseObject with relative PIDL");
            
            // BrowseObject with relative PIDL navigates into the subfolder
            HRESULT hr = pShellBrowser->BrowseObject(itemPidl, SBSP_RELATIVE | SBSP_SAMEBROWSER);
            pShellBrowser->Release();
            CoTaskMemFree(itemPidl);
            
            if (SUCCEEDED(hr)) {
                SEVENZIPVIEW_LOG(L"NavigateToFolder: BrowseObject SUCCESS");
                return true;
            }
            SEVENZIPVIEW_LOG(L"NavigateToFolder: BrowseObject FAILED hr=0x%08X", hr);
        }
    }
    
    // Method 2: Fallback to SHOpenFolderAndSelectItems
    SEVENZIPVIEW_LOG(L"NavigateToFolder: Falling back to SHOpenFolderAndSelectItems");
    
    // Combine with folder PIDL to get absolute PIDL
    PIDLIST_ABSOLUTE targetPidl = ILCombine(_FolderPIDL, itemPidl);
    CoTaskMemFree(itemPidl);
    
    if (!targetPidl) {
        SEVENZIPVIEW_LOG(L"NavigateToFolder: FAIL - ILCombine failed");
        return false;
    }
    
    HRESULT hr = SHOpenFolderAndSelectItems(targetPidl, 0, nullptr, 0);
    CoTaskMemFree(targetPidl);
    
    if (FAILED(hr)) {
        SEVENZIPVIEW_LOG(L"NavigateToFolder: SHOpenFolderAndSelectItems FAILED hr=0x%08X", hr);
        return false;
    }
    
    SEVENZIPVIEW_LOG(L"NavigateToFolder: SUCCESS (via SHOpenFolderAndSelectItems)");
    return true;
}

bool ItemContextMenuHandler::OpenItem(HWND hwnd) {
    SEVENZIPVIEW_LOG(L"OpenItem: START - archive='%s' items=%zu", _ArchivePath.c_str(), _Items.size());
    
    if (_ArchivePath.empty() || _Items.empty()) {
        SEVENZIPVIEW_LOG(L"OpenItem: FAIL - empty archive or items");
        return false;
    }
    
    auto archive = ArchivePool::Instance().GetArchive(_ArchivePath);
    if (!archive || !archive->IsOpen()) {
        SEVENZIPVIEW_LOG(L"OpenItem: FAIL - cannot open archive");
        return false;
    }
    
    std::wstring cachePath = GetTempCachePath();
    SEVENZIPVIEW_LOG(L"OpenItem: cachePath='%s'", cachePath.c_str());
    
    Extractor ext;
    bool allOk = true;
    
    for (const auto& item : _Items) {
        UINT32 itemIndex = item.first;
        const std::wstring& itemPath = item.second;
        
        SEVENZIPVIEW_LOG(L"OpenItem: Processing item index=%u path='%s'", itemIndex, itemPath.c_str());
        
        // Handle synthetic folders (virtual folders created from paths)
        if (itemIndex == ArchiveEntry::SYNTHETIC_FOLDER_INDEX) {
            SEVENZIPVIEW_LOG(L"OpenItem: SYNTHETIC folder - navigating");
            if (!NavigateToFolder(hwnd, itemPath)) {
                allOk = false;
            }
            continue;
        }
        
        // Check if this is a real folder entry in the archive
        ArchiveEntry entry;
        if (archive->GetEntry(itemIndex, entry) && entry.IsDirectory()) {
            SEVENZIPVIEW_LOG(L"OpenItem: Real folder - navigating");
            if (!NavigateToFolder(hwnd, itemPath)) {
                allOk = false;
            }
            continue;
        }
        
        // Sanitize and preserve the full path structure
        std::wstring safePath = SanitizePath(itemPath);
        std::wstring tempFile = cachePath + L"\\" + safePath;
        
        SEVENZIPVIEW_LOG(L"OpenItem: tempFile='%s'", tempFile.c_str());
        
        // Create parent directories if needed
        size_t lastSlash = tempFile.find_last_of(L'\\');
        if (lastSlash != std::wstring::npos) {
            std::wstring parentDir = tempFile.substr(0, lastSlash);
            // Create all parent directories
            for (size_t i = 0; i < parentDir.length(); i++) {
                if (parentDir[i] == L'\\') {
                    CreateDirectoryW(parentDir.substr(0, i).c_str(), nullptr);
                }
            }
            CreateDirectoryW(parentDir.c_str(), nullptr);
        }
        
        // Extract the file if it doesn't exist or is older than archive
        bool needExtract = true;
        WIN32_FILE_ATTRIBUTE_DATA fileAttr;
        WIN32_FILE_ATTRIBUTE_DATA archiveAttr;
        
        if (GetFileAttributesExW(tempFile.c_str(), GetFileExInfoStandard, &fileAttr) &&
            GetFileAttributesExW(_ArchivePath.c_str(), GetFileExInfoStandard, &archiveAttr)) {
            // File exists - check if archive is newer
            if (CompareFileTime(&archiveAttr.ftLastWriteTime, &fileAttr.ftLastWriteTime) <= 0) {
                needExtract = false;  // Cache is still valid
                SEVENZIPVIEW_LOG(L"OpenItem: Using cached file");
            }
        }
        
        if (needExtract) {
            SEVENZIPVIEW_LOG(L"OpenItem: Extracting file...");
            if (!ext.ExtractToFile(_ArchivePath, itemIndex, tempFile)) {
                SEVENZIPVIEW_LOG(L"OpenItem: Extraction FAILED");
                allOk = false;
                continue;
            }
            SEVENZIPVIEW_LOG(L"OpenItem: Extraction OK");
        }
        
        // Open the file with ShellExecute
        SEVENZIPVIEW_LOG(L"OpenItem: Calling ShellExecute");
        HINSTANCE result = ShellExecuteW(hwnd, L"open", tempFile.c_str(), 
                                          nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)result <= 32) {
            SEVENZIPVIEW_LOG(L"OpenItem: ShellExecute FAILED with code %d", (int)(INT_PTR)result);
            allOk = false;
        } else {
            SEVENZIPVIEW_LOG(L"OpenItem: ShellExecute OK");
        }
    }
    
    SEVENZIPVIEW_LOG(L"OpenItem: END - allOk=%d", allOk ? 1 : 0);
    return allOk;
}

} // namespace SevenZipView
