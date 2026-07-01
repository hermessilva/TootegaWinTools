#pragma once
/*
** SQLiteView - Windows Explorer Shell Extension
** SQLite Database Wrapper
*/

#ifndef SQLITEVIEW_DATABASE_H
#define SQLITEVIEW_DATABASE_H

#include "Common.h"
#include <optional>

namespace SQLiteView {

// Tipos de objetos no banco de dados
enum class DatabaseObjectType {
    Table,
    View,
    Index,
    Trigger,
    VirtualTable
};

// Informações de uma coluna
struct ColumnInfo {
    std::wstring name;
    std::wstring type;
    std::wstring declaredType;
    bool notNull = false;
    bool primaryKey = false;
    bool autoIncrement = false;
    std::wstring defaultValue;
    int position = 0;
};

// Informações de um índice
struct IndexInfo {
    std::wstring name;
    std::wstring tableName;
    bool unique = false;
    std::vector<std::wstring> columns;
    std::wstring sql;
};

// Informações de uma tabela
struct TableInfo {
    std::wstring name;
    DatabaseObjectType type = DatabaseObjectType::Table;
    std::vector<ColumnInfo> columns;
    std::vector<IndexInfo> indexes;
    int64_t rowCount = -1;
    std::wstring createSql;
    bool withoutRowId = false;
    bool strict = false;
};

// Valor de célula (pode ser de vários tipos)
struct CellValue {
    enum class Type { Null, Integer, Real, Text, Blob };
    Type type = Type::Null;
    
    int64_t intValue = 0;
    double realValue = 0.0;
    std::wstring textValue;
    std::vector<uint8_t> blobValue;

    bool IsNull() const { return type == Type::Null; }
    std::wstring ToString() const;
    std::wstring GetTypeString() const;
};

// Resultado de uma query
struct QueryResult {
    std::vector<std::wstring> columnNames;
    std::vector<std::wstring> columnTypes;
    std::vector<std::vector<CellValue>> rows;
    int64_t totalRows = 0;
    bool truncated = false;
    std::wstring error;
    double executionTimeMs = 0.0;
};

// Informações do banco de dados
struct DatabaseInfo {
    std::wstring path;
    std::wstring fileName;
    int64_t fileSize = 0;
    int64_t pageSize = 0;
    int64_t pageCount = 0;
    std::wstring encoding;
    int schemaVersion = 0;
    int userVersion = 0;
    bool isEncrypted = false;
    bool isWAL = false;
    std::vector<TableInfo> tables;
    std::vector<TableInfo> views;
    std::vector<IndexInfo> indexes;
    std::vector<TableInfo> triggers;
};

// Wrapper para conexão SQLite
class Database {
public:
    Database();
    ~Database();

    // Não copiável
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Movível
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    // Abrir/fechar
    bool Open(const std::wstring& path, bool readOnly = true);
    void Close();
    bool IsOpen() const { return db_ != nullptr; }

    // Informações do banco
    DatabaseInfo GetDatabaseInfo();
    std::vector<TableInfo> GetTables();
    std::vector<TableInfo> GetViews();
    std::vector<IndexInfo> GetIndexes();
    TableInfo GetTableInfo(const std::wstring& tableName);
    int64_t GetRowCount(const std::wstring& tableName);

    // Executar queries
    QueryResult ExecuteQuery(const std::wstring& sql, int maxRows = 1000);
    QueryResult GetTableData(const std::wstring& tableName, int offset = 0, int limit = 100);
    QueryResult GetTablePreview(const std::wstring& tableName, int maxRows = 50);

    // Schema
    std::wstring GetCreateStatement(const std::wstring& objectName);
    std::wstring GetTableSchema(const std::wstring& tableName);

    // Erro
    std::wstring GetLastError() const { return lastError_; }
    int GetLastErrorCode() const { return lastErrorCode_; }

    // Acesso direto (use com cuidado)
    sqlite3* GetHandle() { return db_; }

private:
    sqlite3* db_ = nullptr;
    std::wstring path_;
    std::wstring lastError_;
    int lastErrorCode_ = SQLITE_OK;
    mutable std::mutex mutex_;

    void SetError(int code);
    void ClearError();
    
    std::vector<ColumnInfo> GetColumnsInfo(const std::wstring& tableName);
    std::vector<IndexInfo> GetTableIndexes(const std::wstring& tableName);
};

// Pool de conexões para melhor performance
class DatabasePool {
public:
    static DatabasePool& Instance();

    std::shared_ptr<Database> GetDatabase(const std::wstring& path);
    void ReleaseDatabase(const std::wstring& path);
    void Clear();

private:
    DatabasePool() = default;
    std::mutex mutex_;
    std::unordered_map<std::wstring, std::weak_ptr<Database>> cache_;
};

} // namespace SQLiteView

#endif // SQLITEVIEW_DATABASE_H
