#pragma once
/*
** SevenZipView - Windows Explorer Shell Extension
** Archive Entry Information
*/

#ifndef SEVENZIPVIEW_ARCHIVEENTRY_H
#define SEVENZIPVIEW_ARCHIVEENTRY_H

#include "Common.h"

namespace SevenZipView {

// Represents a single entry in a 7z archive
struct ArchiveEntry {
    std::wstring    Name;               // File/folder name (without path)
    std::wstring    FullPath;           // Full path inside archive
    ItemType        Type;               // File or Folder
    UINT64          Size;               // Uncompressed size
    UINT64          CompressedSize;     // Compressed size
    FILETIME        ModifiedTime;       // Last modified time
    FILETIME        CreatedTime;        // Creation time
    UINT32          CRC;                // CRC32 checksum
    UINT32          Attributes;         // File attributes
    UINT32          ArchiveIndex;       // Index in the archive (SYNTHETIC_FOLDER_INDEX for synthetic folders)
    bool            IsEncrypted;        // True if encrypted
    std::wstring    Method;             // Compression method name
    
    // Sentinel value for synthetic folders (folders created from path parsing, not explicit archive entries)
    static constexpr UINT32 SYNTHETIC_FOLDER_INDEX = 0xFFFFFFFF;
    
    ArchiveEntry() 
        : Type(ItemType::File)
        , Size(0)
        , CompressedSize(0)
        , CRC(0)
        , Attributes(0)
        , ArchiveIndex(0)
        , IsEncrypted(false) {
        ZeroMemory(&ModifiedTime, sizeof(ModifiedTime));
        ZeroMemory(&CreatedTime, sizeof(CreatedTime));
    }
    
    bool IsDirectory() const { return Type == ItemType::Folder; }
    
    // Check if this is a synthetic folder (not an explicit archive entry)
    bool IsSyntheticFolder() const { 
        return Type == ItemType::Folder && ArchiveIndex == SYNTHETIC_FOLDER_INDEX; 
    }
    
    // Get just the filename from full path
    std::wstring GetFileName() const {
        if (!Name.empty()) return Name;
        size_t pos = FullPath.find_last_of(L"\\/");
        if (pos == std::wstring::npos) return FullPath;
        return FullPath.substr(pos + 1);
    }
    
    // Get parent path
    std::wstring GetParentPath() const {
        size_t pos = FullPath.find_last_of(L"\\/");
        if (pos == std::wstring::npos) return L"";
        return FullPath.substr(0, pos);
    }
};

// Tree node for hierarchical representation
struct ArchiveNode {
    ArchiveEntry                    Entry;
    std::vector<ArchiveNode>        Children;
    ArchiveNode*                    Parent;
    
    ArchiveNode() : Parent(nullptr) {}
    
    // Find child by name
    ArchiveNode* FindChild(const std::wstring& name) {
        for (auto& child : Children) {
            if (_wcsicmp(child.Entry.Name.c_str(), name.c_str()) == 0)
                return &child;
        }
        return nullptr;
    }
    
    // Add child
    ArchiveNode* AddChild(const ArchiveEntry& entry) {
        Children.push_back({});
        auto& child = Children.back();
        child.Entry = entry;
        child.Parent = this;
        return &child;
    }
    
    // Count files recursively
    size_t CountFiles() const {
        size_t count = (Entry.Type == ItemType::File) ? 1 : 0;
        for (const auto& child : Children)
            count += child.CountFiles();
        return count;
    }
    
    // Calculate total size recursively
    UINT64 TotalSize() const {
        UINT64 total = Entry.Size;
        for (const auto& child : Children)
            total += child.TotalSize();
        return total;
    }
};

} // namespace SevenZipView

#endif // SEVENZIPVIEW_ARCHIVEENTRY_H
