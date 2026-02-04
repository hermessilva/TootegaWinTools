/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** SQLite Database Reader Implementation
*/

#include "Database.h"
#include <shlobj.h>

namespace SQLiteView {

// Database Pool Implementation
DatabasePool& DatabasePool::Instance() {
    static DatabasePool instance;
    return instance;
}

std::shared_ptr<Database> DatabasePool::GetDatabase(const std::wstring& path) {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    
    auto it = _Databases.find(path);
    if (it != _Databases.end()) {
        auto ptr = it->second.lock();
        if (ptr && ptr->IsOpen()) return ptr;
    }
    
    auto database = std::make_shared<Database>();
    if (database->Open(path)) {
        _Databases[path] = database;
        return database;
    }
    
    return nullptr;
}

void DatabasePool::Remove(const std::wstring& path) {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    _Databases.erase(path);
}

void DatabasePool::Clear() {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    _Databases.clear();
}

// Database Implementation
Database::Database()
    : _DB(nullptr)
    , _TableCacheBuilt(false) {
    ZeroMemory(&_LastModified, sizeof(_LastModified));
}

Database::~Database() {
    Close();
}

bool Database::Open(const std::wstring& path) {
    SQLITEVIEW_LOG(L"Database::Open acquiring lock for: %s", path.c_str());
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    SQLITEVIEW_LOG(L"Database::Open lock acquired");
    
    if (_DB) Close();
    
    SQLITEVIEW_LOG(L"Opening database: %s", path.c_str());
    
    // Convert path to UTF-8 for SQLite
    std::string utf8Path = WideToUtf8(path);
    
    // Open database in read-only mode
    int flags = SQLITE_OPEN_READONLY | SQLITE_OPEN_URI;
    int rc = sqlite3_open_v2(utf8Path.c_str(), &_DB, flags, nullptr);
    
    if (rc != SQLITE_OK) {
        SQLITEVIEW_LOG(L"  Failed to open database: error=%d", rc);
        if (_DB) {
            sqlite3_close(_DB);
            _DB = nullptr;
        }
        return false;
    }
    
    // Get file modification time
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attrs)) {
        _LastModified = attrs.ftLastWriteTime;
    }
    
    _Path = path;
    _TableCacheBuilt = false;
    
    SQLITEVIEW_LOG(L"  Database opened successfully");
    
    return true;
}

void Database::Close() {
    if (_DB) {
        sqlite3_close(_DB);
        _DB = nullptr;
    }
    
    _Path.clear();
    ClearCache();
    
    SQLITEVIEW_LOG(L"Database closed");
}

void Database::ClearCache() {
    _RecordCountCache.clear();
    _TableCache.clear();
    _TableCacheBuilt = false;
}

std::wstring Database::GetSQLiteVersion() const {
    return Utf8ToWide(sqlite3_libversion());
}

INT64 Database::GetPageSize() const {
    return GetPragmaInt("page_size");
}

INT64 Database::GetPageCount() const {
    return GetPragmaInt("page_count");
}

std::wstring Database::GetEncoding() const {
    return GetPragmaString("encoding");
}

INT64 Database::GetSchemaVersion() const {
    return GetPragmaInt("schema_version");
}

std::wstring Database::GetPragmaString(const char* pragma) const {
    if (!_DB) return L"";
    
    std::string sql = "PRAGMA ";
    sql += pragma;
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_DB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return L"";
    
    std::wstring result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (text) result = Utf8ToWide(text);
    }
    
    sqlite3_finalize(stmt);
    return result;
}

INT64 Database::GetPragmaInt(const char* pragma) const {
    if (!_DB) return 0;
    
    std::string sql = "PRAGMA ";
    sql += pragma;
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_DB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return 0;
    
    INT64 result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int64(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return result;
}

void Database::BuildTableCache() const {
    // Note: Caller should hold lock, or use recursive_mutex
    SQLITEVIEW_LOG(L"BuildTableCache: built=%d", _TableCacheBuilt ? 1 : 0);
    if (_TableCacheBuilt || !_DB) return;
    
    _TableCache.clear();
    
    // Query all tables and views from sqlite_master
    const char* sql = 
        "SELECT name, type, sql FROM sqlite_master "
        "WHERE type IN ('table', 'view') "
        "ORDER BY type DESC, name";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_DB, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TableInfo info;
        
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* sqlText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        
        if (name) info.Name = Utf8ToWide(name);
        if (sqlText) info.SQL = Utf8ToWide(sqlText);
        
        if (type && strcmp(type, "view") == 0) {
            info.Type = ItemType::View;
        } else if (info.Name.find(L"sqlite_") == 0) {
            info.Type = ItemType::SystemTable;
        } else {
            info.Type = ItemType::Table;
        }
        
        // Get columns for this table
        info.Columns = const_cast<Database*>(this)->GetColumns(info.Name);
        
        _TableCache.push_back(std::move(info));
    }
    
    sqlite3_finalize(stmt);
    _TableCacheBuilt = true;
}

std::vector<TableInfo> Database::GetTables(bool includeSystem) const {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    
    BuildTableCache();
    
    std::vector<TableInfo> result;
    for (const auto& table : _TableCache) {
        if (table.Type == ItemType::Table || 
            (includeSystem && table.Type == ItemType::SystemTable)) {
            result.push_back(table);
        }
    }
    
    return result;
}

std::vector<TableInfo> Database::GetViews() const {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    
    BuildTableCache();
    
    std::vector<TableInfo> result;
    for (const auto& table : _TableCache) {
        if (table.Type == ItemType::View) {
            result.push_back(table);
        }
    }
    
    return result;
}

std::vector<std::pair<std::wstring, std::wstring>> Database::GetIndexes() const {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    
    std::vector<std::pair<std::wstring, std::wstring>> result;
    if (!_DB) return result;
    
    const char* sql = 
        "SELECT name, tbl_name FROM sqlite_master WHERE type='index' AND sql IS NOT NULL ORDER BY name";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_DB, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* table = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        
        if (name && table) {
            result.emplace_back(Utf8ToWide(name), Utf8ToWide(table));
        }
    }
    
    sqlite3_finalize(stmt);
    return result;
}

std::vector<std::pair<std::wstring, std::wstring>> Database::GetTriggers() const {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    
    std::vector<std::pair<std::wstring, std::wstring>> result;
    if (!_DB) return result;
    
    const char* sql = 
        "SELECT name, tbl_name FROM sqlite_master WHERE type='trigger' ORDER BY name";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_DB, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* table = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        
        if (name && table) {
            result.emplace_back(Utf8ToWide(name), Utf8ToWide(table));
        }
    }
    
    sqlite3_finalize(stmt);
    return result;
}

TableInfo Database::GetTableInfo(const std::wstring& tableName) const {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    
    BuildTableCache();
    
    for (const auto& table : _TableCache) {
        if (_wcsicmp(table.Name.c_str(), tableName.c_str()) == 0) {
            return table;
        }
    }
    
    return TableInfo();
}

std::vector<ColumnInfo> Database::GetColumns(const std::wstring& tableName) const {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    
    std::vector<ColumnInfo> result;
    if (!_DB) return result;
    
    std::string sql = "PRAGMA table_info(\"" + WideToUtf8(tableName) + "\")";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_DB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return result;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ColumnInfo col;
        
        col.ColumnIndex = sqlite3_column_int(stmt, 0);
        
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name) col.Name = Utf8ToWide(name);
        
        const char* type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (type) {
            col.Type = Utf8ToWide(type);
            col.Affinity = ColumnInfo::ParseAffinity(col.Type);
        }
        
        col.IsNotNull = sqlite3_column_int(stmt, 3) != 0;
        
        const char* defVal = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (defVal) {
            col.HasDefault = true;
            col.DefaultValue = Utf8ToWide(defVal);
        }
        
        col.IsPrimaryKey = sqlite3_column_int(stmt, 5) != 0;
        
        result.push_back(std::move(col));
    }
    
    sqlite3_finalize(stmt);
    return result;
}

INT64 Database::GetRecordCount(const std::wstring& tableName) const {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    
    // Check cache
    auto it = _RecordCountCache.find(tableName);
    if (it != _RecordCountCache.end()) {
        return it->second;
    }
    
    if (!_DB) return 0;
    
    // Use COUNT(*) for accuracy
    std::string sql = "SELECT COUNT(*) FROM \"" + WideToUtf8(tableName) + "\"";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_DB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return 0;
    
    INT64 count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int64(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    
    // Cache the result
    _RecordCountCache[tableName] = count;
    
    return count;
}

std::vector<DatabaseEntry> Database::GetEntriesInFolder(const std::wstring& folderPath) const {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    SQLITEVIEW_LOG(L"GetEntriesInFolder: path='%s'", folderPath.c_str());
    
    std::vector<DatabaseEntry> entries;
    
    if (!_DB) return entries;
    
    if (folderPath.empty()) {
        // Root level - return tables and views as folders
        BuildTableCache();
        
        for (const auto& table : _TableCache) {
            // Skip system tables by default
            if (table.Type == ItemType::SystemTable) continue;
            
            DatabaseEntry entry;
            entry.Name = table.Name;
            entry.FullPath = table.Name;
            entry.Type = table.Type;
            entry.Attributes = FILE_ATTRIBUTE_DIRECTORY;
            entry.RecordCount = GetRecordCount(table.Name);
            entry.ColumnCount = static_cast<INT64>(table.Columns.size());
            entry.ModifiedTime = _LastModified;
            
            // Estimate size based on record count and column count
            entry.Size = entry.RecordCount * entry.ColumnCount * 50; // rough estimate
            
            entries.push_back(std::move(entry));
        }
    } else {
        // Inside a table - return only record IDs (lightweight)
        entries = GetRecordIDsOnly(folderPath, 0, 10000);
    }
    
    return entries;
}

DatabaseEntry Database::GetEntry(const std::wstring& path) const {
    DatabaseEntry entry;
    entry.Type = ItemType::Unknown;
    
    if (!_DB || path.empty()) return entry;
    
    // Check if it's a table
    size_t slashPos = path.find(L'/');
    if (slashPos == std::wstring::npos) {
        // It's a table name
        TableInfo info = GetTableInfo(path);
        if (!info.Name.empty()) {
            entry.Name = info.Name;
            entry.FullPath = info.Name;
            entry.Type = info.Type;
            entry.Attributes = FILE_ATTRIBUTE_DIRECTORY;
            entry.RecordCount = GetRecordCount(info.Name);
            entry.ColumnCount = static_cast<INT64>(info.Columns.size());
            entry.ModifiedTime = _LastModified;
        }
    } else {
        // It's a record: "tablename/Row_123"
        std::wstring tableName = path.substr(0, slashPos);
        std::wstring recordName = path.substr(slashPos + 1);
        
        // Parse rowid from "Row_123"
        if (recordName.find(L"Row_") == 0) {
            INT64 rowid = _wtoi64(recordName.c_str() + 4);
            entry = GetRecordByRowID(tableName, rowid);
        }
    }
    
    return entry;
}

std::vector<DatabaseEntry> Database::GetRecords(const std::wstring& tableName, 
                                                 INT64 offset, 
                                                 INT64 limit) const {
    std::vector<DatabaseEntry> entries;
    
    if (!_DB) return entries;
    
    // Get column info
    auto columns = GetColumns(tableName);
    if (columns.empty()) return entries;
    
    // Build SELECT query with rowid
    std::string sql = "SELECT rowid, * FROM \"" + WideToUtf8(tableName) + "\" LIMIT ? OFFSET ?";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_DB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return entries;
    
    sqlite3_bind_int64(stmt, 1, limit);
    sqlite3_bind_int64(stmt, 2, offset);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DatabaseEntry entry;
        
        entry.RowID = sqlite3_column_int64(stmt, 0);
        entry.TableName = tableName;
        entry.Name = L"Row_" + std::to_wstring(entry.RowID);
        entry.FullPath = tableName + L"/" + entry.Name;
        entry.Type = ItemType::Record;
        entry.Attributes = FILE_ATTRIBUTE_NORMAL;
        entry.ModifiedTime = _LastModified;
        entry.ColumnCount = static_cast<INT64>(columns.size());
        
        // Store record data (column values)
        INT64 estimatedSize = 0;
        for (size_t i = 0; i < columns.size(); i++) {
            std::wstring value = FormatSQLiteValue(stmt, static_cast<int>(i + 1));
            entry.RecordData[columns[i].Name] = value;
            estimatedSize += value.length() * 2;
            
            // Track primary key values for display name
            if (columns[i].IsPrimaryKey) {
                entry.PrimaryKeyValues.emplace_back(columns[i].Name, value);
            }
        }
        
        entry.Size = static_cast<UINT64>(estimatedSize);
        
        // Update display name based on primary key
        if (!entry.PrimaryKeyValues.empty()) {
            entry.Name = entry.GetDisplayName();
        }
        
        entries.push_back(std::move(entry));
    }
    
    sqlite3_finalize(stmt);
    return entries;
}

std::vector<DatabaseEntry> Database::GetRecordIDsOnly(const std::wstring& tableName, 
                                                       INT64 offset, 
                                                       INT64 limit) const {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    SQLITEVIEW_LOG(L"GetRecordIDsOnly: table='%s' offset=%lld limit=%lld", tableName.c_str(), offset, limit);
    
    std::vector<DatabaseEntry> entries;
    
    if (!_DB) return entries;
    
    auto columns = GetColumns(tableName);
    INT64 colCount = static_cast<INT64>(columns.size());
    
    // Only select rowid - much faster
    std::string sql = "SELECT rowid FROM \"" + WideToUtf8(tableName) + "\" LIMIT ? OFFSET ?";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_DB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return entries;
    
    sqlite3_bind_int64(stmt, 1, limit);
    sqlite3_bind_int64(stmt, 2, offset);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DatabaseEntry entry;
        
        entry.RowID = sqlite3_column_int64(stmt, 0);
        entry.TableName = tableName;
        entry.Name = L"Row_" + std::to_wstring(entry.RowID);
        entry.FullPath = tableName + L"/" + entry.Name;
        entry.Type = ItemType::Record;
        entry.Attributes = FILE_ATTRIBUTE_NORMAL;
        entry.ModifiedTime = _LastModified;
        entry.ColumnCount = colCount;
        entry.Size = 0; // Unknown until loaded
        
        entries.push_back(std::move(entry));
    }
    
    sqlite3_finalize(stmt);
    return entries;
}

DatabaseEntry Database::GetRecordByRowID(const std::wstring& tableName, INT64 rowid) const {
    SQLITEVIEW_LOG(L"GetRecordByRowID ENTER: table='%s' rowid=%lld", tableName.c_str(), rowid);
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    SQLITEVIEW_LOG(L"GetRecordByRowID: lock acquired");
    
    DatabaseEntry entry;
    entry.Type = ItemType::Unknown;
    
    if (!_DB) {
        SQLITEVIEW_LOG(L"GetRecordByRowID EXIT: no DB");
        return entry;
    }
    
    SQLITEVIEW_LOG(L"GetRecordByRowID: getting columns...");
    auto columns = GetColumns(tableName);
    SQLITEVIEW_LOG(L"GetRecordByRowID: got %zu columns", columns.size());
    if (columns.empty()) return entry;
    
    std::string sql = "SELECT rowid, * FROM \"" + WideToUtf8(tableName) + "\" WHERE rowid = ?";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_DB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return entry;
    
    sqlite3_bind_int64(stmt, 1, rowid);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        entry.RowID = sqlite3_column_int64(stmt, 0);
        entry.TableName = tableName;
        entry.Name = L"Row_" + std::to_wstring(entry.RowID);
        entry.FullPath = tableName + L"/" + entry.Name;
        entry.Type = ItemType::Record;
        entry.Attributes = FILE_ATTRIBUTE_NORMAL;
        entry.ModifiedTime = _LastModified;
        entry.ColumnCount = static_cast<INT64>(columns.size());
        
        for (size_t i = 0; i < columns.size(); i++) {
            std::wstring value = FormatSQLiteValue(stmt, static_cast<int>(i + 1));
            entry.RecordData[columns[i].Name] = value;
            
            if (columns[i].IsPrimaryKey) {
                entry.PrimaryKeyValues.emplace_back(columns[i].Name, value);
            }
        }
    }
    
    sqlite3_finalize(stmt);
    return entry;
}

bool Database::ExportRecordToJSON(const std::wstring& tableName, INT64 rowid, std::wstring& output) const {
    DatabaseEntry entry = GetRecordByRowID(tableName, rowid);
    if (entry.Type == ItemType::Unknown) return false;
    
    output = L"{\n";
    bool first = true;
    for (const auto& pair : entry.RecordData) {
        if (!first) output += L",\n";
        first = false;
        
        output += L"  \"" + pair.first + L"\": ";
        
        // Try to determine if it's a number
        bool isNumber = false;
        if (!pair.second.empty() && pair.second != L"NULL") {
            wchar_t* end = nullptr;
            wcstod(pair.second.c_str(), &end);
            isNumber = (end && *end == L'\0');
        }
        
        if (pair.second == L"NULL") {
            output += L"null";
        } else if (isNumber) {
            output += pair.second;
        } else {
            // Escape string
            std::wstring escaped;
            for (wchar_t c : pair.second) {
                switch (c) {
                    case L'"':  escaped += L"\\\""; break;
                    case L'\\': escaped += L"\\\\"; break;
                    case L'\n': escaped += L"\\n"; break;
                    case L'\r': escaped += L"\\r"; break;
                    case L'\t': escaped += L"\\t"; break;
                    default:    escaped += c; break;
                }
            }
            output += L"\"" + escaped + L"\"";
        }
    }
    output += L"\n}";
    
    return true;
}

bool Database::ExportRecordToCSV(const std::wstring& tableName, INT64 rowid, std::wstring& output) const {
    DatabaseEntry entry = GetRecordByRowID(tableName, rowid);
    if (entry.Type == ItemType::Unknown) return false;
    
    // Get columns to maintain order
    auto columns = GetColumns(tableName);
    
    // Header
    for (size_t i = 0; i < columns.size(); i++) {
        if (i > 0) output += L",";
        output += L"\"" + columns[i].Name + L"\"";
    }
    output += L"\n";
    
    // Data
    for (size_t i = 0; i < columns.size(); i++) {
        if (i > 0) output += L",";
        
        auto it = entry.RecordData.find(columns[i].Name);
        if (it != entry.RecordData.end()) {
            std::wstring value = it->second;
            // Escape quotes
            size_t pos = 0;
            while ((pos = value.find(L'"', pos)) != std::wstring::npos) {
                value.replace(pos, 1, L"\"\"");
                pos += 2;
            }
            output += L"\"" + value + L"\"";
        }
    }
    output += L"\n";
    
    return true;
}

bool Database::ExportTableToCSV(const std::wstring& tableName, const std::wstring& destPath,
                                std::function<void(INT64, INT64)> progress) const {
    if (!_DB) return false;
    
    auto columns = GetColumns(tableName);
    if (columns.empty()) return false;
    
    FILE* f = nullptr;
    if (_wfopen_s(&f, destPath.c_str(), L"w, ccs=UTF-8") != 0 || !f)
        return false;
    
    // Write BOM for UTF-8
    fputc(0xEF, f);
    fputc(0xBB, f);
    fputc(0xBF, f);
    
    // Write header
    for (size_t i = 0; i < columns.size(); i++) {
        if (i > 0) fputws(L",", f);
        fputws(L"\"", f);
        fputws(columns[i].Name.c_str(), f);
        fputws(L"\"", f);
    }
    fputws(L"\n", f);
    
    // Get total count for progress
    INT64 total = GetRecordCount(tableName);
    INT64 current = 0;
    
    // Query all records
    std::string sql = "SELECT * FROM \"" + WideToUtf8(tableName) + "\"";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_DB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        fclose(f);
        return false;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        for (int i = 0; i < sqlite3_column_count(stmt); i++) {
            if (i > 0) fputws(L",", f);
            
            std::wstring value = FormatSQLiteValue(stmt, i);
            
            // Escape quotes
            size_t pos = 0;
            while ((pos = value.find(L'"', pos)) != std::wstring::npos) {
                value.replace(pos, 1, L"\"\"");
                pos += 2;
            }
            
            fputws(L"\"", f);
            fputws(value.c_str(), f);
            fputws(L"\"", f);
        }
        fputws(L"\n", f);
        
        current++;
        if (progress && (current % 1000 == 0)) {
            progress(current, total);
        }
    }
    
    sqlite3_finalize(stmt);
    fclose(f);
    
    if (progress) progress(total, total);
    
    return true;
}

bool Database::ExecuteQuery(const std::wstring& sql,
                            std::vector<std::wstring>& columnNames,
                            std::vector<std::vector<std::wstring>>& rows,
                            INT64 maxRows) const {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    
    if (!_DB) return false;
    
    columnNames.clear();
    rows.clear();
    
    sqlite3_stmt* stmt = nullptr;
    std::string utf8Sql = WideToUtf8(sql);
    
    if (sqlite3_prepare_v2(_DB, utf8Sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    
    // Get column names
    int colCount = sqlite3_column_count(stmt);
    for (int i = 0; i < colCount; i++) {
        const char* name = sqlite3_column_name(stmt, i);
        columnNames.push_back(name ? Utf8ToWide(name) : L"");
    }
    
    // Fetch rows
    INT64 rowCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && rowCount < maxRows) {
        std::vector<std::wstring> row;
        for (int i = 0; i < colCount; i++) {
            row.push_back(FormatSQLiteValue(stmt, i));
        }
        rows.push_back(std::move(row));
        rowCount++;
    }
    
    sqlite3_finalize(stmt);
    return true;
}

std::wstring Database::GetCreateStatement(const std::wstring& name) const {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    
    if (!_DB) return L"";
    
    const char* sql = "SELECT sql FROM sqlite_master WHERE name = ?";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_DB, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return L"";
    
    std::string utf8Name = WideToUtf8(name);
    sqlite3_bind_text(stmt, 1, utf8Name.c_str(), -1, SQLITE_STATIC);
    
    std::wstring result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (text) result = Utf8ToWide(text);
    }
    
    sqlite3_finalize(stmt);
    return result;
}

Database::Statistics Database::GetStatistics() const {
    std::lock_guard<std::recursive_mutex> lock(_Mutex);
    
    Statistics stats = {};
    
    if (!_DB) return stats;
    
    // Count tables, views, indexes, triggers
    const char* countSql = 
        "SELECT "
        "(SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'),"
        "(SELECT COUNT(*) FROM sqlite_master WHERE type='view'),"
        "(SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND sql IS NOT NULL),"
        "(SELECT COUNT(*) FROM sqlite_master WHERE type='trigger')";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_DB, countSql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.TableCount = sqlite3_column_int64(stmt, 0);
            stats.ViewCount = sqlite3_column_int64(stmt, 1);
            stats.IndexCount = sqlite3_column_int64(stmt, 2);
            stats.TriggerCount = sqlite3_column_int64(stmt, 3);
        }
        sqlite3_finalize(stmt);
    }
    
    // Get pragmas
    stats.PageSize = GetPragmaInt("page_size");
    stats.PageCount = GetPragmaInt("page_count");
    stats.Encoding = GetPragmaString("encoding");
    
    // Calculate file size
    stats.FileSize = stats.PageSize * stats.PageCount;
    
    // Sum up record counts from all tables
    BuildTableCache();
    stats.TotalRecords = 0;
    for (const auto& table : _TableCache) {
        if (table.Type == ItemType::Table) {
            stats.TotalRecords += GetRecordCount(table.Name);
        }
    }
    
    return stats;
}

} // namespace SQLiteView
