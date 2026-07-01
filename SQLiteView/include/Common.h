#pragma once
/*
** SQLiteView - Windows Explorer Shell Extension
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
**
** Common definitions and includes
*/

#ifndef SQLITEVIEW_COMMON_H
#define SQLITEVIEW_COMMON_H

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

// C++ Standard Library
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

// SQLite
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

// Item types no namespace virtual
enum class ItemType {
    Root,           // A própria pasta do DB
    Table,          // Tabela (navegável como pasta)
    View,           // View (navegável como pasta)
    Index,          // Índice
    SystemTable,    // Tabela de sistema (sqlite_*)
    Row,            // Registro/linha de uma tabela (mostrado como arquivo)
};

// Version info
#define SQLITEVIEW_VERSION_MAJOR 1
#define SQLITEVIEW_VERSION_MINOR 0
#define SQLITEVIEW_VERSION_PATCH 0
#define SQLITEVIEW_VERSION_STRING L"1.0.0"

// Registry keys
#define SQLITEVIEW_PROGID L"SQLiteView.Database"
#define SQLITEVIEW_CLSID_STR L"{E8A3C5B7-9D2F-4A1E-8B6C-3D5E7F9A2C4D}"
#define SQLITEVIEW_PREVIEW_CLSID_STR L"{F9B4D6C8-AE3F-5B2F-9C7D-4E6F8A1B3D5E}"
#define SQLITEVIEW_PROPERTY_CLSID_STR L"{A1C5E7F9-BF4A-6C3D-AE8F-5B7D9A2C4E6F}"
#define SQLITEVIEW_CONTEXTMENU_CLSID_STR L"{B2D6F8A1-CF5B-7D4E-BF9A-6C8E1B3D5F7A}"

// {E8A3C5B7-9D2F-4A1E-8B6C-3D5E7F9A2C4D}
// CLSID_SQLiteViewFolder - definido em DllMain.cpp com INITGUID
extern "C" const GUID CLSID_SQLiteViewFolder;

// {F9B4D6C8-AE3F-5B2F-9C7D-4E6F8A1B3D5E}
// CLSID_SQLiteViewPreview - definido em DllMain.cpp com INITGUID
extern "C" const GUID CLSID_SQLiteViewPreview;

// {A1C5E7F9-BF4A-6C3D-AE8F-5B7D9A2C4E6F}
// CLSID_SQLiteViewProperty - definido em DllMain.cpp com INITGUID
extern "C" const GUID CLSID_SQLiteViewProperty;

// {B2D6F8A1-CF5B-7D4E-BF9A-6C8E1B3D5F7A}
// CLSID_SQLiteViewContextMenu - definido em DllMain.cpp com INITGUID
extern "C" const GUID CLSID_SQLiteViewContextMenu;

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
}

// Debug logging - enabled for troubleshooting
// Logs to both OutputDebugString and a file in %TEMP%
namespace DebugLog {
    inline void WriteToFile(const wchar_t* message) {
        // Get temp path
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        wchar_t logPath[MAX_PATH];
        StringCchPrintfW(logPath, MAX_PATH, L"%sSQLiteView_Debug.log", tempPath);
        
        // Open file for append
        HANDLE hFile = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            // Get timestamp
            SYSTEMTIME st;
            GetLocalTime(&st);
            wchar_t timestamp[64];
            StringCchPrintfW(timestamp, 64, L"[%02d:%02d:%02d.%03d] ", 
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
            
            // Write timestamp and message
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
        va_list args;
        va_start(args, format);
        int size = _vscwprintf(format, args) + 1;
        std::wstring message(size, 0);
        vswprintf_s(&message[0], size, format, args);
        va_end(args);
        message.resize(wcslen(message.c_str()));
        
        std::wstring fullMsg = L"[SQLiteView] " + message;
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

#define SQLITEVIEW_LOG(msg, ...) DebugLog::Log(msg, ##__VA_ARGS__)
#define SQLITEVIEW_MSGBOX(title, msg, ...) DebugLog::ShowMessageBox(title, msg, ##__VA_ARGS__)

#endif // SQLITEVIEW_COMMON_H
