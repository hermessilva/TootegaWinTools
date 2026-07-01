/*
** SQLiteView - Windows Explorer Shell Extension
** SQLite Database Wrapper Implementation
*/

#include "Database.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace SQLiteView {

// ============================================================================
// CellValue Implementation
// ============================================================================

std::wstring CellValue::ToString() const {
    switch (type) {
        case Type::Null:
            return L"NULL";
        case Type::Integer:
            return std::to_wstring(intValue);
        case Type::Real: {
            std::wostringstream oss;
            oss << std::setprecision(15) << realValue;
            return oss.str();
        }
        case Type::Text:
            return textValue;
        case Type::Blob: {
            if (blobValue.empty()) return L"(empty blob)";
            std::wostringstream oss;
            oss << L"(BLOB " << blobValue.size() << L" bytes)";
            return oss.str();
        }
        default:
            return L"";
    }
}

std::wstring CellValue::GetTypeString() const {
    switch (type) {
        case Type::Null: return L"NULL";
        case Type::Integer: return L"INTEGER";
        case Type::Real: return L"REAL";
        case Type::Text: return L"TEXT";
        case Type::Blob: return L"BLOB";
        default: return L"UNKNOWN";
    }
}

// ============================================================================
// Database Implementation
// ============================================================================

Database::Database() : db_(nullptr), lastErrorCode_(SQLITE_OK) {
}

Database::~Database() {
    Close();
}

Database::Database(Database&& other) noexcept
    : db_(other.db_)
    , path_(std::move(other.path_))
    , lastError_(std::move(other.lastError_))
    , lastErrorCode_(other.lastErrorCode_) {
    other.db_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        Close();
        db_ = other.db_;
        path_ = std::move(other.path_);
        lastError_ = std::move(other.lastError_);
        lastErrorCode_ = other.lastErrorCode_;
        other.db_ = nullptr;
    }
    return *this;
}

bool Database::Open(const std::wstring& path, bool readOnly) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (db_) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }

    ClearError();
    path_ = path;

    // Converter wide para UTF-8
    std::string utf8Path = StringUtil::WideToUtf8(path);

    int flags = readOnly ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    flags |= SQLITE_OPEN_FULLMUTEX;

    int result = sqlite3_open_v2(utf8Path.c_str(), &db_, flags, nullptr);
    
    if (result != SQLITE_OK) {
        SetError(result);
        if (db_) {
            sqlite3_close_v2(db_);
            db_ = nullptr;
        }
        return false;
    }

    // Configurar timeout para busy
    sqlite3_busy_timeout(db_, 5000);

    return true;
}

void Database::Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (db_) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
    path_.clear();
}

void Database::SetError(int code) {
    lastErrorCode_ = code;
    if (db_) {
        lastError_ = StringUtil::Utf8ToWide(sqlite3_errmsg(db_));
    } else {
        lastError_ = StringUtil::Utf8ToWide(sqlite3_errstr(code));
    }
}

void Database::ClearError() {
    lastErrorCode_ = SQLITE_OK;
    lastError_.clear();
}

DatabaseInfo Database::GetDatabaseInfo() {
    DatabaseInfo info;
    info.path = path_;
    
    // Extrair nome do arquivo
    size_t pos = path_.find_last_of(L"\\/");
    info.fileName = (pos != std::wstring::npos) ? path_.substr(pos + 1) : path_;

    // Tamanho do arquivo
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExW(path_.c_str(), GetFileExInfoStandard, &fileInfo)) {
        info.fileSize = (static_cast<int64_t>(fileInfo.nFileSizeHigh) << 32) | fileInfo.nFileSizeLow;
    }

    if (!IsOpen()) return info;

    std::lock_guard<std::mutex> lock(mutex_);

    // Page size
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "PRAGMA page_size", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            info.pageSize = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Page count
    if (sqlite3_prepare_v2(db_, "PRAGMA page_count", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            info.pageCount = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Encoding
    if (sqlite3_prepare_v2(db_, "PRAGMA encoding", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* enc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (enc) info.encoding = StringUtil::Utf8ToWide(enc);
        }
        sqlite3_finalize(stmt);
    }

    // Schema version
    if (sqlite3_prepare_v2(db_, "PRAGMA schema_version", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            info.schemaVersion = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // User version
    if (sqlite3_prepare_v2(db_, "PRAGMA user_version", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            info.userVersion = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Journal mode (WAL?)
    if (sqlite3_prepare_v2(db_, "PRAGMA journal_mode", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* mode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (mode) info.isWAL = (_stricmp(mode, "wal") == 0);
        }
        sqlite3_finalize(stmt);
    }

    // Obter tabelas, views, índices
    info.tables = GetTables();
    info.views = GetViews();
    info.indexes = GetIndexes();

    return info;
}

std::vector<TableInfo> Database::GetTables() {
    std::vector<TableInfo> tables;
    
    if (!IsOpen()) return tables;
    
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = 
        "SELECT name, sql FROM sqlite_master "
        "WHERE type='table' AND name NOT LIKE 'sqlite_%' "
        "ORDER BY name";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TableInfo table;
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* createSql = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            
            if (name) {
                table.name = StringUtil::Utf8ToWide(name);
                table.type = DatabaseObjectType::Table;
                if (createSql) table.createSql = StringUtil::Utf8ToWide(createSql);
                
                // Verificar se é virtual table
                if (table.createSql.find(L"VIRTUAL TABLE") != std::wstring::npos ||
                    table.createSql.find(L"virtual table") != std::wstring::npos) {
                    table.type = DatabaseObjectType::VirtualTable;
                }
                
                // WITHOUT ROWID
                table.withoutRowId = (table.createSql.find(L"WITHOUT ROWID") != std::wstring::npos);
                
                // STRICT
                table.strict = (table.createSql.find(L"STRICT") != std::wstring::npos);
                
                tables.push_back(table);
            }
        }
        sqlite3_finalize(stmt);
    }

    // Obter informações adicionais de cada tabela
    for (auto& table : tables) {
        table.columns = GetColumnsInfo(table.name);
        table.indexes = GetTableIndexes(table.name);
        table.rowCount = GetRowCount(table.name);
    }

    return tables;
}

std::vector<TableInfo> Database::GetViews() {
    std::vector<TableInfo> views;
    
    if (!IsOpen()) return views;
    
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = 
        "SELECT name, sql FROM sqlite_master "
        "WHERE type='view' "
        "ORDER BY name";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TableInfo view;
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* createSql = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            
            if (name) {
                view.name = StringUtil::Utf8ToWide(name);
                view.type = DatabaseObjectType::View;
                if (createSql) view.createSql = StringUtil::Utf8ToWide(createSql);
                views.push_back(view);
            }
        }
        sqlite3_finalize(stmt);
    }

    // Obter colunas de cada view
    for (auto& view : views) {
        view.columns = GetColumnsInfo(view.name);
    }

    return views;
}

std::vector<IndexInfo> Database::GetIndexes() {
    std::vector<IndexInfo> indexes;
    
    if (!IsOpen()) return indexes;
    
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = 
        "SELECT name, tbl_name, sql FROM sqlite_master "
        "WHERE type='index' AND sql IS NOT NULL "
        "ORDER BY tbl_name, name";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            IndexInfo index;
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* tableName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* indexSql = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            
            if (name && tableName) {
                index.name = StringUtil::Utf8ToWide(name);
                index.tableName = StringUtil::Utf8ToWide(tableName);
                if (indexSql) {
                    index.sql = StringUtil::Utf8ToWide(indexSql);
                    index.unique = (index.sql.find(L"UNIQUE") != std::wstring::npos);
                }
                indexes.push_back(index);
            }
        }
        sqlite3_finalize(stmt);
    }

    return indexes;
}

std::vector<ColumnInfo> Database::GetColumnsInfo(const std::wstring& tableName) {
    std::vector<ColumnInfo> columns;
    
    if (!IsOpen()) return columns;

    std::string sql = "PRAGMA table_info(" + StringUtil::WideToUtf8(tableName) + ")";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ColumnInfo col;
            col.position = sqlite3_column_int(stmt, 0);
            
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* defValue = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            
            if (name) col.name = StringUtil::Utf8ToWide(name);
            if (type) col.type = StringUtil::Utf8ToWide(type);
            if (defValue) col.defaultValue = StringUtil::Utf8ToWide(defValue);
            
            col.notNull = sqlite3_column_int(stmt, 3) != 0;
            col.primaryKey = sqlite3_column_int(stmt, 5) != 0;
            
            columns.push_back(col);
        }
        sqlite3_finalize(stmt);
    }

    return columns;
}

std::vector<IndexInfo> Database::GetTableIndexes(const std::wstring& tableName) {
    std::vector<IndexInfo> indexes;
    
    if (!IsOpen()) return indexes;

    std::string sql = "PRAGMA index_list(" + StringUtil::WideToUtf8(tableName) + ")";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            IndexInfo index;
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            
            if (name) {
                index.name = StringUtil::Utf8ToWide(name);
                index.tableName = tableName;
                index.unique = sqlite3_column_int(stmt, 2) != 0;
                
                // Obter colunas do índice
                std::string indexInfoSql = "PRAGMA index_info(" + std::string(name) + ")";
                sqlite3_stmt* indexStmt = nullptr;
                if (sqlite3_prepare_v2(db_, indexInfoSql.c_str(), -1, &indexStmt, nullptr) == SQLITE_OK) {
                    while (sqlite3_step(indexStmt) == SQLITE_ROW) {
                        const char* colName = reinterpret_cast<const char*>(sqlite3_column_text(indexStmt, 2));
                        if (colName) {
                            index.columns.push_back(StringUtil::Utf8ToWide(colName));
                        }
                    }
                    sqlite3_finalize(indexStmt);
                }
                
                indexes.push_back(index);
            }
        }
        sqlite3_finalize(stmt);
    }

    return indexes;
}

TableInfo Database::GetTableInfo(const std::wstring& tableName) {
    TableInfo table;
    table.name = tableName;
    
    if (!IsOpen()) return table;
    
    std::lock_guard<std::mutex> lock(mutex_);

    // Obter SQL de criação
    std::string sql = "SELECT sql, type FROM sqlite_master WHERE name = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        std::string utf8Name = StringUtil::WideToUtf8(tableName);
        sqlite3_bind_text(stmt, 1, utf8Name.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* createSql = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            
            if (createSql) table.createSql = StringUtil::Utf8ToWide(createSql);
            if (type) {
                if (_stricmp(type, "view") == 0) {
                    table.type = DatabaseObjectType::View;
                } else {
                    table.type = DatabaseObjectType::Table;
                }
            }
        }
        sqlite3_finalize(stmt);
    }

    table.columns = GetColumnsInfo(tableName);
    table.indexes = GetTableIndexes(tableName);
    table.rowCount = GetRowCount(tableName);

    return table;
}

int64_t Database::GetRowCount(const std::wstring& tableName) {
    if (!IsOpen()) return -1;

    std::string sql = "SELECT COUNT(*) FROM \"" + StringUtil::WideToUtf8(tableName) + "\"";
    
    sqlite3_stmt* stmt = nullptr;
    int64_t count = -1;
    
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return count;
}

QueryResult Database::ExecuteQuery(const std::wstring& sql, int maxRows) {
    QueryResult result;
    
    if (!IsOpen()) {
        result.error = L"Database not open";
        return result;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto startTime = std::chrono::high_resolution_clock::now();

    std::string utf8Sql = StringUtil::WideToUtf8(sql);
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(db_, utf8Sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        result.error = StringUtil::Utf8ToWide(sqlite3_errmsg(db_));
        return result;
    }

    // Obter nomes e tipos de colunas
    int columnCount = sqlite3_column_count(stmt);
    for (int i = 0; i < columnCount; i++) {
        const char* name = sqlite3_column_name(stmt, i);
        const char* type = sqlite3_column_decltype(stmt, i);
        
        result.columnNames.push_back(name ? StringUtil::Utf8ToWide(name) : L"?");
        result.columnTypes.push_back(type ? StringUtil::Utf8ToWide(type) : L"");
    }

    // Executar e coletar resultados
    int rowCount = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (maxRows > 0 && rowCount >= maxRows) {
            result.truncated = true;
            // Continuar para contar total
            rowCount++;
            continue;
        }

        std::vector<CellValue> row;
        row.reserve(columnCount);

        for (int i = 0; i < columnCount; i++) {
            CellValue cell;
            int type = sqlite3_column_type(stmt, i);
            
            switch (type) {
                case SQLITE_INTEGER:
                    cell.type = CellValue::Type::Integer;
                    cell.intValue = sqlite3_column_int64(stmt, i);
                    break;
                    
                case SQLITE_FLOAT:
                    cell.type = CellValue::Type::Real;
                    cell.realValue = sqlite3_column_double(stmt, i);
                    break;
                    
                case SQLITE_TEXT: {
                    cell.type = CellValue::Type::Text;
                    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                    if (text) cell.textValue = StringUtil::Utf8ToWide(text);
                    break;
                }
                    
                case SQLITE_BLOB: {
                    cell.type = CellValue::Type::Blob;
                    const void* data = sqlite3_column_blob(stmt, i);
                    int size = sqlite3_column_bytes(stmt, i);
                    if (data && size > 0) {
                        cell.blobValue.assign(
                            static_cast<const uint8_t*>(data),
                            static_cast<const uint8_t*>(data) + size
                        );
                    }
                    break;
                }
                    
                case SQLITE_NULL:
                default:
                    cell.type = CellValue::Type::Null;
                    break;
            }
            
            row.push_back(std::move(cell));
        }

        if (!result.truncated) {
            result.rows.push_back(std::move(row));
        }
        rowCount++;
    }

    result.totalRows = rowCount;
    
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        result.error = StringUtil::Utf8ToWide(sqlite3_errmsg(db_));
    }

    sqlite3_finalize(stmt);

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return result;
}

QueryResult Database::GetTableData(const std::wstring& tableName, int offset, int limit) {
    std::wstring sql = StringUtil::Format(
        L"SELECT * FROM \"%s\" LIMIT %d OFFSET %d",
        tableName.c_str(), limit, offset
    );
    return ExecuteQuery(sql, limit);
}

QueryResult Database::GetTablePreview(const std::wstring& tableName, int maxRows) {
    std::wstring sql = StringUtil::Format(
        L"SELECT * FROM \"%s\" LIMIT %d",
        tableName.c_str(), maxRows
    );
    return ExecuteQuery(sql, maxRows);
}

std::wstring Database::GetCreateStatement(const std::wstring& objectName) {
    if (!IsOpen()) return L"";

    std::lock_guard<std::mutex> lock(mutex_);

    std::string sql = "SELECT sql FROM sqlite_master WHERE name = ?";
    sqlite3_stmt* stmt = nullptr;
    std::wstring result;

    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        std::string utf8Name = StringUtil::WideToUtf8(objectName);
        sqlite3_bind_text(stmt, 1, utf8Name.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* createSql = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (createSql) result = StringUtil::Utf8ToWide(createSql);
        }
        sqlite3_finalize(stmt);
    }

    return result;
}

std::wstring Database::GetTableSchema(const std::wstring& tableName) {
    TableInfo table = GetTableInfo(tableName);
    
    std::wostringstream schema;
    schema << L"Table: " << tableName << L"\n";
    schema << L"Columns: " << table.columns.size() << L"\n";
    schema << L"Rows: " << table.rowCount << L"\n\n";
    
    schema << L"┌─────┬────────────────────┬──────────────┬──────────┬─────────┬──────────────┐\n";
    schema << L"│ #   │ Name               │ Type         │ Not Null │ PK      │ Default      │\n";
    schema << L"├─────┼────────────────────┼──────────────┼──────────┼─────────┼──────────────┤\n";
    
    for (const auto& col : table.columns) {
        schema << StringUtil::Format(
            L"│ %-3d │ %-18.18s │ %-12.12s │ %-8s │ %-7s │ %-12.12s │\n",
            col.position,
            col.name.c_str(),
            col.type.c_str(),
            col.notNull ? L"YES" : L"NO",
            col.primaryKey ? L"YES" : L"NO",
            col.defaultValue.empty() ? L"" : col.defaultValue.c_str()
        );
    }
    
    schema << L"└─────┴────────────────────┴──────────────┴──────────┴─────────┴──────────────┘\n";

    if (!table.indexes.empty()) {
        schema << L"\nIndexes:\n";
        for (const auto& idx : table.indexes) {
            schema << L"  • " << idx.name;
            if (idx.unique) schema << L" (UNIQUE)";
            schema << L": ";
            for (size_t i = 0; i < idx.columns.size(); i++) {
                if (i > 0) schema << L", ";
                schema << idx.columns[i];
            }
            schema << L"\n";
        }
    }

    return schema.str();
}

// ============================================================================
// DatabasePool Implementation
// ============================================================================

DatabasePool& DatabasePool::Instance() {
    static DatabasePool instance;
    return instance;
}

std::shared_ptr<Database> DatabasePool::GetDatabase(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Verificar se já existe no cache
    auto it = cache_.find(path);
    if (it != cache_.end()) {
        if (auto db = it->second.lock()) {
            return db;
        }
        // Expirou, remover
        cache_.erase(it);
    }

    // Criar nova conexão
    auto db = std::make_shared<Database>();
    if (db->Open(path, true)) {
        cache_[path] = db;
        return db;
    }

    return nullptr;
}

void DatabasePool::ReleaseDatabase(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(path);
}

void DatabasePool::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

} // namespace SQLiteView
