/*
** SQLLocalDBView - Windows Explorer Shell Extension
** SQL Server LocalDB Database Wrapper Implementation (ODBC)
*/

#include "Database.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace SQLLocalDBView {

// ============================================================================
// Helpers ODBC (arquivo-local)
// ============================================================================

// Instancia automatica do LocalDB.
static const wchar_t* const LOCALDB_SERVER = L"(localdb)\\MSSQLLocalDB";

// Drivers ODBC tentados em ordem de preferencia.
// "SQL Server" (sqlsrv32.dll) e in-box no Windows: priorizado para nao exigir
// instalacao. Os drivers modernos entram como fallback caso estejam presentes.
static const wchar_t* const ODBC_DRIVERS[] = {
    L"SQL Server",
    L"ODBC Driver 18 for SQL Server",
    L"ODBC Driver 17 for SQL Server"
};

// Cache do driver que conectou com sucesso: apos a 1a conexao, as proximas usam-no
// direto, sem re-sondar drivers que falham (e gastam timeout). Persistido no registro
// (HKCU) para que ate a 1a abertura de um NOVO processo (Explorer/prevhost/reboot)
// ja use o driver conhecido.
static std::mutex g_DriverMutex;
static std::wstring g_PreferredDriver;

static const wchar_t* const DRIVER_REG_KEY = L"Software\\Tootega\\SQLLocalDBView";
static const wchar_t* const DRIVER_REG_VALUE = L"PreferredDriver";

static std::wstring ReadPreferredDriverFromRegistry() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, DRIVER_REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return std::wstring();
    wchar_t buf[128] = {};
    DWORD size = sizeof(buf), type = 0;
    LONG r = RegQueryValueExW(hKey, DRIVER_REG_VALUE, nullptr, &type, (LPBYTE)buf, &size);
    RegCloseKey(hKey);
    if (r == ERROR_SUCCESS && type == REG_SZ) return std::wstring(buf);
    return std::wstring();
}

static void WritePreferredDriverToRegistry(const std::wstring& drv) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, DRIVER_REG_KEY, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return;
    RegSetValueExW(hKey, DRIVER_REG_VALUE, 0, REG_SZ,
        (const BYTE*)drv.c_str(), (DWORD)((drv.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
}

// Nome de tipo SQL Server a partir do codigo SQL ODBC.
static std::wstring SqlTypeName(SQLSMALLINT t) {
    switch (t) {
        case SQL_INTEGER:        return L"int";
        case SQL_SMALLINT:       return L"smallint";
        case SQL_TINYINT:        return L"tinyint";
        case SQL_BIGINT:         return L"bigint";
        case SQL_BIT:            return L"bit";
        case SQL_REAL:           return L"real";
        case SQL_FLOAT:
        case SQL_DOUBLE:         return L"float";
        case SQL_DECIMAL:        return L"decimal";
        case SQL_NUMERIC:        return L"numeric";
        case SQL_CHAR:           return L"char";
        case SQL_VARCHAR:        return L"varchar";
        case SQL_LONGVARCHAR:    return L"text";
        case SQL_WCHAR:          return L"nchar";
        case SQL_WVARCHAR:       return L"nvarchar";
        case SQL_WLONGVARCHAR:   return L"ntext";
        case SQL_BINARY:         return L"binary";
        case SQL_VARBINARY:      return L"varbinary";
        case SQL_LONGVARBINARY:  return L"image";
        case SQL_TYPE_DATE:      return L"date";
        case SQL_TYPE_TIME:      return L"time";
        case SQL_TYPE_TIMESTAMP: return L"datetime";
        case SQL_GUID:           return L"uniqueidentifier";
        default:                 return L"";
    }
}

// Categoriza o tipo SQL em uma familia CellValue.
static CellValue::Type CategorizeType(SQLSMALLINT t) {
    switch (t) {
        case SQL_INTEGER: case SQL_SMALLINT: case SQL_TINYINT:
        case SQL_BIGINT:  case SQL_BIT:
            return CellValue::Type::Integer;
        case SQL_REAL: case SQL_FLOAT: case SQL_DOUBLE:
        case SQL_DECIMAL: case SQL_NUMERIC:
            return CellValue::Type::Real;
        case SQL_BINARY: case SQL_VARBINARY: case SQL_LONGVARBINARY:
            return CellValue::Type::Blob;
        default:
            return CellValue::Type::Text;
    }
}

// Le uma coluna textual (SQL_C_WCHAR) tratando valores longos. Retorna true se NULL.
static bool FetchText(SQLHSTMT h, SQLUSMALLINT col, std::wstring& out) {
    out.clear();
    SQLWCHAR buf[1024];
    SQLLEN ind = 0;
    for (;;) {
        SQLRETURN r = SQLGetData(h, col, SQL_C_WCHAR, buf, sizeof(buf), &ind);
        if (r == SQL_NO_DATA) break;
        if (!SQL_SUCCEEDED(r)) break;
        if (ind == SQL_NULL_DATA) return true;
        SQLLEN bytes;
        if (ind == SQL_NO_TOTAL || ind >= (SQLLEN)sizeof(buf))
            bytes = (SQLLEN)sizeof(buf) - (SQLLEN)sizeof(SQLWCHAR);
        else
            bytes = ind;
        out.append(reinterpret_cast<wchar_t*>(buf), bytes / sizeof(SQLWCHAR));
        if (r == SQL_SUCCESS) break;  // ultimo bloco
    }
    return false;
}

// Le uma coluna binaria (SQL_C_BINARY). Retorna true se NULL.
static bool FetchBlob(SQLHSTMT h, SQLUSMALLINT col, std::vector<uint8_t>& out) {
    out.clear();
    uint8_t buf[4096];
    SQLLEN ind = 0;
    for (;;) {
        SQLRETURN r = SQLGetData(h, col, SQL_C_BINARY, buf, sizeof(buf), &ind);
        if (r == SQL_NO_DATA) break;
        if (!SQL_SUCCEEDED(r)) break;
        if (ind == SQL_NULL_DATA) return true;
        SQLLEN bytes;
        if (ind == SQL_NO_TOTAL || ind > (SQLLEN)sizeof(buf))
            bytes = (SQLLEN)sizeof(buf);
        else
            bytes = ind;
        out.insert(out.end(), buf, buf + bytes);
        if (r == SQL_SUCCESS) break;
    }
    return false;
}

// Le uma coluna uniqueidentifier (SQL_C_GUID) e formata como string. Retorna true se NULL.
static bool FetchGuid(SQLHSTMT h, SQLUSMALLINT col, std::wstring& out) {
    out.clear();
    SQLGUID g = {};
    SQLLEN ind = 0;
    SQLRETURN r = SQLGetData(h, col, SQL_C_GUID, &g, sizeof(g), &ind);
    if (!SQL_SUCCEEDED(r) || ind == SQL_NULL_DATA) return true;
    wchar_t buf[40];
    StringCchPrintfW(buf, 40,
        L"%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        (unsigned long)g.Data1, (unsigned)g.Data2, (unsigned)g.Data3,
        g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
        g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    out = buf;
    return false;
}

// Monta um literal OBJECT_ID(N'schema.name') com aspas escapadas.
static std::wstring ObjectIdLiteral(const std::wstring& display) {
    std::wstring q = display;
    if (q.find(L'.') == std::wstring::npos) q = L"dbo." + q;
    std::wstring esc;
    for (wchar_t c : q) {
        if (c == L'\'') esc += L"''";
        else esc += c;
    }
    return L"N'" + esc + L"'";
}

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
            if (blobValue.empty()) return L"(empty)";
            std::wostringstream oss;
            oss << L"(binary " << blobValue.size() << L" bytes)";
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
        case Type::Blob: return L"BINARY";
        default: return L"UNKNOWN";
    }
}

// ============================================================================
// Database Implementation
// ============================================================================

Database::Database() {
}

Database::~Database() {
    Close();
}

Database::Database(Database&& other) noexcept
    : henv_(other.henv_)
    , hdbc_(other.hdbc_)
    , path_(std::move(other.path_))
    , databaseName_(std::move(other.databaseName_))
    , lastError_(std::move(other.lastError_))
    , lastErrorCode_(other.lastErrorCode_) {
    other.henv_ = nullptr;
    other.hdbc_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        Close();
        henv_ = other.henv_;
        hdbc_ = other.hdbc_;
        path_ = std::move(other.path_);
        databaseName_ = std::move(other.databaseName_);
        lastError_ = std::move(other.lastError_);
        lastErrorCode_ = other.lastErrorCode_;
        other.henv_ = nullptr;
        other.hdbc_ = nullptr;
    }
    return *this;
}

void Database::SetError(SQLSMALLINT handleType, SQLHANDLE handle, const wchar_t* context) {
    SQLWCHAR state[6] = {};
    SQLINTEGER native = 0;
    SQLWCHAR msg[1024] = {};
    SQLSMALLINT len = 0;
    if (SQL_SUCCEEDED(SQLGetDiagRecW(handleType, handle, 1, state, &native, msg, 1024, &len))) {
        lastErrorCode_ = native;
        lastError_ = StringUtil::Format(L"%s: [%s] %s", context,
            reinterpret_cast<wchar_t*>(state), reinterpret_cast<wchar_t*>(msg));
    } else {
        lastError_ = StringUtil::Format(L"%s: unknown ODBC error", context);
    }
    SQLLOCALDB_LOG(L"Database error - %s", lastError_.c_str());
}

void Database::ClearError() {
    lastErrorCode_ = 0;
    lastError_.clear();
}

bool Database::Open(const std::wstring& path, bool /*readOnly*/) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    Close();
    ClearError();
    path_ = path;

    // Alocar ambiente ODBC
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv_))) {
        lastError_ = L"Failed to allocate ODBC environment";
        return false;
    }
    SQLSetEnvAttr(henv_, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, henv_, &hdbc_))) {
        SetError(SQL_HANDLE_ENV, henv_, L"AllocConnection");
        SQLFreeHandle(SQL_HANDLE_ENV, henv_);
        henv_ = nullptr;
        hdbc_ = nullptr;
        return false;
    }

    // Timeout curto: se um driver nao alcanca a LocalDB, falha rapido e passa ao proximo.
    SQLSetConnectAttrW(hdbc_, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

    // Montar lista de drivers a tentar: o driver ja conhecido (cache) primeiro.
    std::vector<std::wstring> drivers;
    {
        std::lock_guard<std::mutex> lock(g_DriverMutex);
        // 1a vez no processo: carregar do registro (persistido entre sessoes).
        if (g_PreferredDriver.empty())
            g_PreferredDriver = ReadPreferredDriverFromRegistry();
        if (!g_PreferredDriver.empty()) drivers.push_back(g_PreferredDriver);
    }
    for (const wchar_t* d : ODBC_DRIVERS) {
        if (drivers.empty() || drivers[0] != d) drivers.push_back(d);
    }

    // Tentar cada driver.
    bool connected = false;
    for (const std::wstring& drv : drivers) {
        std::wstring conn = StringUtil::Format(
            L"Driver={%s};Server=%s;AttachDbFilename=%s;Trusted_Connection=Yes;"
            L"Encrypt=No;TrustServerCertificate=Yes;Application Name=SQLLocalDBView;",
            drv.c_str(), LOCALDB_SERVER, path.c_str());

        SQLWCHAR outConn[2048] = {};
        SQLSMALLINT outLen = 0;
        SQLRETURN r = SQLDriverConnectW(
            hdbc_, nullptr,
            reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(conn.c_str())), SQL_NTS,
            outConn, 2048, &outLen, SQL_DRIVER_NOPROMPT);

        if (SQL_SUCCEEDED(r)) {
            connected = true;
            SQLLOCALDB_LOG(L"Connected to LocalDB using driver '%s'", drv.c_str());
            std::lock_guard<std::mutex> lock(g_DriverMutex);
            if (g_PreferredDriver != drv) {
                g_PreferredDriver = drv;             // memoria
                WritePreferredDriverToRegistry(drv); // persiste (so quando muda)
            }
            break;
        }
        SetError(SQL_HANDLE_DBC, hdbc_, L"Connect");
    }

    if (!connected) {
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc_);
        SQLFreeHandle(SQL_HANDLE_ENV, henv_);
        hdbc_ = nullptr;
        henv_ = nullptr;
        return false;
    }

    // Obter o nome logico da base anexada.
    QueryResult r = RunQuery(L"SELECT DB_NAME()", 1, false);
    if (r.error.empty() && !r.rows.empty() && !r.rows[0].empty())
        databaseName_ = r.rows[0][0].ToString();

    ClearError();
    return true;
}

void Database::Close() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (hdbc_) {
        // Detach da base anexada para LIBERAR o .mdf/.ldf. Sem isso a LocalDB mantem
        // o arquivo anexado (e travado) apos o disconnect. Roda em master e engole
        // erros (ex.: outra conexao ainda usa a base -> sera detachada quando ela sair).
        if (!databaseName_.empty()) {
            std::wstring esc;
            for (wchar_t c : databaseName_) {
                if (c == L'\'') esc += L"''";
                else esc += c;
            }
            std::wstring sql =
                L"USE [master]; BEGIN TRY EXEC sp_detach_db @dbname = N'" + esc +
                L"', @skipchecks = 'true'; END TRY BEGIN CATCH END CATCH;";

            SQLHANDLE hstmt = nullptr;
            if (SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, hdbc_, &hstmt))) {
                SQLExecDirectW(hstmt,
                    reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(sql.c_str())), SQL_NTS);
                SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
            }
        }

        SQLDisconnect(hdbc_);
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc_);
        hdbc_ = nullptr;
    }
    if (henv_) {
        SQLFreeHandle(SQL_HANDLE_ENV, henv_);
        henv_ = nullptr;
    }
    path_.clear();
    databaseName_.clear();
}

// Executa uma query e coleta resultados. countTotal reservado.
QueryResult Database::RunQuery(const std::wstring& sql, int maxRows, bool /*countTotal*/) {
    QueryResult result;

    if (!hdbc_) {
        result.error = L"Database not open";
        return result;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    SQLHANDLE hstmt = nullptr;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, hdbc_, &hstmt))) {
        SetError(SQL_HANDLE_DBC, hdbc_, L"AllocStmt");
        result.error = lastError_;
        return result;
    }

    SQLRETURN r = SQLExecDirectW(hstmt,
        reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(sql.c_str())), SQL_NTS);

    if (!SQL_SUCCEEDED(r) && r != SQL_NO_DATA) {
        SetError(SQL_HANDLE_STMT, hstmt, L"Execute");
        result.error = lastError_;
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return result;
    }

    SQLSMALLINT columnCount = 0;
    SQLNumResultCols(hstmt, &columnCount);

    // Metadados de coluna + familia de tipo por coluna.
    std::vector<CellValue::Type> colCat(columnCount, CellValue::Type::Text);
    std::vector<SQLSMALLINT> colType(columnCount, 0);
    for (SQLSMALLINT i = 1; i <= columnCount; i++) {
        SQLWCHAR name[256] = {};
        SQLSMALLINT nameLen = 0, dataType = 0, decimalDigits = 0, nullable = 0;
        SQLULEN colSize = 0;
        SQLDescribeColW(hstmt, i, name, 256, &nameLen, &dataType, &colSize, &decimalDigits, &nullable);
        result.columnNames.push_back(nameLen > 0 ? std::wstring(reinterpret_cast<wchar_t*>(name), nameLen) : L"?");
        result.columnTypes.push_back(SqlTypeName(dataType));
        colCat[i - 1] = CategorizeType(dataType);
        colType[i - 1] = dataType;
    }

    // Coletar linhas.
    int rowCount = 0;
    while ((r = SQLFetch(hstmt)) == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO) {
        if (maxRows > 0 && rowCount >= maxRows) {
            result.truncated = true;
            break;
        }

        std::vector<CellValue> row;
        row.reserve(columnCount);

        for (SQLSMALLINT i = 1; i <= columnCount; i++) {
            CellValue cell;
            switch (colCat[i - 1]) {
                case CellValue::Type::Integer: {
                    SQLBIGINT v = 0;
                    SQLLEN ind = 0;
                    if (SQL_SUCCEEDED(SQLGetData(hstmt, i, SQL_C_SBIGINT, &v, sizeof(v), &ind)) &&
                        ind != SQL_NULL_DATA) {
                        cell.type = CellValue::Type::Integer;
                        cell.intValue = (int64_t)v;
                    }
                    break;
                }
                case CellValue::Type::Real: {
                    double v = 0;
                    SQLLEN ind = 0;
                    if (SQL_SUCCEEDED(SQLGetData(hstmt, i, SQL_C_DOUBLE, &v, sizeof(v), &ind)) &&
                        ind != SQL_NULL_DATA) {
                        cell.type = CellValue::Type::Real;
                        cell.realValue = v;
                    }
                    break;
                }
                case CellValue::Type::Blob: {
                    std::vector<uint8_t> data;
                    if (!FetchBlob(hstmt, i, data)) {
                        cell.type = CellValue::Type::Blob;
                        cell.blobValue = std::move(data);
                    }
                    break;
                }
                default: {
                    std::wstring text;
                    // uniqueidentifier: ler como SQL_C_GUID (driver legado nao converte
                    // bem para WCHAR) e formatar. Demais tipos: texto Unicode.
                    bool isNull = (colType[i - 1] == SQL_GUID)
                        ? FetchGuid(hstmt, i, text)
                        : FetchText(hstmt, i, text);
                    if (!isNull) {
                        cell.type = CellValue::Type::Text;
                        cell.textValue = std::move(text);
                    }
                    break;
                }
            }
            row.push_back(std::move(cell));
        }

        result.rows.push_back(std::move(row));
        rowCount++;
    }

    result.totalRows = rowCount;

    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return result;
}

QueryResult Database::ExecuteQuery(const std::wstring& sql, int maxRows) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return RunQuery(sql, maxRows, false);
}

DatabaseInfo Database::GetDatabaseInfo() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    DatabaseInfo info;
    info.path = path_;
    info.databaseName = databaseName_;

    size_t pos = path_.find_last_of(L"\\/");
    info.fileName = (pos != std::wstring::npos) ? path_.substr(pos + 1) : path_;

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExW(path_.c_str(), GetFileExInfoStandard, &fileInfo)) {
        info.fileSize = (static_cast<int64_t>(fileInfo.nFileSizeHigh) << 32) | fileInfo.nFileSizeLow;
    }

    if (!IsOpen()) return info;

    auto scalar = [&](const std::wstring& sql) -> std::wstring {
        QueryResult r = RunQuery(sql, 1, false);
        if (r.error.empty() && !r.rows.empty() && !r.rows[0].empty())
            return r.rows[0][0].ToString();
        return std::wstring();
    };

    info.encoding = scalar(L"SELECT CONVERT(nvarchar(128), DATABASEPROPERTYEX(DB_NAME(), 'Collation'))");
    info.productVersion = scalar(L"SELECT CONVERT(nvarchar(128), SERVERPROPERTY('ProductVersion'))");

    std::wstring compat = scalar(L"SELECT compatibility_level FROM sys.databases WHERE database_id = DB_ID()");
    info.compatibilityLevel = compat.empty() ? 0 : _wtoi(compat.c_str());

    std::wstring pages = scalar(L"SELECT SUM(CONVERT(bigint, size)) FROM sys.database_files");
    info.pageCount = pages.empty() ? 0 : _wtoi64(pages.c_str());

    info.tables = GetTables();
    info.views = GetViews();
    info.indexes = GetIndexes();

    return info;
}

std::vector<TableInfo> Database::GetTables() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<TableInfo> tables;
    if (!IsOpen()) return tables;

    // Uma unica query traz nome + contagem de linhas (via metadados) + numero de
    // colunas. Evita N round-trips por tabela (grande ganho de performance).
    const wchar_t* sql =
        L"SELECT s.name, t.name, "
        L"  ISNULL((SELECT SUM(p.rows) FROM sys.partitions p "
        L"          WHERE p.object_id = t.object_id AND p.index_id IN (0,1)), 0), "
        L"  (SELECT COUNT(*) FROM sys.columns c WHERE c.object_id = t.object_id) "
        L"FROM sys.tables t "
        L"INNER JOIN sys.schemas s ON t.schema_id = s.schema_id "
        L"WHERE t.is_ms_shipped = 0 ORDER BY s.name, t.name";

    QueryResult r = RunQuery(sql, 100000, false);
    for (const auto& row : r.rows) {
        if (row.size() < 4) continue;
        TableInfo table;
        table.schema = row[0].textValue;
        std::wstring tname = row[1].textValue;
        table.name = (table.schema == L"dbo") ? tname : (table.schema + L"." + tname);
        table.type = DatabaseObjectType::Table;
        table.rowCount = (row[2].type == CellValue::Type::Integer) ? row[2].intValue
                       : (int64_t)row[2].realValue;
        table.columnCount = (int)row[3].intValue;
        tables.push_back(table);
    }

    return tables;
}

std::vector<TableInfo> Database::GetViews() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<TableInfo> views;
    if (!IsOpen()) return views;

    const wchar_t* sql =
        L"SELECT s.name, v.name, "
        L"  (SELECT COUNT(*) FROM sys.columns c WHERE c.object_id = v.object_id) "
        L"FROM sys.views v "
        L"INNER JOIN sys.schemas s ON v.schema_id = s.schema_id "
        L"WHERE v.is_ms_shipped = 0 ORDER BY s.name, v.name";

    QueryResult r = RunQuery(sql, 100000, false);
    for (const auto& row : r.rows) {
        if (row.size() < 3) continue;
        TableInfo view;
        view.schema = row[0].textValue;
        std::wstring vname = row[1].textValue;
        view.name = (view.schema == L"dbo") ? vname : (view.schema + L"." + vname);
        view.type = DatabaseObjectType::View;
        view.columnCount = (int)row[2].intValue;
        view.rowCount = -1;
        views.push_back(view);
    }

    return views;
}

std::vector<IndexInfo> Database::GetIndexes() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<IndexInfo> indexes;
    if (!IsOpen()) return indexes;

    const wchar_t* sql =
        L"SELECT s.name + '.' + t.name AS tbl, i.name, i.is_unique, i.is_primary_key, i.type_desc "
        L"FROM sys.indexes i "
        L"INNER JOIN sys.tables t ON i.object_id = t.object_id "
        L"INNER JOIN sys.schemas s ON t.schema_id = s.schema_id "
        L"WHERE i.type > 0 AND i.name IS NOT NULL AND t.is_ms_shipped = 0 "
        L"ORDER BY tbl, i.name";

    QueryResult r = RunQuery(sql, 100000, false);
    for (const auto& row : r.rows) {
        if (row.size() < 5) continue;
        IndexInfo idx;
        idx.tableName = row[0].textValue;
        idx.name = row[1].textValue;
        idx.unique = (row[2].type == CellValue::Type::Integer && row[2].intValue != 0);
        idx.primaryKey = (row[3].type == CellValue::Type::Integer && row[3].intValue != 0);
        idx.type = row[4].textValue;
        indexes.push_back(idx);
    }

    return indexes;
}

std::vector<ColumnInfo> Database::GetColumnsInfo(const std::wstring& tableName) {
    std::vector<ColumnInfo> columns;
    if (!IsOpen()) return columns;

    std::wstring objLit = ObjectIdLiteral(tableName);
    std::wstring sql =
        L"SELECT c.column_id, c.name, ty.name, c.max_length, c.precision, c.scale, "
        L"c.is_nullable, c.is_identity, "
        L"CASE WHEN pk.column_id IS NOT NULL THEN 1 ELSE 0 END, "
        L"OBJECT_DEFINITION(c.default_object_id) "
        L"FROM sys.columns c "
        L"INNER JOIN sys.types ty ON c.user_type_id = ty.user_type_id "
        L"LEFT JOIN (SELECT ic.object_id, ic.column_id FROM sys.index_columns ic "
        L"  INNER JOIN sys.indexes i ON i.object_id = ic.object_id AND i.index_id = ic.index_id "
        L"  WHERE i.is_primary_key = 1) pk "
        L"  ON pk.object_id = c.object_id AND pk.column_id = c.column_id "
        L"WHERE c.object_id = OBJECT_ID(" + objLit + L") ORDER BY c.column_id";

    QueryResult r = RunQuery(sql, 100000, false);
    for (const auto& row : r.rows) {
        if (row.size() < 10) continue;
        ColumnInfo col;
        col.position = (int)row[0].intValue;
        col.name = row[1].textValue;
        col.type = row[2].textValue;
        col.maxLength = (int)row[3].intValue;
        col.precision = (int)row[4].intValue;
        col.scale = (int)row[5].intValue;
        col.notNull = !(row[6].type == CellValue::Type::Integer && row[6].intValue != 0);
        col.autoIncrement = (row[7].type == CellValue::Type::Integer && row[7].intValue != 0);
        col.primaryKey = (row[8].type == CellValue::Type::Integer && row[8].intValue != 0);
        col.defaultValue = row[9].textValue;

        // Tipo declarado com tamanho.
        std::wstring t = col.type;
        if (t == L"nvarchar" || t == L"nchar") {
            int chars = (col.maxLength < 0) ? -1 : col.maxLength / 2;
            col.declaredType = t + L"(" + (chars < 0 ? L"max" : std::to_wstring(chars)) + L")";
        } else if (t == L"varchar" || t == L"char" || t == L"varbinary" || t == L"binary") {
            col.declaredType = t + L"(" + (col.maxLength < 0 ? L"max" : std::to_wstring(col.maxLength)) + L")";
        } else if (t == L"decimal" || t == L"numeric") {
            col.declaredType = t + L"(" + std::to_wstring(col.precision) + L"," + std::to_wstring(col.scale) + L")";
        } else {
            col.declaredType = t;
        }

        columns.push_back(col);
    }

    return columns;
}

std::vector<IndexInfo> Database::GetTableIndexes(const std::wstring& tableName) {
    std::vector<IndexInfo> indexes;
    if (!IsOpen()) return indexes;

    std::wstring objLit = ObjectIdLiteral(tableName);
    std::wstring sql =
        L"SELECT i.index_id, i.name, i.is_unique, i.is_primary_key, i.type_desc "
        L"FROM sys.indexes i WHERE i.object_id = OBJECT_ID(" + objLit + L") "
        L"AND i.type > 0 AND i.name IS NOT NULL ORDER BY i.name";

    QueryResult r = RunQuery(sql, 100000, false);
    for (const auto& row : r.rows) {
        if (row.size() < 5) continue;
        IndexInfo idx;
        idx.tableName = tableName;
        int64_t indexId = row[0].intValue;
        idx.name = row[1].textValue;
        idx.unique = (row[2].type == CellValue::Type::Integer && row[2].intValue != 0);
        idx.primaryKey = (row[3].type == CellValue::Type::Integer && row[3].intValue != 0);
        idx.type = row[4].textValue;

        std::wstring colSql =
            L"SELECT c.name FROM sys.index_columns ic "
            L"INNER JOIN sys.columns c ON c.object_id = ic.object_id AND c.column_id = ic.column_id "
            L"WHERE ic.object_id = OBJECT_ID(" + objLit + L") AND ic.index_id = " +
            std::to_wstring(indexId) + L" ORDER BY ic.key_ordinal";
        QueryResult cr = RunQuery(colSql, 100000, false);
        for (const auto& crow : cr.rows)
            if (!crow.empty()) idx.columns.push_back(crow[0].textValue);

        indexes.push_back(idx);
    }

    return indexes;
}

std::vector<ForeignKeyInfo> Database::GetTableForeignKeys(const std::wstring& tableName) {
    std::vector<ForeignKeyInfo> fks;
    if (!IsOpen()) return fks;

    std::wstring objLit = ObjectIdLiteral(tableName);
    std::wstring sql =
        L"SELECT fk.name, pc.name, SCHEMA_NAME(rt.schema_id) + '.' + rt.name, rc.name "
        L"FROM sys.foreign_keys fk "
        L"INNER JOIN sys.foreign_key_columns fkc ON fkc.constraint_object_id = fk.object_id "
        L"INNER JOIN sys.columns pc ON pc.object_id = fkc.parent_object_id AND pc.column_id = fkc.parent_column_id "
        L"INNER JOIN sys.tables rt ON rt.object_id = fkc.referenced_object_id "
        L"INNER JOIN sys.columns rc ON rc.object_id = fkc.referenced_object_id AND rc.column_id = fkc.referenced_column_id "
        L"WHERE fk.parent_object_id = OBJECT_ID(" + objLit + L") ORDER BY fk.name";

    QueryResult r = RunQuery(sql, 100000, false);
    for (const auto& row : r.rows) {
        if (row.size() < 4) continue;
        ForeignKeyInfo fk;
        fk.name = row[0].textValue;
        fk.column = row[1].textValue;
        fk.referencedTable = row[2].textValue;
        fk.referencedColumn = row[3].textValue;
        fks.push_back(fk);
    }

    return fks;
}

TableInfo Database::GetTableInfo(const std::wstring& tableName) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    TableInfo table;
    table.name = tableName;
    size_t dot = tableName.find(L'.');
    table.schema = (dot == std::wstring::npos) ? L"dbo" : tableName.substr(0, dot);

    if (!IsOpen()) return table;

    // Determinar se e view.
    std::wstring objLit = ObjectIdLiteral(tableName);
    QueryResult tr = RunQuery(
        L"SELECT type FROM sys.objects WHERE object_id = OBJECT_ID(" + objLit + L")", 1, false);
    if (!tr.rows.empty() && !tr.rows[0].empty()) {
        std::wstring t = tr.rows[0][0].textValue;
        // 'V ' = view, 'U ' = user table
        if (!t.empty() && (t[0] == L'V' || t[0] == L'v'))
            table.type = DatabaseObjectType::View;
    }

    table.columns = GetColumnsInfo(tableName);
    table.indexes = GetTableIndexes(tableName);
    table.foreignKeys = GetTableForeignKeys(tableName);
    table.rowCount = GetRowCount(tableName);
    table.createSql = GetCreateStatement(tableName);

    return table;
}

int64_t Database::GetRowCount(const std::wstring& tableName) {
    if (!IsOpen()) return -1;

    // Contagem rapida via metadados (sem varrer a tabela).
    std::wstring objLit = ObjectIdLiteral(tableName);
    std::wstring sql =
        L"SELECT SUM(p.rows) FROM sys.partitions p "
        L"WHERE p.object_id = OBJECT_ID(" + objLit + L") AND p.index_id IN (0, 1)";

    QueryResult r = RunQuery(sql, 1, false);
    if (r.error.empty() && !r.rows.empty() && !r.rows[0].empty()) {
        const auto& cell = r.rows[0][0];
        if (cell.type == CellValue::Type::Integer) return cell.intValue;
        if (cell.type == CellValue::Type::Real) return (int64_t)cell.realValue;
    }
    return -1;
}

QueryResult Database::GetTableData(const std::wstring& tableName, int offset, int limit) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::wstring q = Qualify(tableName);
    // offset==0: usar TOP (compativel com qualquer compatibility level).
    // OFFSET/FETCH exige compat >= 110 (SQL Server 2012) e falharia em bases antigas.
    std::wstring sql = (offset <= 0)
        ? StringUtil::Format(L"SELECT TOP (%d) * FROM %s", limit, q.c_str())
        : StringUtil::Format(
            L"SELECT * FROM %s ORDER BY (SELECT NULL) OFFSET %d ROWS FETCH NEXT %d ROWS ONLY",
            q.c_str(), offset, limit);
    return RunQuery(sql, limit, false);
}

QueryResult Database::GetTablePreview(const std::wstring& tableName, int maxRows) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::wstring q = Qualify(tableName);
    // maxRows <= 0 significa "todas as linhas" (SQL Server nao aceita TOP negativo).
    std::wstring sql = (maxRows > 0)
        ? StringUtil::Format(L"SELECT TOP (%d) * FROM %s", maxRows, q.c_str())
        : (L"SELECT * FROM " + q);
    return RunQuery(sql, maxRows, false);
}

std::wstring Database::BuildCreateStatement(const TableInfo& table) {
    std::wostringstream sql;
    sql << L"CREATE TABLE " << StringUtil::QualifyBracketed(table.name) << L" (\n";

    // Ha uma constraint de PK a emitir?
    const IndexInfo* pk = nullptr;
    for (const auto& idx : table.indexes)
        if (idx.primaryKey) { pk = &idx; break; }

    for (size_t i = 0; i < table.columns.size(); i++) {
        const auto& col = table.columns[i];
        sql << L"    " << StringUtil::BracketQuote(col.name) << L" " << col.declaredType;
        if (col.autoIncrement) sql << L" IDENTITY(1,1)";
        sql << (col.notNull ? L" NOT NULL" : L" NULL");
        if (!col.defaultValue.empty()) sql << L" DEFAULT " << col.defaultValue;
        if (i + 1 < table.columns.size() || pk) sql << L",";
        sql << L"\n";
    }

    // Chave primaria.
    if (pk) {
        const IndexInfo& idx = *pk;
        sql << L"    CONSTRAINT " << StringUtil::BracketQuote(idx.name) << L" PRIMARY KEY (";
        for (size_t i = 0; i < idx.columns.size(); i++) {
            if (i > 0) sql << L", ";
            sql << StringUtil::BracketQuote(idx.columns[i]);
        }
        sql << L")\n";
    }

    sql << L");";
    return sql.str();
}

std::wstring Database::GetCreateStatement(const std::wstring& objectName) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!IsOpen()) return L"";

    // Views/procedures possuem definicao armazenada.
    std::wstring objLit = ObjectIdLiteral(objectName);
    QueryResult r = RunQuery(
        L"SELECT OBJECT_DEFINITION(OBJECT_ID(" + objLit + L"))", 1, false);
    if (r.error.empty() && !r.rows.empty() && !r.rows[0].empty()) {
        const auto& cell = r.rows[0][0];
        if (cell.type == CellValue::Type::Text && !cell.textValue.empty())
            return cell.textValue;
    }

    // Tabela: reconstruir a partir dos metadados.
    TableInfo table;
    table.name = objectName;
    table.columns = GetColumnsInfo(objectName);
    table.indexes = GetTableIndexes(objectName);
    if (table.columns.empty()) return L"";
    return BuildCreateStatement(table);
}

std::wstring Database::GetTableSchema(const std::wstring& tableName) {
    TableInfo table = GetTableInfo(tableName);

    std::wostringstream schema;
    schema << L"Table: " << tableName << L"\n";
    schema << L"Columns: " << table.columns.size() << L"\n";
    schema << L"Rows: " << table.rowCount << L"\n\n";

    schema << L"#    Name                 Type              Null   PK   Default\n";
    schema << L"------------------------------------------------------------------------\n";

    for (const auto& col : table.columns) {
        schema << StringUtil::Format(
            L"%-4d %-20.20s %-17.17s %-6s %-4s %-.20s\n",
            col.position,
            col.name.c_str(),
            col.declaredType.c_str(),
            col.notNull ? L"NO" : L"YES",
            col.primaryKey ? L"YES" : L"",
            col.defaultValue.empty() ? L"" : col.defaultValue.c_str());
    }

    if (!table.indexes.empty()) {
        schema << L"\nIndexes:\n";
        for (const auto& idx : table.indexes) {
            schema << L"  - " << idx.name;
            if (idx.primaryKey) schema << L" (PK)";
            else if (idx.unique) schema << L" (UNIQUE)";
            schema << L" " << idx.type << L": ";
            for (size_t i = 0; i < idx.columns.size(); i++) {
                if (i > 0) schema << L", ";
                schema << idx.columns[i];
            }
            schema << L"\n";
        }
    }

    if (!table.foreignKeys.empty()) {
        schema << L"\nForeign Keys:\n";
        for (const auto& fk : table.foreignKeys) {
            schema << L"  - " << fk.column << L" -> " << fk.referencedTable
                   << L"(" << fk.referencedColumn << L")\n";
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

DatabasePool::~DatabasePool() {
    Clear();
}

std::shared_ptr<Database> DatabasePool::GetDatabase(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(path);
    if (it != cache_.end()) {
        if (it->second.db && it->second.db->IsOpen()) {
            it->second.lastUse = std::chrono::steady_clock::now();
            return it->second.db;
        }
        cache_.erase(it);
    }

    auto db = std::make_shared<Database>();
    if (!db->Open(path, true))
        return nullptr;

    cache_[path] = Entry{ db, std::chrono::steady_clock::now() };
    EnsureTimer();
    return db;
}

void DatabasePool::EnsureTimer() {
    // mutex_ ja detido pelo chamador.
    if (timer_) return;
    if (!timerQueue_) timerQueue_ = CreateTimerQueue();
    if (timerQueue_)
        CreateTimerQueueTimer(&timer_, timerQueue_, SweepThunk, this,
            3000, 3000, WT_EXECUTEDEFAULT);
}

void CALLBACK DatabasePool::SweepThunk(PVOID param, BOOLEAN) {
    reinterpret_cast<DatabasePool*>(param)->Sweep();
}

void DatabasePool::Sweep() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    for (auto it = cache_.begin(); it != cache_.end(); ) {
        // use_count()==1 => somente o pool referencia => ninguem navegando o .mdf.
        // Idle curto evita detach se o usuario voltar logo em seguida.
        bool idle = (now - it->second.lastUse) > std::chrono::seconds(5);
        if (it->second.db && it->second.db.use_count() == 1 && idle) {
            it->second.db->Close();   // detach (libera mdf/ldf) + disconnect
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void DatabasePool::ReleaseDatabase(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(path);
    if (it != cache_.end()) {
        if (it->second.db) it->second.db->Close();
        cache_.erase(it);
    }
}

void DatabasePool::Clear() {
    // Destruir a fila de timers de forma NAO-BLOQUEANTE (CompletionEvent=NULL): evita
    // esperar callbacks e possivel deadlock com o loader lock ao descarregar a DLL.
    // A pool e um singleton estatico, entao 'this' permanece valido para callbacks tardias.
    HANDLE tq;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tq = timerQueue_;
        timerQueue_ = nullptr;
        timer_ = nullptr;
    }
    if (tq) DeleteTimerQueueEx(tq, nullptr);

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : cache_)
        if (kv.second.db) kv.second.db->Close();
    cache_.clear();
}

} // namespace SQLLocalDBView
