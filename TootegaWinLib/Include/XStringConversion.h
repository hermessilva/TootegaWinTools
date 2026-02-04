#pragma once
// ============================================================================
// XStringConversion.h - String Encoding Conversion Utilities
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================
//
// High-performance string conversion utilities for:
//   - UTF-8 <-> Wide string conversion
//   - UTF-16 <-> UTF-8 encoding
//   - ANSI <-> Wide string conversion
//   - File size and number formatting
//
// ============================================================================

#include "XPlatform.h"
#include "XResult.h"

namespace Tootega
{
    // ========================================================================
    // XStringConversion - Encoding Conversion Utilities
    // ========================================================================

    class XStringConversion final
    {
    public:
        XStringConversion() = delete;

        // ====================================================================
        // UTF-8 <-> Wide String Conversion
        // ====================================================================

        [[nodiscard]] static std::wstring Utf8ToWide(std::string_view pUtf8) noexcept;
        [[nodiscard]] static std::wstring Utf8ToWide(const char* pUtf8) noexcept;

        [[nodiscard]] static std::string WideToUtf8(std::wstring_view pWide) noexcept;
        [[nodiscard]] static std::string WideToUtf8(const wchar_t* pWide) noexcept;

        // ====================================================================
        // ANSI <-> Wide String Conversion (system code page)
        // ====================================================================

        [[nodiscard]] static std::wstring AnsiToWide(std::string_view pAnsi) noexcept;
        [[nodiscard]] static std::string WideToAnsi(std::wstring_view pWide) noexcept;

        // ====================================================================
        // Size Formatting (human-readable file sizes)
        // ====================================================================

        [[nodiscard]] static std::wstring FormatFileSize(UINT64 pBytes) noexcept;
        [[nodiscard]] static std::wstring FormatFileSizeEx(UINT64 pBytes, int pDecimals = 1) noexcept;

        // ====================================================================
        // Compression Ratio Formatting
        // ====================================================================

        [[nodiscard]] static std::wstring FormatCompressionRatio(UINT64 pCompressed, UINT64 pOriginal) noexcept;

        // ====================================================================
        // Number Formatting
        // ====================================================================

        [[nodiscard]] static std::wstring FormatNumber(UINT64 pNumber) noexcept;
        [[nodiscard]] static std::wstring FormatPercentage(double pValue, int pDecimals = 1) noexcept;

        // ====================================================================
        // Time Formatting
        // ====================================================================

        [[nodiscard]] static std::wstring FormatFileTime(const FILETIME& pFileTime) noexcept;
        [[nodiscard]] static std::wstring FormatSystemTime(const SYSTEMTIME& pSystemTime) noexcept;
        [[nodiscard]] static std::wstring FormatDuration(UINT64 pMilliseconds) noexcept;
    };

} // namespace Tootega

