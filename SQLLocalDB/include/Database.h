#pragma once
/*
** SQLLocalDBView - Windows Explorer Shell Extension
** SQL Server LocalDB Database Wrapper (ODBC)
**
** Attaches an .mdf file to the automatic LocalDB instance
** (localdb)\MSSQLLocalDB and exposes its schema/data read-only.
*/

#ifndef SQLLOCALDB_DATABASE_H
#define SQLLOCALDB_DATABASE_H

#include "Common.h"
#include <optional>

namespace SQLLocalDBView {

// Tipos de objetos no banco de dados
enum class DatabaseObjectType {
    Table,
    View,
    Index,
    Trigger,
    SystemTable
};

// Informacoes de uma coluna
struct ColumnInfo {
    std::wstring name;
    std::wstring type;          // Nome do tipo SQL Server (int, nvarchar, ...)
    std::wstring declaredType;  // Tipo formatado com tamanho (nvarchar(50))
    bool notNull = false;
    bool primaryKey = false;
    bool autoIncrement = false; // IDENTITY
    std::wstring defaultValue;
    int position = 0;
    int maxLength = 0;          // Tamanho em bytes (-1 = MAX)
    int precision = 0;
    int scale = 0;
};

// Chave estrangeira
struct ForeignKeyInfo {
    std::wstring name;
    std::wstring column;
    std::wstring referencedTable;   // schema.table
    std::wstring referencedColumn;
};

// Informacoes de um indice
struct IndexInfo {
    std::wstring name;
    std::wstring tableName;
    bool unique = false;
    bool primaryKey = false;
    std::wstring type;          // CLUSTERED / NONCLUSTERED
    std::vector<std::wstring> columns;
    std::wstring sql;
};

// Informacoes de uma tabela ou view
struct TableInfo {
    std::wstring name;          // Nome de exibicao: schema.name (dbo omitido)
    std::wstring schema;        // Schema (dbo por padrao)
    DatabaseObjectType type = DatabaseObjectType::Table;
    std::vector<ColumnInfo> columns;
    std::vector<IndexInfo> indexes;
    std::vector<ForeignKeyInfo> foreignKeys;
    int64_t rowCount = -1;
    int columnCount = 0;        // Numero de colunas (barato, sem carregar metadados)
    std::wstring createSql;     // Reconstruido a partir dos metadados
};

// Valor de celula (pode ser de varios tipos)
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

// Informacoes do banco de dados
struct DatabaseInfo {
    std::wstring path;
    std::wstring fileName;
    std::wstring databaseName;      // Nome logico atribuido no attach
    int64_t fileSize = 0;
    int64_t pageSize = 8192;        // SQL Server usa paginas de 8 KB
    int64_t pageCount = 0;
    std::wstring encoding;          // Collation da base
    std::wstring productVersion;    // Versao do SQL Server LocalDB
    int compatibilityLevel = 0;
    int schemaVersion = 0;
    int userVersion = 0;
    bool isEncrypted = false;
    bool isWAL = false;             // Nao aplicavel; mantido por compatibilidade
    std::vector<TableInfo> tables;
    std::vector<TableInfo> views;
    std::vector<IndexInfo> indexes;
    std::vector<TableInfo> triggers;
};

// Wrapper para conexao ODBC com LocalDB
class Database {
public:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    // Abrir/fechar (readOnly ignorado: acesso sempre somente leitura)
    bool Open(const std::wstring& path, bool readOnly = true);
    void Close();
    bool IsOpen() const { return hdbc_ != nullptr; }

    // Informacoes do banco
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

private:
    SQLHANDLE henv_ = nullptr;
    SQLHANDLE hdbc_ = nullptr;
    std::wstring path_;
    std::wstring databaseName_;
    std::wstring lastError_;
    int lastErrorCode_ = 0;
    mutable std::recursive_mutex mutex_;

    void SetError(SQLSMALLINT handleType, SQLHANDLE handle, const wchar_t* context);
    void ClearError();

    // Helpers ODBC
    QueryResult RunQuery(const std::wstring& sql, int maxRows, bool countTotal);

    std::vector<ColumnInfo> GetColumnsInfo(const std::wstring& tableName);
    std::vector<IndexInfo> GetTableIndexes(const std::wstring& tableName);
    std::vector<ForeignKeyInfo> GetTableForeignKeys(const std::wstring& tableName);
    std::wstring BuildCreateStatement(const TableInfo& table);

    // Converte nome de exibicao (schema.name) em [schema].[name]
    static std::wstring Qualify(const std::wstring& display) {
        return StringUtil::QualifyBracketed(display);
    }
};

// Pool de conexoes para melhor performance
class DatabasePool {
public:
    static DatabasePool& Instance();

    std::shared_ptr<Database> GetDatabase(const std::wstring& path);
    void ReleaseDatabase(const std::wstring& path);
    void Clear();

private:
    DatabasePool() = default;
    std::mutex mutex_;
    // Referencia forte: mantem a conexao/attach vivos entre navegacoes
    // (evita re-attach lento do .mdf e conflito de "database ja anexado").
    std::unordered_map<std::wstring, std::shared_ptr<Database>> cache_;
};

} // namespace SQLLocalDBView

#endif // SQLLOCALDB_DATABASE_H
