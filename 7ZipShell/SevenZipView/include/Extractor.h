#pragma once
/*
** SevenZipView - Windows Explorer Shell Extension
** Extraction Engine with Progress
*/

#ifndef SEVENZIPVIEW_EXTRACTOR_H
#define SEVENZIPVIEW_EXTRACTOR_H

#include "Common.h"
#include "Archive.h"

namespace SevenZipView {

// Progress callback interface
class IExtractProgress {
public:
    virtual ~IExtractProgress() = default;
    
    // Called when starting extraction
    virtual void OnStart(UINT32 totalItems, UINT64 totalSize) = 0;
    
    // Called for each file
    virtual void OnProgress(const std::wstring& currentFile, UINT32 currentItem, 
                           UINT64 bytesProcessed, UINT64 totalBytes) = 0;
    
    // Called when extraction is complete       
    virtual void OnComplete(bool success, const std::wstring& errorMessage) = 0;
    
    // Check if user cancelled
    virtual bool IsCancelled() const = 0;
};

// Extraction operation options
struct ExtractOptions {
    std::wstring DestinationPath;       // Where to extract
    bool PreservePaths;                  // Keep folder structure
    bool OverwriteExisting;              // Overwrite files
    std::vector<UINT32> ItemIndices;     // Items to extract (empty = all)
    std::wstring Password;               // Password for encrypted archives
    
    ExtractOptions() : PreservePaths(true), OverwriteExisting(false) {}
};

// Extraction result
struct ExtractResult {
    bool Success;
    UINT32 FilesExtracted;
    UINT32 FilesFailed;
    UINT64 BytesExtracted;
    std::wstring ErrorMessage;
    std::vector<std::wstring> FailedFiles;
    
    ExtractResult() : Success(false), FilesExtracted(0), FilesFailed(0), BytesExtracted(0) {}
};

// Main extraction class
class Extractor {
public:
    Extractor();
    ~Extractor();
    
    // Extract with progress callback
    ExtractResult Extract(const std::wstring& archivePath, 
                         const ExtractOptions& options,
                         IExtractProgress* progress = nullptr);
    
    // Extract single item to buffer
    bool ExtractToBuffer(const std::wstring& archivePath, 
                        UINT32 itemIndex,
                        std::vector<BYTE>& buffer);
    
    // Extract single item to file
    bool ExtractToFile(const std::wstring& archivePath,
                      UINT32 itemIndex,
                      const std::wstring& destPath);
    
    // Test archive integrity
    bool TestArchive(const std::wstring& archivePath,
                    IExtractProgress* progress = nullptr);

private:
    bool EnsureDirectoryExists(const std::wstring& path);
    std::wstring MakeValidPath(const std::wstring& basePath, const std::wstring& itemPath);
};

// Progress dialog (optional, for UI operations)
class ProgressDialog : public IExtractProgress {
public:
    ProgressDialog(HWND parent);
    ~ProgressDialog();
    
    // Show the dialog (non-blocking)
    void Show();
    
    // Hide the dialog
    void Hide();
    
    // IExtractProgress
    void OnStart(UINT32 totalItems, UINT64 totalSize) override;
    void OnProgress(const std::wstring& currentFile, UINT32 currentItem,
                   UINT64 bytesProcessed, UINT64 totalBytes) override;
    void OnComplete(bool success, const std::wstring& errorMessage) override;
    bool IsCancelled() const override;

private:
    HWND _ParentHwnd;
    HWND _DialogHwnd;
    HWND _ProgressBar;
    HWND _StatusText;
    HWND _FileText;
    bool _Cancelled;
    
    UINT32 _TotalItems;
    UINT64 _TotalSize;
    
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void CreateDialogWindow();
    void UpdateProgress(UINT32 current, UINT32 total, UINT64 bytes, UINT64 totalBytes);
    void SetCurrentFile(const std::wstring& file);
};

} // namespace SevenZipView

#endif // SEVENZIPVIEW_EXTRACTOR_H
