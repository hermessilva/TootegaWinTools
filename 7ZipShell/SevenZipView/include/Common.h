#pragma once
/*
** SevenZipView - Windows Explorer Shell Extension for 7z Archives
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
**
** Common definitions and includes
*/

#ifndef SEVENZIPVIEW_COMMON_H
#define SEVENZIPVIEW_COMMON_H

// =============================================================================
// TootegaWinLib - Shared library includes
// Provides: XComPtr, Utf8ToWide, WideToUtf8, FormatFileSize, FormatCompressionRatio
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

// C++ Standard Library (additional)
#include <map>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <optional>
#include <sstream>
#include <iomanip>

// 7-Zip SDK C API
extern "C" {
#include "7z.h"
#include "7zAlloc.h"
#include "7zBuf.h"
#include "7zCrc.h"
#include "7zFile.h"
}

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
    Root,           // The archive itself
    Folder,         // Directory inside archive
    File,           // File inside archive
};

// Version info
#define SEVENZIPVIEW_VERSION_MAJOR 1
#define SEVENZIPVIEW_VERSION_MINOR 0
#define SEVENZIPVIEW_VERSION_PATCH 0
#define SEVENZIPVIEW_VERSION_STRING L"1.0.0"

// Registry keys and identifiers
#define SEVENZIPVIEW_PROGID L"SevenZipView.Archive"
#define SEVENZIPVIEW_CLSID_STR L"{7A8B9C0D-1E2F-3A4B-5C6D-7E8F9A0B1C2D}"
#define SEVENZIPVIEW_PREVIEW_CLSID_STR L"{8B9C0D1E-2F3A-4B5C-6D7E-8F9A0B1C2D3E}"
#define SEVENZIPVIEW_PROPERTY_CLSID_STR L"{9C0D1E2F-3A4B-5C6D-7E8F-9A0B1C2D3E4F}"
#define SEVENZIPVIEW_CONTEXTMENU_CLSID_STR L"{0D1E2F3A-4B5C-6D7E-8F9A-0B1C2D3E4F5A}"
#define SEVENZIPVIEW_ICON_CLSID_STR L"{1E2F3A4B-5C6D-7E8F-9A0B-1C2D3E4F5A6B}"

// CLSIDs - defined in DllMain.cpp
// {7A8B9C0D-1E2F-3A4B-5C6D-7E8F9A0B1C2D}
extern "C" const GUID CLSID_SevenZipViewFolder;

// {8B9C0D1E-2F3A-4B5C-6D7E-8F9A0B1C2D3E}
extern "C" const GUID CLSID_SevenZipViewPreview;

// {9C0D1E2F-3A4B-5C6D-7E8F-9A0B1C2D3E4F}
extern "C" const GUID CLSID_SevenZipViewProperty;

// {0D1E2F3A-4B5C-6D7E-8F9A-0B1C2D3E4F5A}
extern "C" const GUID CLSID_SevenZipViewContextMenu;

// {1E2F3A4B-5C6D-7E8F-9A0B-1C2D3E4F5A6B}
extern "C" const GUID CLSID_SevenZipViewIcon;

// Global variables - defined in DllMain.cpp
extern HMODULE g_hModule;
extern LONG g_DllRefCount;

// Custom property keys for 7z properties
namespace SevenZipView {
    extern const PROPERTYKEY PKEY_7z_FileCount;
    extern const PROPERTYKEY PKEY_7z_FolderCount;
    extern const PROPERTYKEY PKEY_7z_CompressionRatio;
    extern const PROPERTYKEY PKEY_7z_IsEncrypted;
    extern const PROPERTYKEY PKEY_7z_Method;
}

// =============================================================================
// Use TootegaWinLib types - aliased for backward compatibility
// =============================================================================

// Smart COM pointer - use Tootega::Shell::XComPtr
template<typename T>
using ComPtr = Tootega::Shell::XComPtr<T>;

// String conversion wrapper functions for backward compatibility
// These wrap the TootegaWinLib static class methods as free functions
inline std::wstring Utf8ToWide(const char* utf8) {
    return Tootega::XStringConversion::Utf8ToWide(utf8);
}

inline std::string WideToUtf8(const wchar_t* wide) {
    return Tootega::XStringConversion::WideToUtf8(wide);
}

inline std::wstring FormatFileSize(UINT64 size) {
    return Tootega::XStringConversion::FormatFileSize(size);
}

inline std::wstring FormatCompressionRatio(UINT64 compressed, UINT64 original) {
    return Tootega::XStringConversion::FormatCompressionRatio(compressed, original);
}

// Debug logging - set to 1 to enable detailed logging to file
#define SEVENZIPVIEW_ENABLE_LOG 0

#if SEVENZIPVIEW_ENABLE_LOG
inline void SevenZipViewLog(const wchar_t* fmt, ...) {
    wchar_t buffer[2048];
    va_list args;
    va_start(args, fmt);
    StringCchVPrintfW(buffer, ARRAYSIZE(buffer), fmt, args);
    va_end(args);
    
    // Also output to debugger
    OutputDebugStringW(L"[SevenZipView] ");
    OutputDebugStringW(buffer);
    OutputDebugStringW(L"\n");
    
    // Write to log file
    static bool firstWrite = true;
    const wchar_t* logPath = L"D:\\Tootega\\Source\\TootegaTools\\SevenZipView.log";
    
    FILE* f = nullptr;
    _wfopen_s(&f, logPath, firstWrite ? L"w" : L"a");
    if (f) {
        // Get timestamp
        SYSTEMTIME st;
        GetLocalTime(&st);
        fwprintf(f, L"[%02d:%02d:%02d.%03d] %s\n", 
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buffer);
        fclose(f);
        firstWrite = false;
    }
}
#define SEVENZIPVIEW_LOG(fmt, ...) SevenZipViewLog(fmt, __VA_ARGS__)
#else
#define SEVENZIPVIEW_LOG(fmt, ...) ((void)0)
#endif

#endif // SEVENZIPVIEW_COMMON_H
