#pragma once
/*
** SevenZipView - Windows Explorer Shell Extension
** 7z Archive Reader
*/

#ifndef SEVENZIPVIEW_ARCHIVE_H
#define SEVENZIPVIEW_ARCHIVE_H

#include "Common.h"
#include "ArchiveEntry.h"
#include <memory>
#include <mutex>

namespace SevenZipView {

// Forward declaration
class Archive;

// Archive pool for caching open archives
class ArchivePool {
public:
    static ArchivePool& Instance();
    
    std::shared_ptr<Archive> GetArchive(const std::wstring& path);
    void Remove(const std::wstring& path);
    void Clear();
    
private:
    ArchivePool() = default;
    ~ArchivePool() = default;
    ArchivePool(const ArchivePool&) = delete;
    ArchivePool& operator=(const ArchivePool&) = delete;
    
    std::mutex _Mutex;
    std::unordered_map<std::wstring, std::weak_ptr<Archive>> _Archives;
};

// Main archive class - wraps 7z SDK
class Archive : public std::enable_shared_from_this<Archive> {
public:
    Archive();
    ~Archive();
    
    // Open an archive file
    bool Open(const std::wstring& path);
    void Close();
    bool IsOpen() const { return _IsOpen; }
    
    // Get archive file path
    const std::wstring& GetPath() const { return _Path; }
    
    // Get number of items
    UINT32 GetItemCount() const;
    
    // Get entry at index
    bool GetEntry(UINT32 index, ArchiveEntry& entry) const;
    
    // Get entry by path
    ArchiveEntry GetEntry(const std::wstring& path) const;
    
    // Get all entries
    std::vector<ArchiveEntry> GetAllEntries() const;
    
    // Get entries in a specific folder path
    std::vector<ArchiveEntry> GetEntriesInFolder(const std::wstring& folderPath) const;
    
    // Get root tree structure
    const ArchiveNode& GetRootNode();
    
    // Extract a single file to a buffer (by index)
    bool ExtractToBuffer(UINT32 index, std::vector<BYTE>& buffer);
    
    // Extract a single file to a buffer (by path)
    bool ExtractToBuffer(const std::wstring& entryPath, std::vector<uint8_t>& buffer);
    
    // Extract a single file to disk (by index)
    bool ExtractToFile(UINT32 index, const std::wstring& destPath);
    
    // Extract a single file to disk (by path)
    bool ExtractToFile(const std::wstring& entryPath, const std::wstring& destPath);
    
    // Extract all files to a directory
    bool ExtractAll(const std::wstring& destDir, 
                    std::function<void(const std::wstring&, UINT64, UINT64)> progress = nullptr);
    
    // Archive statistics
    UINT64 GetTotalUncompressedSize() const;
    UINT64 GetTotalCompressedSize() const;
    UINT32 GetFileCount() const;
    UINT32 GetFolderCount() const;
    
private:
    // Build the tree structure from flat list
    void BuildTree();
    
    // Ensure tree is built
    void EnsureTree();
    
    // Build folder cache for fast lookup
    void BuildFolderCache();
    
    std::wstring        _Path;
    bool                _IsOpen;
    CSzArEx             _Archive;           // 7z archive structure
    CLookToRead2        _LookStream;        // Input stream
    ISzAlloc            _AllocImp;          // Memory allocator
    ISzAlloc            _AllocTempImp;      // Temp allocator
    CFileInStream       _FileStream;        // File stream
    
    mutable std::mutex  _Mutex;
    ArchiveNode         _RootNode;
    bool                _TreeBuilt;
    
    // Folder cache: maps folder path to list of entries in that folder
    std::unordered_map<std::wstring, std::vector<ArchiveEntry>> _FolderCache;
    bool                _FolderCacheBuilt;
    
    // Extraction cache
    UInt32              _BlockIndex;
    Byte*               _OutBuffer;
    size_t              _OutBufferSize;
};

} // namespace SevenZipView

#endif // SEVENZIPVIEW_ARCHIVE_H
