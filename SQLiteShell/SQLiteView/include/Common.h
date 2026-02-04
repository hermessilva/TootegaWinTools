#pragma once
/*
** SQLiteView - Windows Explorer Shell Extension for SQLite Databases
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
**
** Common definitions and includes
*/

#ifndef SQLITEVIEW_COMMON_H
#define SQLITEVIEW_COMMON_H

// =============================================================================
// TootegaWinLib - Shared library includes
// Provides: XComPtr, Utf8ToWide, WideToUtf8, FormatFileSize
// =============================================================================
#include <XPlatform.h>
#include <XStringConversion.h>
#include <Shell/XShell.h>

// Windows headers (additional shell-specific)
#include <windowsx.h>
#include <wingdi.h>
#include <propkey.h>
#include <propvarutil.h>
#include <thumbcache.h>
#include <commoncontrols.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <dwmapi.h>
#include <strsafe.h>

// COM headers
#include <objbase.h>
#include <olectl.h>

// C++ Standard Library
#include <map>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <optional>
#include <sstream>
#include <iomanip>
#include <variant>

// SQLite3 (amalgamation - statically linked)
#include "sqlite3.h"

// Pragmas for linking
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

// Item types in the virtual namespace
enum class ItemType : BYTE {
    Unknown,        // Unknown/invalid item
    Database,       // The database file itself (root)
    Table,          // Database table (appears as folder)
    View,           // Database view (appears as folder with different icon)
    Index,          // Database index (appears in metadata)
    Trigger,        // Database trigger (appears in metadata)
    Record,         // Single record/row (appears as file)
    Column,         // Column metadata (for header display)
    Schema,         // Schema information
    SystemTable,    // System table (sqlite_*)
};

// Column data types (SQLite affinity)
enum class ColumnType : BYTE {
    Integer,
    Real,
    Text,
    Blob,
    Null,
    Unknown
};

// Version info
#define SQLITEVIEW_VERSION_MAJOR 1
#define SQLITEVIEW_VERSION_MINOR 0
#define SQLITEVIEW_VERSION_PATCH 0
#define SQLITEVIEW_VERSION_STRING L"1.0.0"

// Registry keys and identifiers
#define SQLITEVIEW_PROGID L"SQLiteView.Database"
#define SQLITEVIEW_CLSID_STR L"{A1B2C3D4-E5F6-7A8B-9C0D-1E2F3A4B5C6D}"
#define SQLITEVIEW_PREVIEW_CLSID_STR L"{B2C3D4E5-F6A7-8B9C-0D1E-2F3A4B5C6D7E}"
#define SQLITEVIEW_PROPERTY_CLSID_STR L"{C3D4E5F6-A7B8-9C0D-1E2F-3A4B5C6D7E8F}"
#define SQLITEVIEW_CONTEXTMENU_CLSID_STR L"{D4E5F6A7-B8C9-0D1E-2F3A-4B5C6D7E8F9A}"
#define SQLITEVIEW_ICON_CLSID_STR L"{E5F6A7B8-C9D0-1E2F-3A4B-5C6D7E8F9A0B}"

// CLSIDs - defined in DllMain.cpp
// {A1B2C3D4-E5F6-7A8B-9C0D-1E2F3A4B5C6D}
extern "C" const GUID CLSID_SQLiteViewFolder;

// {B2C3D4E5-F6A7-8B9C-0D1E-2F3A4B5C6D7E}
extern "C" const GUID CLSID_SQLiteViewPreview;

// {C3D4E5F6-A7B8-9C0D-1E2F-3A4B5C6D7E8F}
extern "C" const GUID CLSID_SQLiteViewProperty;

// {D4E5F6A7-B8C9-0D1E-2F3A-4B5C6D7E8F9A}
extern "C" const GUID CLSID_SQLiteViewContextMenu;

// {E5F6A7B8-C9D0-1E2F-3A4B-5C6D7E8F9A0B}
extern "C" const GUID CLSID_SQLiteViewIcon;

// Global variables - defined in DllMain.cpp
extern HMODULE g_hModule;
extern LONG g_DllRefCount;

// Custom property keys for SQLite properties
namespace SQLiteView {
    extern const PROPERTYKEY PKEY_SQLite_TableCount;
    extern const PROPERTYKEY PKEY_SQLite_ViewCount;
    extern const PROPERTYKEY PKEY_SQLite_IndexCount;
    extern const PROPERTYKEY PKEY_SQLite_TriggerCount;
    extern const PROPERTYKEY PKEY_SQLite_RecordCount;
    extern const PROPERTYKEY PKEY_SQLite_PageSize;
    extern const PROPERTYKEY PKEY_SQLite_Encoding;
    extern const PROPERTYKEY PKEY_SQLite_SQLiteVersion;
    extern const PROPERTYKEY PKEY_SQLite_SchemaVersion;
    extern const PROPERTYKEY PKEY_SQLite_ColumnType;
    extern const PROPERTYKEY PKEY_SQLite_PrimaryKey;
    extern const PROPERTYKEY PKEY_SQLite_NotNull;
    extern const PROPERTYKEY PKEY_SQLite_DefaultValue;
}

// =============================================================================
// Use TootegaWinLib types - aliased for backward compatibility
// =============================================================================

// Smart COM pointer - use Tootega::Shell::XComPtr
template<typename T>
using ComPtr = Tootega::Shell::XComPtr<T>;

// String conversion wrapper functions for backward compatibility
inline std::wstring Utf8ToWide(const char* utf8) {
    return Tootega::XStringConversion::Utf8ToWide(utf8);
}

inline std::wstring Utf8ToWide(const std::string& utf8) {
    return Tootega::XStringConversion::Utf8ToWide(utf8);
}

inline std::string WideToUtf8(const wchar_t* wide) {
    return Tootega::XStringConversion::WideToUtf8(wide);
}

inline std::string WideToUtf8(const std::wstring& wide) {
    return Tootega::XStringConversion::WideToUtf8(wide);
}

inline std::wstring FormatFileSize(UINT64 size) {
    return Tootega::XStringConversion::FormatFileSize(size);
}

// SQLite value to string formatting
inline std::wstring FormatSQLiteValue(sqlite3_stmt* stmt, int col) {
    int type = sqlite3_column_type(stmt, col);
    switch (type) {
        case SQLITE_INTEGER:
            return std::to_wstring(sqlite3_column_int64(stmt, col));
        case SQLITE_FLOAT: {
            wchar_t buffer[64];
            swprintf_s(buffer, L"%.6g", sqlite3_column_double(stmt, col));
            return buffer;
        }
        case SQLITE_TEXT:
            return Utf8ToWide(reinterpret_cast<const char*>(sqlite3_column_text(stmt, col)));
        case SQLITE_BLOB: {
            int size = sqlite3_column_bytes(stmt, col);
            wchar_t buffer[64];
            swprintf_s(buffer, L"[BLOB %d bytes]", size);
            return buffer;
        }
        case SQLITE_NULL:
        default:
            return L"NULL";
    }
}

// Debug logging - set to 1 to enable detailed logging to file
// WARNING: Logging with file I/O can cause Explorer freezes due to thread contention
// Only enable for debugging, disable for production
#define SQLITEVIEW_ENABLE_LOG 1

// Build version - INCREMENT THIS BEFORE EACH BUILD
#define SQLITEVIEW_BUILD_VERSION 19

#if SQLITEVIEW_ENABLE_LOG

// Log file path
#define SQLITEVIEW_LOG_PATH "D:\\Tootega\\Source\\Tools\\Temp\\SQLiteView.log"

// Global mutex for thread-safe logging
inline std::mutex& GetLogMutex() {
    static std::mutex logMutex;
    return logMutex;
}

// Global flag to track if header was written
inline bool& GetHeaderWritten() {
    static bool headerWritten = false;
    return headerWritten;
}

// Simple logging function - opens file, writes, closes every time
// This is slower but guarantees no data loss on crash
inline void SQLiteViewLog(const wchar_t* fmt, ...) {
    wchar_t buffer[2048];
    va_list args;
    va_start(args, fmt);
    StringCchVPrintfW(buffer, ARRAYSIZE(buffer), fmt, args);
    va_end(args);
    
    // Output to debugger
    OutputDebugStringW(L"[SQLiteView] ");
    OutputDebugStringW(buffer);
    OutputDebugStringW(L"\n");
    
    // Thread-safe file logging - open/write/close each time
    std::lock_guard<std::mutex> lock(GetLogMutex());
    
    FILE* logFile = nullptr;
    
    // Write header on first call, then append
    if (!GetHeaderWritten()) {
        fopen_s(&logFile, SQLITEVIEW_LOG_PATH, "w");
        if (logFile) {
            fprintf(logFile, "========================================\n");
            fprintf(logFile, "SQLiteView Debug Log - Build Version %d\n", SQLITEVIEW_BUILD_VERSION);
            fprintf(logFile, "========================================\n");
            fclose(logFile);
            logFile = nullptr;
            GetHeaderWritten() = true;
        }
    }
    
    // Append log line
    fopen_s(&logFile, SQLITEVIEW_LOG_PATH, "a");
    if (logFile) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        DWORD tid = GetCurrentThreadId();
        
        char utf8Buffer[4096];
        WideCharToMultiByte(CP_UTF8, 0, buffer, -1, utf8Buffer, sizeof(utf8Buffer), nullptr, nullptr);
        
        fprintf(logFile, "[%02d:%02d:%02d.%03d] [TID:%05lu] %s\n", 
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, tid, utf8Buffer);
        fclose(logFile);
    }
}

#define SQLITEVIEW_LOG(fmt, ...) SQLiteViewLog(fmt, __VA_ARGS__)
#else
#define SQLITEVIEW_LOG(fmt, ...) ((void)0)
#endif

#endif // SQLITEVIEW_COMMON_H
