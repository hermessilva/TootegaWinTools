#pragma once
/*
** SQLLocalDBView - Windows Explorer Shell Extension
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
**
** Common definitions and includes.
** Browses Microsoft SQL Server LocalDB database files (.mdf) inside
** Windows Explorer as if they were folders, using ODBC to attach the
** file to the automatic instance (localdb)\MSSQLLocalDB.
*/

#ifndef SQLLOCALDB_COMMON_H
#define SQLLOCALDB_COMMON_H

// Windows version targeting
#ifndef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN10_19H1
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#endif

#ifndef WINVER
#define WINVER _WIN32_WINNT_WIN10
#endif

// Lean Windows headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Windows headers
#include <windows.h>
#include <windowsx.h>
#include <wingdi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <propkey.h>
#include <propvarutil.h>
#include <propsys.h>
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

// ODBC headers - connectivity to SQL Server LocalDB
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

// C++ Standard Library
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <optional>
#include <variant>

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
#pragma comment(lib, "odbc32.lib")

// Item types no namespace virtual
enum class ItemType {
    Root,           // A propria base de dados (.mdf)
    Table,          // Tabela (navegavel como pasta)
    View,           // View (navegavel como pasta)
    Index,          // Indice
    SystemTable,    // Tabela de sistema
    Row,            // Registro/linha de uma tabela (mostrado como arquivo)
};

// Version info
#define SQLLOCALDB_VERSION_MAJOR 1
#define SQLLOCALDB_VERSION_MINOR 0
#define SQLLOCALDB_VERSION_PATCH 0
#define SQLLOCALDB_VERSION_STRING L"1.0.0"

// Registry keys
#define SQLLOCALDB_PROGID L"SQLLocalDBView.Database"
#define SQLLOCALDB_CLSID_STR L"{3F1A9C2E-7B4D-4E6A-9C11-2A5E8D3F7B10}"
#define SQLLOCALDB_PREVIEW_CLSID_STR L"{3F1A9C2E-7B4D-4E6A-9C11-2A5E8D3F7B11}"
#define SQLLOCALDB_PROPERTY_CLSID_STR L"{3F1A9C2E-7B4D-4E6A-9C11-2A5E8D3F7B12}"
#define SQLLOCALDB_CONTEXTMENU_CLSID_STR L"{3F1A9C2E-7B4D-4E6A-9C11-2A5E8D3F7B13}"

// CLSID_SQLLocalDBViewFolder - definido em DllMain.cpp
extern "C" const GUID CLSID_SQLLocalDBViewFolder;

// CLSID_SQLLocalDBViewPreview - definido em DllMain.cpp
extern "C" const GUID CLSID_SQLLocalDBViewPreview;

// CLSID_SQLLocalDBViewProperty - definido em DllMain.cpp
extern "C" const GUID CLSID_SQLLocalDBViewProperty;

// CLSID_SQLLocalDBViewContextMenu - definido em DllMain.cpp
extern "C" const GUID CLSID_SQLLocalDBViewContextMenu;

// Smart COM pointer helper
template<typename T>
class ComPtr {
public:
    ComPtr() : ptr_(nullptr) {}
    ComPtr(T* p) : ptr_(p) { if (ptr_) ptr_->AddRef(); }
    ComPtr(const ComPtr& other) : ptr_(other.ptr_) { if (ptr_) ptr_->AddRef(); }
    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    ~ComPtr() { Release(); }

    ComPtr& operator=(const ComPtr& other) {
        if (this != &other) {
            Release();
            ptr_ = other.ptr_;
            if (ptr_) ptr_->AddRef();
        }
        return *this;
    }

    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            Release();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    void Release() {
        if (ptr_) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }

    T* Get() const { return ptr_; }
    T** GetAddressOf() { Release(); return &ptr_; }
    T** ReleaseAndGetAddressOf() { Release(); return &ptr_; }
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    operator T*() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }

    template<typename U>
    HRESULT QueryInterface(U** pp) {
        return ptr_ ? ptr_->QueryInterface(__uuidof(U), (void**)pp) : E_POINTER;
    }

    void Attach(T* p) { Release(); ptr_ = p; }
    T* Detach() { T* p = ptr_; ptr_ = nullptr; return p; }

private:
    T* ptr_;
};

// String conversion utilities
namespace StringUtil {
    inline std::wstring Utf8ToWide(const std::string& utf8) {
        if (utf8.empty()) return std::wstring();
        int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size);
        return result;
    }

    inline std::string WideToUtf8(const std::wstring& wide) {
        if (wide.empty()) return std::string();
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &result[0], size, nullptr, nullptr);
        return result;
    }

    inline std::wstring Format(const wchar_t* format, ...) {
        va_list args;
        va_start(args, format);
        int size = _vscwprintf(format, args) + 1;
        std::wstring result(size, 0);
        vswprintf_s(&result[0], size, format, args);
        va_end(args);
        result.resize(wcslen(result.c_str()));
        return result;
    }

    // Escapa um identificador SQL Server: [name] com ] duplicado.
    inline std::wstring BracketQuote(const std::wstring& name) {
        std::wstring out = L"[";
        for (wchar_t c : name) {
            if (c == L']') out += L"]]";
            else out += c;
        }
        out += L"]";
        return out;
    }

    // Converte "schema.name" (ou "name") em [schema].[name] (default schema dbo).
    inline std::wstring QualifyBracketed(const std::wstring& display) {
        size_t dot = display.find(L'.');
        if (dot == std::wstring::npos) {
            return L"[dbo]." + BracketQuote(display);
        }
        return BracketQuote(display.substr(0, dot)) + L"." + BracketQuote(display.substr(dot + 1));
    }
}

// Debug logging - opt-in via variavel de ambiente SQLLOCALDBVIEW_DEBUG.
// Sem ela, nao ha I/O de arquivo (o log em disco a cada chamada COM torna o
// Explorer MUITO lento). Definido uma vez por processo.
namespace DebugLog {
    inline bool IsEnabled() {
        static int enabled = -1;
        if (enabled < 0) {
            wchar_t buf[8] = {};
            DWORD n = GetEnvironmentVariableW(L"SQLLOCALDBVIEW_DEBUG", buf, 8);
            enabled = (n > 0 && buf[0] != L'0') ? 1 : 0;
        }
        return enabled == 1;
    }

    inline void WriteToFile(const wchar_t* message) {
        if (!IsEnabled()) return;
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        wchar_t logPath[MAX_PATH];
        StringCchPrintfW(logPath, MAX_PATH, L"%sSQLLocalDBView_Debug.log", tempPath);

        HANDLE hFile = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            wchar_t timestamp[64];
            StringCchPrintfW(timestamp, 64, L"[%02d:%02d:%02d.%03d] ",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

            std::string utf8Time = StringUtil::WideToUtf8(timestamp);
            std::string utf8Msg = StringUtil::WideToUtf8(message);
            utf8Msg += "\r\n";
            DWORD written;
            WriteFile(hFile, utf8Time.c_str(), (DWORD)utf8Time.size(), &written, nullptr);
            WriteFile(hFile, utf8Msg.c_str(), (DWORD)utf8Msg.size(), &written, nullptr);
            CloseHandle(hFile);
        }
    }

    inline void Log(const wchar_t* format, ...) {
        if (!IsEnabled()) return;
        va_list args;
        va_start(args, format);
        int size = _vscwprintf(format, args) + 1;
        std::wstring message(size, 0);
        vswprintf_s(&message[0], size, format, args);
        va_end(args);
        message.resize(wcslen(message.c_str()));

        std::wstring fullMsg = L"[SQLLocalDBView] " + message;
        OutputDebugStringW((fullMsg + L"\n").c_str());
        WriteToFile(fullMsg.c_str());
    }

    inline void ShowMessageBox(const wchar_t* title, const wchar_t* format, ...) {
        va_list args;
        va_start(args, format);
        int size = _vscwprintf(format, args) + 1;
        std::wstring message(size, 0);
        vswprintf_s(&message[0], size, format, args);
        va_end(args);
        message.resize(wcslen(message.c_str()));

        MessageBoxW(nullptr, message.c_str(), title, MB_OK | MB_ICONINFORMATION);
        Log(L"[MSGBOX:%s] %s", title, message.c_str());
    }
}

#define SQLLOCALDB_LOG(msg, ...) DebugLog::Log(msg, ##__VA_ARGS__)
#define SQLLOCALDB_MSGBOX(title, msg, ...) DebugLog::ShowMessageBox(title, msg, ##__VA_ARGS__)

#endif // SQLLOCALDB_COMMON_H
