#pragma once
/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** SQLite Database Reader
*/

#ifndef SQLITEVIEW_DATABASE_H
#define SQLITEVIEW_DATABASE_H

#include "Common.h"
#include "DatabaseEntry.h"
#include <memory>
#include <mutex>

namespace SQLiteView {

// Forward declaration
class Database;

// Database pool for caching open databases
class DatabasePool {
public:
    static DatabasePool& Instance();
    
    std::shared_ptr<Database> GetDatabase(const std::wstring& path);
    void Remove(const std::wstring& path);
    void Clear();
    
private:
    DatabasePool() = default;
    ~DatabasePool() = default;
    DatabasePool(const DatabasePool&) = delete;
    DatabasePool& operator=(const DatabasePool&) = delete;
    
    std::recursive_mutex _Mutex;
    std::unordered_map<std::wstring, std::weak_ptr<Database>> _Databases;
};

// Main database class - wraps SQLite3
class Database : public std::enable_shared_from_this<Database> {
public:
    Database();
    ~Database();
    
    // Open a database file (read-only)
    bool Open(const std::wstring& path);
    void Close();
    bool IsOpen() const { return _DB != nullptr; }
    
    // Get database file path
    const std::wstring& GetPath() const { return _Path; }
    
    // Get SQLite version string
    std::wstring GetSQLiteVersion() const;
    
    // Database metadata
    INT64 GetPageSize() const;
    INT64 GetPageCount() const;
    std::wstring GetEncoding() const;
    INT64 GetSchemaVersion() const;
    
    // Get list of tables (including system tables if requested)
    std::vector<TableInfo> GetTables(bool includeSystem = false) const;
    
    // Get list of views
    std::vector<TableInfo> GetViews() const;
    
    // Get list of indexes
    std::vector<std::pair<std::wstring, std::wstring>> GetIndexes() const; // name, table
    
    // Get list of triggers
    std::vector<std::pair<std::wstring, std::wstring>> GetTriggers() const; // name, table
    
    // Get table info
    TableInfo GetTableInfo(const std::wstring& tableName) const;
    
    // Get column info for a table
    std::vector<ColumnInfo> GetColumns(const std::wstring& tableName) const;
    
    // Get record count for a table (cached)
    INT64 GetRecordCount(const std::wstring& tableName) const;
    
    // Get entries (tables as folders at root, records as files inside)
    std::vector<DatabaseEntry> GetEntriesInFolder(const std::wstring& folderPath) const;
    
    // Get a specific entry
    DatabaseEntry GetEntry(const std::wstring& path) const;
    
    // Get records from a table with pagination
    std::vector<DatabaseEntry> GetRecords(const std::wstring& tableName, 
                                          INT64 offset = 0, 
                                          INT64 limit = 1000) const;
    
    // Get record IDs only (lightweight for enumeration)
    std::vector<DatabaseEntry> GetRecordIDsOnly(const std::wstring& tableName, 
                                                 INT64 offset = 0, 
                                                 INT64 limit = 1000) const;
    
    // Get a single record by rowid
    DatabaseEntry GetRecordByRowID(const std::wstring& tableName, INT64 rowid) const;
    
    // Export record data to various formats
    bool ExportRecordToJSON(const std::wstring& tableName, INT64 rowid, std::wstring& output) const;
    bool ExportRecordToCSV(const std::wstring& tableName, INT64 rowid, std::wstring& output) const;
    bool ExportTableToCSV(const std::wstring& tableName, const std::wstring& destPath,
                          std::function<void(INT64, INT64)> progress = nullptr) const;
    
    // Execute custom query (for preview/export)
    bool ExecuteQuery(const std::wstring& sql, 
                      std::vector<std::wstring>& columnNames,
                      std::vector<std::vector<std::wstring>>& rows,
                      INT64 maxRows = 1000) const;
    
    // Get the CREATE statement for a table/view
    std::wstring GetCreateStatement(const std::wstring& name) const;
    
    // Database statistics
    struct Statistics {
        INT64 TableCount;
        INT64 ViewCount;
        INT64 IndexCount;
        INT64 TriggerCount;
        INT64 TotalRecords;
        INT64 FileSize;
        INT64 PageSize;
        INT64 PageCount;
        std::wstring Encoding;
    };
    
    Statistics GetStatistics() const;
    
private:
    // Internal helpers
    bool ExecuteSQL(const char* sql) const;
    std::wstring GetPragmaString(const char* pragma) const;
    INT64 GetPragmaInt(const char* pragma) const;
    
    // Cache management
    void ClearCache();
    void BuildTableCache() const;
    
    std::wstring        _Path;
    sqlite3*            _DB;
    
    mutable std::recursive_mutex  _Mutex;
    
    // Caches
    mutable std::unordered_map<std::wstring, INT64> _RecordCountCache;
    mutable std::vector<TableInfo> _TableCache;
    mutable bool _TableCacheBuilt;
    mutable FILETIME _LastModified;
};

} // namespace SQLiteView

#endif // SQLITEVIEW_DATABASE_H
