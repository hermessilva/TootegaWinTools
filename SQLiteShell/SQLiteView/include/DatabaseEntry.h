#pragma once
/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** Database Entry Information
*/

#ifndef SQLITEVIEW_DATABASEENTRY_H
#define SQLITEVIEW_DATABASEENTRY_H

#include "Common.h"

namespace SQLiteView {

// Column information structure
struct ColumnInfo {
    std::wstring    Name;           // Column name
    std::wstring    Type;           // Declared type (e.g., "INTEGER", "TEXT")
    ColumnType      Affinity;       // SQLite type affinity
    bool            IsPrimaryKey;   // Part of primary key
    bool            IsNotNull;      // NOT NULL constraint
    bool            IsUnique;       // UNIQUE constraint
    bool            HasDefault;     // Has default value
    std::wstring    DefaultValue;   // Default value expression
    int             ColumnIndex;    // Zero-based index in table
    
    ColumnInfo()
        : Affinity(ColumnType::Unknown)
        , IsPrimaryKey(false)
        , IsNotNull(false)
        , IsUnique(false)
        , HasDefault(false)
        , ColumnIndex(0) {}
    
    // Parse SQLite type affinity from type string
    static ColumnType ParseAffinity(const std::wstring& typeStr) {
        std::wstring upper = typeStr;
        for (auto& c : upper) c = towupper(c);
        
        if (upper.find(L"INT") != std::wstring::npos)
            return ColumnType::Integer;
        if (upper.find(L"CHAR") != std::wstring::npos ||
            upper.find(L"CLOB") != std::wstring::npos ||
            upper.find(L"TEXT") != std::wstring::npos)
            return ColumnType::Text;
        if (upper.find(L"BLOB") != std::wstring::npos || upper.empty())
            return ColumnType::Blob;
        if (upper.find(L"REAL") != std::wstring::npos ||
            upper.find(L"FLOA") != std::wstring::npos ||
            upper.find(L"DOUB") != std::wstring::npos)
            return ColumnType::Real;
        
        return ColumnType::Unknown;
    }
    
    std::wstring GetAffinityString() const {
        switch (Affinity) {
            case ColumnType::Integer: return L"INTEGER";
            case ColumnType::Real:    return L"REAL";
            case ColumnType::Text:    return L"TEXT";
            case ColumnType::Blob:    return L"BLOB";
            case ColumnType::Null:    return L"NULL";
            default:                  return L"UNKNOWN";
        }
    }
};

// Table/View information structure  
struct TableInfo {
    std::wstring                Name;           // Table/view name
    ItemType                    Type;           // Table, View, SystemTable
    std::wstring                SQL;            // CREATE statement
    INT64                       RecordCount;    // Number of rows (cached)
    INT64                       PageCount;      // Number of pages used
    std::vector<ColumnInfo>     Columns;        // Column definitions
    std::vector<std::wstring>   Indexes;        // Index names on this table
    std::vector<std::wstring>   Triggers;       // Trigger names on this table
    bool                        IsWithoutRowid; // WITHOUT ROWID table
    
    TableInfo()
        : Type(ItemType::Table)
        , RecordCount(-1)
        , PageCount(0)
        , IsWithoutRowid(false) {}
    
    bool IsSystemTable() const {
        return Name.find(L"sqlite_") == 0;
    }
    
    bool IsView() const {
        return Type == ItemType::View;
    }
    
    // Get primary key column names
    std::vector<std::wstring> GetPrimaryKeyColumns() const {
        std::vector<std::wstring> pkCols;
        for (const auto& col : Columns) {
            if (col.IsPrimaryKey)
                pkCols.push_back(col.Name);
        }
        return pkCols;
    }
    
    // Find column by name (case-insensitive)
    const ColumnInfo* FindColumn(const std::wstring& name) const {
        for (const auto& col : Columns) {
            if (_wcsicmp(col.Name.c_str(), name.c_str()) == 0)
                return &col;
        }
        return nullptr;
    }
};

// Represents a database entry (table, view, or record)
struct DatabaseEntry {
    std::wstring    Name;               // Entry name (table name, or "Row_N")
    std::wstring    FullPath;           // Full path: "tablename" or "tablename/Row_123"
    ItemType        Type;               // Table, View, Record, etc.
    UINT64          Size;               // Estimated size in bytes
    FILETIME        ModifiedTime;       // Database modification time
    UINT32          Attributes;         // File attributes for Shell
    INT64           RowID;              // SQLite rowid (for records)
    INT64           RecordCount;        // Number of records (for tables)
    INT64           ColumnCount;        // Number of columns (for tables)
    std::wstring    TableName;          // Parent table name (for records)
    
    // For record display - primary key values for identification
    std::vector<std::pair<std::wstring, std::wstring>> PrimaryKeyValues;
    
    // Record data (column name -> value as string)
    std::map<std::wstring, std::wstring> RecordData;
    
    // Sentinel value for virtual entries
    static constexpr INT64 VIRTUAL_ROWID = -1;
    
    DatabaseEntry() 
        : Type(ItemType::Unknown)
        , Size(0)
        , Attributes(0)
        , RowID(VIRTUAL_ROWID)
        , RecordCount(0)
        , ColumnCount(0) {
        ZeroMemory(&ModifiedTime, sizeof(ModifiedTime));
    }
    
    bool IsTable() const { 
        return Type == ItemType::Table || Type == ItemType::View || Type == ItemType::SystemTable; 
    }
    
    bool IsRecord() const { 
        return Type == ItemType::Record; 
    }
    
    bool IsView() const {
        return Type == ItemType::View;
    }
    
    bool IsSystemTable() const {
        return Type == ItemType::SystemTable;
    }
    
    // Generate display name for record
    std::wstring GetDisplayName() const {
        if (Type == ItemType::Record) {
            // Use primary key values if available
            if (!PrimaryKeyValues.empty()) {
                std::wstring result;
                for (size_t i = 0; i < PrimaryKeyValues.size(); i++) {
                    if (i > 0) result += L"_";
                    result += PrimaryKeyValues[i].second;
                }
                return result;
            }
            // Fall back to rowid
            return L"Row_" + std::to_wstring(RowID);
        }
        return Name;
    }
    
    // Get parent path
    std::wstring GetParentPath() const {
        size_t pos = FullPath.find_last_of(L"/");
        if (pos == std::wstring::npos) return L"";
        return FullPath.substr(0, pos);
    }
    
    // Get preview text (first few columns concatenated)
    std::wstring GetPreviewText(size_t maxCols = 5) const {
        if (RecordData.empty()) return L"";
        
        std::wstring result;
        size_t count = 0;
        for (const auto& pair : RecordData) {
            if (count++ >= maxCols) break;
            if (!result.empty()) result += L" | ";
            result += pair.second;
        }
        return result;
    }
};

// Tree node for hierarchical representation (database -> tables -> records)
struct DatabaseNode {
    DatabaseEntry                   Entry;
    std::vector<DatabaseNode>       Children;
    DatabaseNode*                   Parent;
    
    DatabaseNode() : Parent(nullptr) {}
    
    // Find child by name
    DatabaseNode* FindChild(const std::wstring& name) {
        for (auto& child : Children) {
            if (_wcsicmp(child.Entry.Name.c_str(), name.c_str()) == 0)
                return &child;
        }
        return nullptr;
    }
    
    // Add child
    DatabaseNode* AddChild(const DatabaseEntry& entry) {
        Children.push_back({});
        auto& child = Children.back();
        child.Entry = entry;
        child.Parent = this;
        return &child;
    }
    
    // Count tables
    size_t CountTables() const {
        size_t count = Entry.IsTable() ? 1 : 0;
        for (const auto& child : Children)
            count += child.CountTables();
        return count;
    }
    
    // Count total records
    INT64 TotalRecords() const {
        INT64 total = Entry.RecordCount;
        for (const auto& child : Children)
            total += child.TotalRecords();
        return total;
    }
};

} // namespace SQLiteView

#endif // SQLITEVIEW_DATABASEENTRY_H
