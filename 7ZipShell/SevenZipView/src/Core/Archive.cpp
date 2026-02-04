/*
** SevenZipView - Windows Explorer Shell Extension
** 7z Archive Reader Implementation
*/

#include "Archive.h"
#include <set>
#include <shlobj.h>

namespace SevenZipView {

// Case-insensitive string comparator for std::set
struct CaseInsensitiveCompare {
    bool operator()(const std::wstring& a, const std::wstring& b) const {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    }
};

// Memory allocation callbacks for 7z SDK
static void* SzAlloc(ISzAllocPtr p, size_t size) {
    (void)p;
    return malloc(size);
}

static void SzFree(ISzAllocPtr p, void* address) {
    (void)p;
    free(address);
}

// Archive Pool Implementation
ArchivePool& ArchivePool::Instance() {
    static ArchivePool instance;
    return instance;
}

std::shared_ptr<Archive> ArchivePool::GetArchive(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(_Mutex);
    
    auto it = _Archives.find(path);
    if (it != _Archives.end()) {
        auto ptr = it->second.lock();
        if (ptr) return ptr;
    }
    
    auto archive = std::make_shared<Archive>();
    if (archive->Open(path)) {
        _Archives[path] = archive;
        return archive;
    }
    
    return nullptr;
}

void ArchivePool::Remove(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(_Mutex);
    _Archives.erase(path);
}

void ArchivePool::Clear() {
    std::lock_guard<std::mutex> lock(_Mutex);
    _Archives.clear();
}

// Archive Implementation
Archive::Archive()
    : _IsOpen(false)
    , _TreeBuilt(false)
    , _FolderCacheBuilt(false)
    , _BlockIndex(0xFFFFFFFF)
    , _OutBuffer(nullptr)
    , _OutBufferSize(0) {
    
    // Initialize allocators
    _AllocImp.Alloc = SzAlloc;
    _AllocImp.Free = SzFree;
    _AllocTempImp.Alloc = SzAlloc;
    _AllocTempImp.Free = SzFree;
    
    // Initialize CRC table (must be done once)
    static bool crcInitialized = false;
    if (!crcInitialized) {
        CrcGenerateTable();
        crcInitialized = true;
    }
    
    SzArEx_Init(&_Archive);
}

Archive::~Archive() {
    Close();
}

bool Archive::Open(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(_Mutex);
    
    if (_IsOpen) Close();
    
    SEVENZIPVIEW_LOG(L"Opening archive: %s", path.c_str());
    
    // Open file
    WRes wres = InFile_OpenW(&_FileStream.file, path.c_str());
    if (wres != 0) {
        SEVENZIPVIEW_LOG(L"  Failed to open file: error=%d", wres);
        return false;
    }
    
    // Setup stream wrappers
    FileInStream_CreateVTable(&_FileStream);
    LookToRead2_CreateVTable(&_LookStream, False);
    
    _LookStream.buf = nullptr;
    _LookStream.bufSize = (1 << 18); // 256KB buffer
    _LookStream.buf = (Byte*)ISzAlloc_Alloc(&_AllocImp, _LookStream.bufSize);
    if (!_LookStream.buf) {
        File_Close(&_FileStream.file);
        SEVENZIPVIEW_LOG(L"  Failed to allocate buffer");
        return false;
    }
    
    _LookStream.realStream = &_FileStream.vt;
    LookToRead2_INIT(&_LookStream);
    
    // Open archive
    SRes res = SzArEx_Open(&_Archive, &_LookStream.vt, &_AllocImp, &_AllocTempImp);
    if (res != SZ_OK) {
        SEVENZIPVIEW_LOG(L"  Failed to open archive: error=%d", res);
        ISzAlloc_Free(&_AllocImp, _LookStream.buf);
        _LookStream.buf = nullptr;
        File_Close(&_FileStream.file);
        return false;
    }
    
    _Path = path;
    _IsOpen = true;
    _TreeBuilt = false;
    
    SEVENZIPVIEW_LOG(L"  Archive opened successfully: %u files", _Archive.NumFiles);
    
    return true;
}

void Archive::Close() {
    std::lock_guard<std::mutex> lock(_Mutex);
    
    if (!_IsOpen) return;
    
    SzArEx_Free(&_Archive, &_AllocImp);
    
    if (_OutBuffer) {
        ISzAlloc_Free(&_AllocImp, _OutBuffer);
        _OutBuffer = nullptr;
        _OutBufferSize = 0;
    }
    
    if (_LookStream.buf) {
        ISzAlloc_Free(&_AllocImp, _LookStream.buf);
        _LookStream.buf = nullptr;
    }
    
    File_Close(&_FileStream.file);
    
    _Path.clear();
    _IsOpen = false;
    _TreeBuilt = false;
    _FolderCacheBuilt = false;
    _FolderCache.clear();
    _RootNode = ArchiveNode();
    
    SEVENZIPVIEW_LOG(L"Archive closed");
}

UINT32 Archive::GetItemCount() const {
    if (!_IsOpen) return 0;
    return _Archive.NumFiles;
}

bool Archive::GetEntry(UINT32 index, ArchiveEntry& entry) const {
    if (!_IsOpen || index >= _Archive.NumFiles) return false;
    
    // Get file name
    size_t nameLen = SzArEx_GetFileNameUtf16(&_Archive, index, nullptr);
    if (nameLen > 0) {
        std::vector<UInt16> nameBuf(nameLen);
        SzArEx_GetFileNameUtf16(&_Archive, index, nameBuf.data());
        entry.FullPath = reinterpret_cast<const wchar_t*>(nameBuf.data());
        
        // Extract just the name from path
        size_t pos = entry.FullPath.find_last_of(L"\\/");
        if (pos != std::wstring::npos)
            entry.Name = entry.FullPath.substr(pos + 1);
        else
            entry.Name = entry.FullPath;
    }
    
    // Is directory?
    entry.Type = SzArEx_IsDir(&_Archive, index) ? ItemType::Folder : ItemType::File;
    
    // Size
    entry.Size = SzArEx_GetFileSize(&_Archive, index);
    
    // CRC
    if (SzBitWithVals_Check(&_Archive.CRCs, index)) {
        entry.CRC = _Archive.CRCs.Vals[index];
    }
    
    // Attributes
    if (SzBitWithVals_Check(&_Archive.Attribs, index)) {
        entry.Attributes = _Archive.Attribs.Vals[index];
    }
    
    // Modified time
    if (SzBitWithVals_Check(&_Archive.MTime, index)) {
        entry.ModifiedTime.dwLowDateTime = _Archive.MTime.Vals[index].Low;
        entry.ModifiedTime.dwHighDateTime = _Archive.MTime.Vals[index].High;
    }
    
    // Created time
    if (SzBitWithVals_Check(&_Archive.CTime, index)) {
        entry.CreatedTime.dwLowDateTime = _Archive.CTime.Vals[index].Low;
        entry.CreatedTime.dwHighDateTime = _Archive.CTime.Vals[index].High;
    }
    
    entry.ArchiveIndex = index;
    
    // Compressed size is harder to calculate accurately for solid archives
    // We'll estimate based on archive structure
    if (_Archive.db.NumFolders > 0 && entry.Type == ItemType::File) {
        UInt32 folderIndex = _Archive.FileToFolder[index];
        if (folderIndex != (UInt32)-1 && folderIndex < _Archive.db.NumFolders) {
            // Calculate pack size for folder using pack positions
            UInt32 packStreamStart = _Archive.db.FoStartPackStreamIndex[folderIndex];
            UInt32 packStreamEnd = _Archive.db.FoStartPackStreamIndex[folderIndex + 1];
            
            if (packStreamStart < packStreamEnd && packStreamEnd <= _Archive.db.NumPackStreams) {
                UInt64 packSize = _Archive.db.PackPositions[packStreamEnd] - _Archive.db.PackPositions[packStreamStart];
                
                // Estimate compressed size proportionally
                UInt64 folderUnpackSize = SzAr_GetFolderUnpackSize(&_Archive.db, folderIndex);
                if (folderUnpackSize > 0) {
                    entry.CompressedSize = (UInt64)((double)entry.Size * packSize / folderUnpackSize);
                }
            }
        }
    }
    
    return true;
}

ArchiveEntry Archive::GetEntry(const std::wstring& path) const {
    ArchiveEntry result;
    result.Type = ItemType::Unknown;
    
    if (!_IsOpen) return result;
    
    // Normalize path
    std::wstring normalizedPath = path;
    for (auto& ch : normalizedPath) {
        if (ch == L'\\') ch = L'/';
    }
    
    // Remove trailing slash
    while (!normalizedPath.empty() && normalizedPath.back() == L'/') {
        normalizedPath.pop_back();
    }
    
    // Search for entry
    for (UINT32 i = 0; i < _Archive.NumFiles; i++) {
        ArchiveEntry entry;
        if (GetEntry(i, entry)) {
            std::wstring entryPath = entry.FullPath;
            for (auto& ch : entryPath) {
                if (ch == L'\\') ch = L'/';
            }
            while (!entryPath.empty() && entryPath.back() == L'/') {
                entryPath.pop_back();
            }
            
            if (_wcsicmp(entryPath.c_str(), normalizedPath.c_str()) == 0) {
                return entry;
            }
        }
    }
    
    return result;
}

std::vector<ArchiveEntry> Archive::GetAllEntries() const {
    std::vector<ArchiveEntry> entries;
    if (!_IsOpen) return entries;
    
    entries.reserve(_Archive.NumFiles);
    
    for (UINT32 i = 0; i < _Archive.NumFiles; i++) {
        ArchiveEntry entry;
        if (GetEntry(i, entry)) {
            entries.push_back(std::move(entry));
        }
    }
    
    return entries;
}

std::vector<ArchiveEntry> Archive::GetEntriesInFolder(const std::wstring& folderPath) const {
    std::vector<ArchiveEntry> entries;
    if (!_IsOpen) return entries;
    
    std::wstring normalizedPath = folderPath;
    // Remove trailing slash
    while (!normalizedPath.empty() && 
           (normalizedPath.back() == L'/' || normalizedPath.back() == L'\\')) {
        normalizedPath.pop_back();
    }
    
    // Convert to forward slashes for comparison
    for (auto& c : normalizedPath) {
        if (c == L'\\') c = L'/';
    }
    
    // Check cache first (need to cast away const for cache access)
    Archive* mutableThis = const_cast<Archive*>(this);
    if (_FolderCacheBuilt) {
        auto it = _FolderCache.find(normalizedPath);
        if (it != _FolderCache.end()) {
            return it->second;
        }
        // Path not in cache means empty folder
        return entries;
    }
    
    // Build the cache on first access
    mutableThis->BuildFolderCache();
    
    // Return from cache
    auto it = _FolderCache.find(normalizedPath);
    if (it != _FolderCache.end()) {
        return it->second;
    }
    
    return entries;
}

void Archive::BuildFolderCache() {
    std::lock_guard<std::mutex> lock(_Mutex);
    
    if (_FolderCacheBuilt || !_IsOpen) return;
    
    SEVENZIPVIEW_LOG(L"Building folder cache for %u files", _Archive.NumFiles);
    
    // Track which folder paths we've added synthetic entries for (case-insensitive)
    std::set<std::wstring, CaseInsensitiveCompare> syntheticFolders;
    
    for (UINT32 i = 0; i < _Archive.NumFiles; i++) {
        ArchiveEntry entry;
        if (!GetEntry(i, entry)) continue;
        
        // Normalize entry path
        std::wstring entryPath = entry.FullPath;
        for (auto& c : entryPath) {
            if (c == L'\\') c = L'/';
        }
        while (!entryPath.empty() && entryPath.back() == L'/') {
            entryPath.pop_back();
        }
        
        // Find parent folder path
        std::wstring parentPath;
        size_t lastSlash = entryPath.find_last_of(L'/');
        if (lastSlash != std::wstring::npos) {
            parentPath = entryPath.substr(0, lastSlash);
            entry.Name = entryPath.substr(lastSlash + 1);
        } else {
            // Root level item
            parentPath = L"";
            entry.Name = entryPath;
        }
        
        // Add entry to its parent folder
        _FolderCache[parentPath].push_back(entry);
        
        // Create synthetic folder entries for all ancestor folders
        std::wstring ancestorPath;
        size_t pos = 0;
        while ((pos = entryPath.find(L'/', pos)) != std::wstring::npos) {
            std::wstring folderName = entryPath.substr(ancestorPath.empty() ? 0 : ancestorPath.length() + 1, 
                                                       pos - (ancestorPath.empty() ? 0 : ancestorPath.length() + 1));
            std::wstring fullFolderPath = ancestorPath.empty() ? folderName : ancestorPath + L"/" + folderName;
            
            if (syntheticFolders.find(fullFolderPath) == syntheticFolders.end()) {
                syntheticFolders.insert(fullFolderPath);
                
                // Add synthetic folder to its parent
                ArchiveEntry folderEntry;
                folderEntry.Name = folderName;
                folderEntry.FullPath = fullFolderPath;
                folderEntry.Type = ItemType::Folder;
                folderEntry.ArchiveIndex = ArchiveEntry::SYNTHETIC_FOLDER_INDEX;
                folderEntry.Attributes = FILE_ATTRIBUTE_DIRECTORY;
                
                _FolderCache[ancestorPath].push_back(folderEntry);
            }
            
            ancestorPath = fullFolderPath;
            pos++;
        }
    }
    
    // Remove duplicate entries (folders that exist both as synthetic and real)
    for (auto& pair : _FolderCache) {
        auto& vec = pair.second;
        std::set<std::wstring, CaseInsensitiveCompare> seenNames;
        vec.erase(std::remove_if(vec.begin(), vec.end(), [&seenNames](const ArchiveEntry& e) {
            if (seenNames.find(e.Name) != seenNames.end()) {
                return true;  // Remove duplicate
            }
            seenNames.insert(e.Name);
            return false;
        }), vec.end());
    }
    
    _FolderCacheBuilt = true;
    SEVENZIPVIEW_LOG(L"Folder cache built: %zu folders", _FolderCache.size());
}

// Legacy implementation kept for reference - now uses cache
/*
std::vector<ArchiveEntry> Archive::GetEntriesInFolder_Legacy(const std::wstring& folderPath) const {
    std::vector<ArchiveEntry> entries;
    if (!_IsOpen) return entries;
    
    std::wstring normalizedPath = folderPath;
    // Remove trailing slash
    while (!normalizedPath.empty() && 
           (normalizedPath.back() == L'/' || normalizedPath.back() == L'\\')) {
        normalizedPath.pop_back();
    }
    
    // Convert to forward slashes for comparison
    for (auto& c : normalizedPath) {
        if (c == L'\\') c = L'/';
    }
    
    // Use a set to track which names we've already added (case-insensitive)
    std::set<std::wstring, CaseInsensitiveCompare> addedNames;
    
    for (UINT32 i = 0; i < _Archive.NumFiles; i++) {
        ArchiveEntry entry;
        if (!GetEntry(i, entry)) continue;
        
        // Normalize entry path - remove trailing slash
        std::wstring entryPath = entry.FullPath;
        for (auto& c : entryPath) {
            if (c == L'\\') c = L'/';
        }
        while (!entryPath.empty() && entryPath.back() == L'/') {
            entryPath.pop_back();
        }
        
        // Check if entry is in the requested folder
        if (normalizedPath.empty()) {
            // Root folder - only items with no path separator (or just the name)
            size_t slashPos = entryPath.find(L'/');
            if (slashPos == std::wstring::npos) {
                // Direct child in root - could be file or explicit folder
                if (addedNames.find(entry.Name) == addedNames.end()) {
                    addedNames.insert(entry.Name);
                    entries.push_back(std::move(entry));
                }
            } else {
                // Has path separator - extract top-level folder
                std::wstring firstPart = entryPath.substr(0, slashPos);
                
                // Add as synthetic folder if not already present
                if (addedNames.find(firstPart) == addedNames.end()) {
                    addedNames.insert(firstPart);
                    ArchiveEntry folderEntry;
                    folderEntry.Name = firstPart;
                    folderEntry.FullPath = firstPart;
                    folderEntry.Type = ItemType::Folder;
                    folderEntry.ArchiveIndex = ArchiveEntry::SYNTHETIC_FOLDER_INDEX;
                    folderEntry.Attributes = FILE_ATTRIBUTE_DIRECTORY;
                    entries.push_back(std::move(folderEntry));
                }
            }
        } else {
            // Check if entry is directly inside the folder
            if (entryPath.length() > normalizedPath.length() &&
                _wcsnicmp(entryPath.c_str(), normalizedPath.c_str(), normalizedPath.length()) == 0 &&
                entryPath[normalizedPath.length()] == L'/') {
                
                // Get the remaining path after the folder
                std::wstring remaining = entryPath.substr(normalizedPath.length() + 1);
                
                // Remove any trailing slash from remaining
                while (!remaining.empty() && remaining.back() == L'/') {
                    remaining.pop_back();
                }
                if (remaining.empty()) continue;
                
                size_t nextSlash = remaining.find(L'/');
                if (nextSlash == std::wstring::npos) {
                    // Direct child - use the name from remaining
                    if (addedNames.find(remaining) == addedNames.end()) {
                        addedNames.insert(remaining);
                        entry.Name = remaining;  // Use the clean name
                        entries.push_back(std::move(entry));
                    }
                } else {
                    // It's a subfolder - add synthetic folder entry if not present
                    std::wstring subfolderName = remaining.substr(0, nextSlash);
                    
                    if (addedNames.find(subfolderName) == addedNames.end()) {
                        addedNames.insert(subfolderName);
                        ArchiveEntry folderEntry;
                        folderEntry.Name = subfolderName;
                        folderEntry.FullPath = normalizedPath + L"/" + subfolderName;
                        folderEntry.Type = ItemType::Folder;
                        folderEntry.ArchiveIndex = ArchiveEntry::SYNTHETIC_FOLDER_INDEX;
                        folderEntry.Attributes = FILE_ATTRIBUTE_DIRECTORY;
                        entries.push_back(std::move(folderEntry));
                    }
                }
            }
        }
    }
    
    return entries;
}
*/

const ArchiveNode& Archive::GetRootNode() {
    EnsureTree();
    return _RootNode;
}

void Archive::EnsureTree() {
    if (_TreeBuilt) return;
    BuildTree();
}

void Archive::BuildTree() {
    std::lock_guard<std::mutex> lock(_Mutex);
    
    if (_TreeBuilt || !_IsOpen) return;
    
    _RootNode = ArchiveNode();
    _RootNode.Entry.Name = L"";
    _RootNode.Entry.Type = ItemType::Root;
    
    for (UINT32 i = 0; i < _Archive.NumFiles; i++) {
        ArchiveEntry entry;
        if (!GetEntry(i, entry)) continue;
        
        // Parse path and create tree nodes
        std::wstring path = entry.FullPath;
        ArchiveNode* currentNode = &_RootNode;
        
        size_t start = 0;
        size_t end;
        
        while ((end = path.find_first_of(L"\\/", start)) != std::wstring::npos) {
            std::wstring part = path.substr(start, end - start);
            if (!part.empty()) {
                ArchiveNode* child = currentNode->FindChild(part);
                if (!child) {
                    ArchiveEntry folderEntry;
                    folderEntry.Name = part;
                    folderEntry.FullPath = path.substr(0, end);
                    folderEntry.Type = ItemType::Folder;
                    child = currentNode->AddChild(folderEntry);
                }
                currentNode = child;
            }
            start = end + 1;
        }
        
        // Add the file/final folder
        if (start < path.length()) {
            entry.Name = path.substr(start);
            currentNode->AddChild(entry);
        }
    }
    
    _TreeBuilt = true;
}

bool Archive::ExtractToBuffer(UINT32 index, std::vector<BYTE>& buffer) {
    SEVENZIPVIEW_LOG(L"Archive::ExtractToBuffer: index=%u _IsOpen=%d NumFiles=%u", index, _IsOpen ? 1 : 0, _Archive.NumFiles);
    
    std::lock_guard<std::mutex> lock(_Mutex);
    
    if (!_IsOpen || index >= _Archive.NumFiles) {
        SEVENZIPVIEW_LOG(L"Archive::ExtractToBuffer: FAILED - not open or index out of range");
        return false;
    }
    
    // Don't extract directories
    if (SzArEx_IsDir(&_Archive, index)) {
        SEVENZIPVIEW_LOG(L"Archive::ExtractToBuffer: FAILED - index is a directory");
        return false;
    }
    
    size_t offset = 0;
    size_t outSizeProcessed = 0;
    
    SRes res = SzArEx_Extract(
        &_Archive,
        &_LookStream.vt,
        index,
        &_BlockIndex,
        &_OutBuffer,
        &_OutBufferSize,
        &offset,
        &outSizeProcessed,
        &_AllocImp,
        &_AllocTempImp
    );
    
    if (res != SZ_OK) {
        SEVENZIPVIEW_LOG(L"ExtractToBuffer failed: index=%u error=%d", index, res);
        return false;
    }
    
    buffer.resize(outSizeProcessed);
    if (outSizeProcessed > 0) {
        memcpy(buffer.data(), _OutBuffer + offset, outSizeProcessed);
    }
    
    return true;
}

bool Archive::ExtractToFile(UINT32 index, const std::wstring& destPath) {
    SEVENZIPVIEW_LOG(L"Archive::ExtractToFile: index=%u dest='%s'", index, destPath.c_str());
    
    std::vector<BYTE> buffer;
    if (!ExtractToBuffer(index, buffer)) {
        SEVENZIPVIEW_LOG(L"Archive::ExtractToFile: ExtractToBuffer FAILED");
        return false;
    }
    
    SEVENZIPVIEW_LOG(L"Archive::ExtractToFile: buffer size=%zu", buffer.size());
    
    // Create directory if needed - using SHCreateDirectoryExW for robust nested creation
    size_t lastSlash = destPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        std::wstring dir = destPath.substr(0, lastSlash);
        DWORD attr = GetFileAttributesW(dir.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) {
            int shRes = SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
            SEVENZIPVIEW_LOG(L"Archive::ExtractToFile: SHCreateDirectoryExW('%s') = %d", dir.c_str(), shRes);
            if (shRes != ERROR_SUCCESS && shRes != ERROR_ALREADY_EXISTS && shRes != ERROR_FILE_EXISTS) {
                SEVENZIPVIEW_LOG(L"Archive::ExtractToFile: FAILED to create directory");
                return false;
            }
        }
    }
    
    // If file exists, try to delete it first (it may be from a previous extraction)
    DWORD existingAttr = GetFileAttributesW(destPath.c_str());
    if (existingAttr != INVALID_FILE_ATTRIBUTES) {
        // Remove read-only attribute if present
        if (existingAttr & FILE_ATTRIBUTE_READONLY) {
            SetFileAttributesW(destPath.c_str(), existingAttr & ~FILE_ATTRIBUTE_READONLY);
        }
        DeleteFileW(destPath.c_str());
        SEVENZIPVIEW_LOG(L"Archive::ExtractToFile: deleted existing file");
    }
    
    // Write file with retry logic
    HANDLE hFile = INVALID_HANDLE_VALUE;
    for (int retry = 0; retry < 3 && hFile == INVALID_HANDLE_VALUE; retry++) {
        if (retry > 0) Sleep(50);
        hFile = CreateFileW(
            destPath.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
    }
    
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        SEVENZIPVIEW_LOG(L"Failed to create file: %s (error=%u)", destPath.c_str(), err);
        return false;
    }
    
    DWORD written;
    BOOL success = WriteFile(hFile, buffer.data(), (DWORD)buffer.size(), &written, nullptr);
    CloseHandle(hFile);
    
    if (!success || written != buffer.size()) {
        DeleteFileW(destPath.c_str());
        return false;
    }
    
    // Set file attributes
    ArchiveEntry entry;
    if (GetEntry(index, entry) && entry.Attributes != 0) {
        SetFileAttributesW(destPath.c_str(), entry.Attributes);
    }
    
    // Set file time
    if (entry.ModifiedTime.dwLowDateTime != 0 || entry.ModifiedTime.dwHighDateTime != 0) {
        hFile = CreateFileW(destPath.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            SetFileTime(hFile, &entry.CreatedTime, nullptr, &entry.ModifiedTime);
            CloseHandle(hFile);
        }
    }
    
    return true;
}

bool Archive::ExtractToBuffer(const std::wstring& entryPath, std::vector<uint8_t>& buffer) {
    ArchiveEntry entry = GetEntry(entryPath);
    if (entry.Type == ItemType::Unknown || entry.Type == ItemType::Folder) {
        return false;
    }
    
    std::vector<BYTE> tempBuffer;
    if (!ExtractToBuffer(entry.ArchiveIndex, tempBuffer)) {
        return false;
    }
    
    buffer.assign(tempBuffer.begin(), tempBuffer.end());
    return true;
}

bool Archive::ExtractToFile(const std::wstring& entryPath, const std::wstring& destPath) {
    ArchiveEntry entry = GetEntry(entryPath);
    if (entry.Type == ItemType::Unknown || entry.Type == ItemType::Folder) {
        return false;
    }
    
    return ExtractToFile(entry.ArchiveIndex, destPath);
}

bool Archive::ExtractAll(const std::wstring& destDir,
                        std::function<void(const std::wstring&, UINT64, UINT64)> progress) {
    if (!_IsOpen) return false;
    
    UINT64 totalSize = GetTotalUncompressedSize();
    UINT64 processedSize = 0;
    
    for (UINT32 i = 0; i < _Archive.NumFiles; i++) {
        ArchiveEntry entry;
        if (!GetEntry(i, entry)) continue;
        
        if (progress) {
            progress(entry.FullPath, processedSize, totalSize);
        }
        
        if (entry.Type == ItemType::File) {
            std::wstring destPath = destDir + L"\\" + entry.FullPath;
            // Normalize path separators
            for (auto& c : destPath) {
                if (c == L'/') c = L'\\';
            }
            
            if (!ExtractToFile(i, destPath)) {
                SEVENZIPVIEW_LOG(L"Failed to extract: %s", entry.FullPath.c_str());
            }
        } else {
            // Create directory
            std::wstring dirPath = destDir + L"\\" + entry.FullPath;
            for (auto& c : dirPath) {
                if (c == L'/') c = L'\\';
            }
            CreateDirectoryW(dirPath.c_str(), nullptr);
        }
        
        processedSize += entry.Size;
    }
    
    if (progress) {
        progress(L"", totalSize, totalSize);
    }
    
    return true;
}

UINT64 Archive::GetTotalUncompressedSize() const {
    if (!_IsOpen) return 0;
    
    UINT64 total = 0;
    for (UINT32 i = 0; i < _Archive.NumFiles; i++) {
        total += SzArEx_GetFileSize(&_Archive, i);
    }
    return total;
}

UINT64 Archive::GetTotalCompressedSize() const {
    if (!_IsOpen) return 0;
    
    // Calculate total packed size
    UINT64 total = 0;
    for (UINT32 i = 0; i < _Archive.db.NumPackStreams; i++) {
        if (_Archive.db.PackPositions) {
            total += _Archive.db.PackPositions[i + 1] - _Archive.db.PackPositions[i];
        }
    }
    return total;
}

UINT32 Archive::GetFileCount() const {
    if (!_IsOpen) return 0;
    
    UINT32 count = 0;
    for (UINT32 i = 0; i < _Archive.NumFiles; i++) {
        if (!SzArEx_IsDir(&_Archive, i)) count++;
    }
    return count;
}

UINT32 Archive::GetFolderCount() const {
    if (!_IsOpen) return 0;
    
    UINT32 count = 0;
    for (UINT32 i = 0; i < _Archive.NumFiles; i++) {
        if (SzArEx_IsDir(&_Archive, i)) count++;
    }
    return count;
}

} // namespace SevenZipView
