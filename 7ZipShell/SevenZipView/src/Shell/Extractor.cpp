/*
** SevenZipView - Windows Explorer Shell Extension for 7z Archives
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
**
** Extraction Engine Implementation
*/

#include "Extractor.h"
#include <strsafe.h>

namespace SevenZipView {

//==============================================================================
// Helper functions
//==============================================================================

static bool CreateDirectoryRecursive(const std::wstring& path) {
    if (path.empty()) return true;
    
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
        return true;
    
    // Find parent
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos && pos > 0) {
        std::wstring parent = path.substr(0, pos);
        if (!CreateDirectoryRecursive(parent))
            return false;
    }
    
    return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static void SetFileModifiedTime(const std::wstring& path, const FILETIME& ft) {
    if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0) return;
    
    HANDLE hFile = CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        SetFileTime(hFile, nullptr, nullptr, &ft);
        CloseHandle(hFile);
    }
}

//==============================================================================
// Extractor
//==============================================================================

Extractor::Extractor() {
}

Extractor::~Extractor() {
}

ExtractResult Extractor::Extract(
    const std::wstring& archivePath,
    const ExtractOptions& options,
    IExtractProgress* progress) {
    
    ExtractResult result;
    
    auto archive = ArchivePool::Instance().GetArchive(archivePath);
    if (!archive || !archive->IsOpen()) {
        result.ErrorMessage = L"Failed to open archive";
        return result;
    }
    
    // Get entries to extract
    std::vector<ArchiveEntry> entries;
    if (options.ItemIndices.empty()) {
        entries = archive->GetAllEntries();
    } else {
        for (UINT32 idx : options.ItemIndices) {
            ArchiveEntry entry;
            if (archive->GetEntry(idx, entry))
                entries.push_back(entry);
        }
    }
    
    // Calculate totals
    UINT64 totalSize = 0;
    UINT32 totalFiles = 0;
    for (const auto& e : entries) {
        if (!e.IsDirectory()) {
            totalSize += e.Size;
            totalFiles++;
        }
    }
    
    if (progress)
        progress->OnStart(totalFiles, totalSize);
    
    // Ensure destination exists
    if (!CreateDirectoryRecursive(options.DestinationPath)) {
        result.ErrorMessage = L"Failed to create destination directory";
        if (progress)
            progress->OnComplete(false, result.ErrorMessage);
        return result;
    }
    
    // Extract files
    UINT64 bytesExtracted = 0;
    UINT32 filesExtracted = 0;
    
    for (const auto& entry : entries) {
        if (progress && progress->IsCancelled()) {
            result.ErrorMessage = L"Cancelled by user";
            break;
        }
        
        std::wstring destPath = options.DestinationPath;
        if (!destPath.empty() && destPath.back() != L'\\' && destPath.back() != L'/')
            destPath += L'\\';
        
        if (options.PreservePaths)
            destPath += entry.FullPath;
        else
            destPath += entry.Name;
        
        if (entry.IsDirectory()) {
            CreateDirectoryRecursive(destPath);
            continue;
        }
        
        if (progress)
            progress->OnProgress(entry.Name, filesExtracted, bytesExtracted, totalSize);
        
        // Create parent directory
        size_t lastSlash = destPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos)
            CreateDirectoryRecursive(destPath.substr(0, lastSlash));
        
        // Check if file exists
        if (!options.OverwriteExisting && GetFileAttributesW(destPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            result.FilesFailed++;
            result.FailedFiles.push_back(destPath);
            continue;
        }
        
        // Extract the file
        if (archive->ExtractToFile(entry.ArchiveIndex, destPath)) {
            SetFileModifiedTime(destPath, entry.ModifiedTime);
            bytesExtracted += entry.Size;
            filesExtracted++;
        } else {
            result.FilesFailed++;
            result.FailedFiles.push_back(destPath);
        }
    }
    
    result.Success = (result.FilesFailed == 0);
    result.FilesExtracted = filesExtracted;
    result.BytesExtracted = bytesExtracted;
    
    if (progress)
        progress->OnComplete(result.Success, result.ErrorMessage);
    
    return result;
}

bool Extractor::ExtractToBuffer(
    const std::wstring& archivePath,
    UINT32 itemIndex,
    std::vector<BYTE>& buffer) {
    
    auto archive = ArchivePool::Instance().GetArchive(archivePath);
    if (!archive || !archive->IsOpen())
        return false;
    
    return archive->ExtractToBuffer(itemIndex, buffer);
}

bool Extractor::ExtractToFile(
    const std::wstring& archivePath,
    UINT32 itemIndex,
    const std::wstring& destPath) {
    
    SEVENZIPVIEW_LOG(L"Extractor::ExtractToFile: index=%u dest='%s'", itemIndex, destPath.c_str());
    
    auto archive = ArchivePool::Instance().GetArchive(archivePath);
    if (!archive || !archive->IsOpen()) {
        SEVENZIPVIEW_LOG(L"Extractor::ExtractToFile: FAILED - archive not open");
        return false;
    }
    
    // Ensure parent directory exists
    size_t lastSlash = destPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
        CreateDirectoryRecursive(destPath.substr(0, lastSlash));
    
    bool result = archive->ExtractToFile(itemIndex, destPath);
    SEVENZIPVIEW_LOG(L"Extractor::ExtractToFile: result=%d", result ? 1 : 0);
    return result;
}

bool Extractor::TestArchive(
    const std::wstring& archivePath,
    IExtractProgress* progress) {
    
    auto archive = ArchivePool::Instance().GetArchive(archivePath);
    if (!archive || !archive->IsOpen())
        return false;
    
    UINT32 count = archive->GetItemCount();
    if (progress)
        progress->OnStart(count, archive->GetTotalUncompressedSize());
    
    std::vector<BYTE> buffer;
    UINT64 bytesProcessed = 0;
    
    for (UINT32 i = 0; i < count; i++) {
        if (progress && progress->IsCancelled())
            return false;
        
        ArchiveEntry entry;
        if (!archive->GetEntry(i, entry))
            continue;
        
        if (entry.IsDirectory())
            continue;
        
        if (progress)
            progress->OnProgress(entry.Name, i, bytesProcessed, archive->GetTotalUncompressedSize());
        
        buffer.clear();
        if (!archive->ExtractToBuffer(i, buffer)) {
            if (progress)
                progress->OnComplete(false, L"Failed to extract: " + entry.Name);
            return false;
        }
        
        // TODO: Verify CRC if needed
        bytesProcessed += entry.Size;
    }
    
    if (progress)
        progress->OnComplete(true, L"");
    
    return true;
}

bool Extractor::EnsureDirectoryExists(const std::wstring& path) {
    return CreateDirectoryRecursive(path);
}

std::wstring Extractor::MakeValidPath(const std::wstring& basePath, const std::wstring& itemPath) {
    std::wstring result = basePath;
    if (!result.empty() && result.back() != L'\\' && result.back() != L'/')
        result += L'\\';
    result += itemPath;
    return result;
}

//==============================================================================
// ProgressDialog
//==============================================================================

ProgressDialog::ProgressDialog(HWND parent)
    : _ParentHwnd(parent)
    , _DialogHwnd(nullptr)
    , _ProgressBar(nullptr)
    , _StatusText(nullptr)
    , _FileText(nullptr)
    , _Cancelled(false)
    , _TotalItems(0)
    , _TotalSize(0) {
}

ProgressDialog::~ProgressDialog() {
    Hide();
}

void ProgressDialog::Show() {
    if (_DialogHwnd) return;
    CreateDialogWindow();
}

void ProgressDialog::Hide() {
    if (_DialogHwnd) {
        DestroyWindow(_DialogHwnd);
        _DialogHwnd = nullptr;
    }
}

void ProgressDialog::OnStart(UINT32 totalItems, UINT64 totalSize) {
    _TotalItems = totalItems;
    _TotalSize = totalSize;
    _Cancelled = false;
    
    if (_ProgressBar)
        SendMessageW(_ProgressBar, PBM_SETRANGE32, 0, 100);
}

void ProgressDialog::OnProgress(
    const std::wstring& currentFile,
    UINT32 currentItem,
    UINT64 bytesProcessed,
    UINT64 totalBytes) {
    
    UpdateProgress(currentItem, _TotalItems, bytesProcessed, totalBytes);
    SetCurrentFile(currentFile);
    
    // Process messages to keep UI responsive
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void ProgressDialog::OnComplete(bool success, const std::wstring& errorMessage) {
    Hide();
}

bool ProgressDialog::IsCancelled() const {
    return _Cancelled;
}

void ProgressDialog::CreateDialogWindow() {
    // Register window class
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = g_hModule;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SevenZipViewProgress";
    RegisterClassExW(&wc);
    
    // Create dialog window
    _DialogHwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"SevenZipViewProgress",
        L"Extracting...",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 150,
        _ParentHwnd, nullptr, g_hModule, this);
    
    if (!_DialogHwnd) return;
    
    // Create progress bar
    _ProgressBar = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        20, 50, 360, 25,
        _DialogHwnd, nullptr, g_hModule, nullptr);
    
    // Create status text
    _StatusText = CreateWindowExW(0, L"STATIC", L"Preparing...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 20, 360, 20,
        _DialogHwnd, nullptr, g_hModule, nullptr);
    
    // Create file text
    _FileText = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS,
        20, 85, 360, 20,
        _DialogHwnd, nullptr, g_hModule, nullptr);
    
    // Center and show
    RECT rc;
    GetWindowRect(_DialogHwnd, &rc);
    int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
    SetWindowPos(_DialogHwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    ShowWindow(_DialogHwnd, SW_SHOW);
    UpdateWindow(_DialogHwnd);
}

void ProgressDialog::UpdateProgress(UINT32 current, UINT32 total, UINT64 bytes, UINT64 totalBytes) {
    if (!_ProgressBar) return;
    
    int percent = 0;
    if (totalBytes > 0)
        percent = static_cast<int>((bytes * 100) / totalBytes);
    
    SendMessageW(_ProgressBar, PBM_SETPOS, percent, 0);
    
    if (_StatusText) {
        WCHAR text[256];
        StringCchPrintfW(text, 256, L"Extracting %u of %u files (%d%%)", current + 1, total, percent);
        SetWindowTextW(_StatusText, text);
    }
}

void ProgressDialog::SetCurrentFile(const std::wstring& file) {
    if (_FileText)
        SetWindowTextW(_FileText, file.c_str());
}

INT_PTR CALLBACK ProgressDialog::DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ProgressDialog* pThis = reinterpret_cast<ProgressDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    
    switch (msg) {
    case WM_CREATE:
        {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        }
        return 0;
        
    case WM_CLOSE:
        if (pThis)
            pThis->_Cancelled = true;
        return 0;
        
    case WM_DESTROY:
        return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace SevenZipView
