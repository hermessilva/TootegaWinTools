// ============================================================================
// XStringConversion.cpp - String Encoding Conversion Implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "../Include/XStringConversion.h"
#include <strsafe.h>

namespace Tootega
{
    // ========================================================================
    // UTF-8 <-> Wide String Conversion
    // ========================================================================

    std::wstring XStringConversion::Utf8ToWide(std::string_view pUtf8) noexcept
    {
        if (pUtf8.empty())
            return {};

        int len = MultiByteToWideChar(CP_UTF8, 0, pUtf8.data(),
            static_cast<int>(pUtf8.size()), nullptr, 0);
        if (len <= 0)
            return {};

        std::wstring result(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, pUtf8.data(),
            static_cast<int>(pUtf8.size()), result.data(), len);
        return result;
    }

    std::wstring XStringConversion::Utf8ToWide(const char* pUtf8) noexcept
    {
        if (!pUtf8 || !*pUtf8)
            return {};

        int len = MultiByteToWideChar(CP_UTF8, 0, pUtf8, -1, nullptr, 0);
        if (len <= 0)
            return {};

        std::wstring result(len - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, pUtf8, -1, result.data(), len);
        return result;
    }

    std::string XStringConversion::WideToUtf8(std::wstring_view pWide) noexcept
    {
        if (pWide.empty())
            return {};

        int len = WideCharToMultiByte(CP_UTF8, 0, pWide.data(),
            static_cast<int>(pWide.size()), nullptr, 0, nullptr, nullptr);
        if (len <= 0)
            return {};

        std::string result(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, pWide.data(),
            static_cast<int>(pWide.size()), result.data(), len, nullptr, nullptr);
        return result;
    }

    std::string XStringConversion::WideToUtf8(const wchar_t* pWide) noexcept
    {
        if (!pWide || !*pWide)
            return {};

        int len = WideCharToMultiByte(CP_UTF8, 0, pWide, -1,
            nullptr, 0, nullptr, nullptr);
        if (len <= 0)
            return {};

        std::string result(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, pWide, -1,
            result.data(), len, nullptr, nullptr);
        return result;
    }

    // ========================================================================
    // ANSI <-> Wide String Conversion
    // ========================================================================

    std::wstring XStringConversion::AnsiToWide(std::string_view pAnsi) noexcept
    {
        if (pAnsi.empty())
            return {};

        int len = MultiByteToWideChar(CP_ACP, 0, pAnsi.data(),
            static_cast<int>(pAnsi.size()), nullptr, 0);
        if (len <= 0)
            return {};

        std::wstring result(len, L'\0');
        MultiByteToWideChar(CP_ACP, 0, pAnsi.data(),
            static_cast<int>(pAnsi.size()), result.data(), len);
        return result;
    }

    std::string XStringConversion::WideToAnsi(std::wstring_view pWide) noexcept
    {
        if (pWide.empty())
            return {};

        int len = WideCharToMultiByte(CP_ACP, 0, pWide.data(),
            static_cast<int>(pWide.size()), nullptr, 0, nullptr, nullptr);
        if (len <= 0)
            return {};

        std::string result(len, '\0');
        WideCharToMultiByte(CP_ACP, 0, pWide.data(),
            static_cast<int>(pWide.size()), result.data(), len, nullptr, nullptr);
        return result;
    }

    // ========================================================================
    // Size Formatting
    // ========================================================================

    std::wstring XStringConversion::FormatFileSize(UINT64 pBytes) noexcept
    {
        return FormatFileSizeEx(pBytes, 1);
    }

    std::wstring XStringConversion::FormatFileSizeEx(UINT64 pBytes, int pDecimals) noexcept
    {
        wchar_t buffer[64];

        constexpr UINT64 KB = 1024ULL;
        constexpr UINT64 MB = KB * 1024ULL;
        constexpr UINT64 GB = MB * 1024ULL;
        constexpr UINT64 TB = GB * 1024ULL;

        if (pBytes < KB)
        {
            StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%llu bytes", pBytes);
        }
        else if (pBytes < MB)
        {
            double value = static_cast<double>(pBytes) / static_cast<double>(KB);
            StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%.*f KB", pDecimals, value);
        }
        else if (pBytes < GB)
        {
            double value = static_cast<double>(pBytes) / static_cast<double>(MB);
            StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%.*f MB", pDecimals, value);
        }
        else if (pBytes < TB)
        {
            double value = static_cast<double>(pBytes) / static_cast<double>(GB);
            StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%.*f GB", pDecimals, value);
        }
        else
        {
            double value = static_cast<double>(pBytes) / static_cast<double>(TB);
            StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%.*f TB", pDecimals, value);
        }

        return buffer;
    }

    // ========================================================================
    // Compression Ratio Formatting
    // ========================================================================

    std::wstring XStringConversion::FormatCompressionRatio(UINT64 pCompressed, UINT64 pOriginal) noexcept
    {
        if (pOriginal == 0)
            return L"N/A";

        wchar_t buffer[32];
        double ratio = 100.0 * (1.0 - static_cast<double>(pCompressed) / static_cast<double>(pOriginal));
        StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%.1f%%", ratio);
        return buffer;
    }

    // ========================================================================
    // Number Formatting
    // ========================================================================

    std::wstring XStringConversion::FormatNumber(UINT64 pNumber) noexcept
    {
        // Format with thousand separators
        std::wstring numStr = std::to_wstring(pNumber);
        std::wstring result;
        result.reserve(numStr.size() + numStr.size() / 3);

        int count = 0;
        for (auto it = numStr.rbegin(); it != numStr.rend(); ++it)
        {
            if (count > 0 && count % 3 == 0)
                result.insert(result.begin(), L',');
            result.insert(result.begin(), *it);
            ++count;
        }

        return result;
    }

    std::wstring XStringConversion::FormatPercentage(double pValue, int pDecimals) noexcept
    {
        wchar_t buffer[32];
        StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%.*f%%", pDecimals, pValue);
        return buffer;
    }

    // ========================================================================
    // Time Formatting
    // ========================================================================

    std::wstring XStringConversion::FormatFileTime(const FILETIME& pFileTime) noexcept
    {
        SYSTEMTIME st;
        FileTimeToSystemTime(&pFileTime, &st);
        return FormatSystemTime(st);
    }

    std::wstring XStringConversion::FormatSystemTime(const SYSTEMTIME& pSystemTime) noexcept
    {
        wchar_t buffer[64];
        StringCchPrintfW(buffer, ARRAYSIZE(buffer),
            L"%04d-%02d-%02d %02d:%02d:%02d",
            pSystemTime.wYear, pSystemTime.wMonth, pSystemTime.wDay,
            pSystemTime.wHour, pSystemTime.wMinute, pSystemTime.wSecond);
        return buffer;
    }

    std::wstring XStringConversion::FormatDuration(UINT64 pMilliseconds) noexcept
    {
        UINT64 seconds = pMilliseconds / 1000;
        UINT64 minutes = seconds / 60;
        UINT64 hours = minutes / 60;

        wchar_t buffer[64];

        if (hours > 0)
        {
            StringCchPrintfW(buffer, ARRAYSIZE(buffer),
                L"%llu:%02llu:%02llu",
                hours, minutes % 60, seconds % 60);
        }
        else if (minutes > 0)
        {
            StringCchPrintfW(buffer, ARRAYSIZE(buffer),
                L"%llu:%02llu",
                minutes, seconds % 60);
        }
        else
        {
            StringCchPrintfW(buffer, ARRAYSIZE(buffer),
                L"%llu.%03llu s",
                seconds, pMilliseconds % 1000);
        }

        return buffer;
    }

} // namespace Tootega

