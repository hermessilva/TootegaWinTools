#pragma once
// ============================================================================
// XString.h - String manipulation utilities
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XPlatform.h"
#include "XResult.h"

namespace Tootega
{
    // ========================================================================
    // XString - String utility functions
    // ========================================================================

    class XString final
    {
    public:
        XString() = delete;

        [[nodiscard]] static std::wstring ToWide(std::string_view pStr);
        [[nodiscard]] static std::string ToUtf8(std::wstring_view pStr);
        [[nodiscard]] static std::wstring ToLower(std::wstring_view pStr);
        [[nodiscard]] static std::wstring ToUpper(std::wstring_view pStr);

        [[nodiscard]] static std::wstring Trim(std::wstring_view pStr);
        [[nodiscard]] static std::wstring TrimLeft(std::wstring_view pStr);
        [[nodiscard]] static std::wstring TrimRight(std::wstring_view pStr);

        [[nodiscard]] static bool StartsWith(std::wstring_view pStr, std::wstring_view pPrefix) noexcept;
        [[nodiscard]] static bool EndsWith(std::wstring_view pStr, std::wstring_view pSuffix) noexcept;
        [[nodiscard]] static bool Contains(std::wstring_view pStr, std::wstring_view pSubstr) noexcept;

        [[nodiscard]] static bool EqualsIgnoreCase(std::wstring_view pA, std::wstring_view pB) noexcept;

        [[nodiscard]] static std::vector<std::wstring> Split(std::wstring_view pStr, wchar_t pDelimiter);
        [[nodiscard]] static std::vector<std::wstring> Split(std::wstring_view pStr, std::wstring_view pDelimiter);

        [[nodiscard]] static std::wstring Join(const std::vector<std::wstring>& pParts, std::wstring_view pSeparator);

        [[nodiscard]] static std::wstring Replace(std::wstring_view pStr, std::wstring_view pOld, std::wstring_view pNew);
        [[nodiscard]] static std::wstring ReplaceAll(std::wstring_view pStr, std::wstring_view pOld, std::wstring_view pNew);

        [[nodiscard]] static std::wstring FromErrorCode(DWORD pErrorCode);
        [[nodiscard]] static std::wstring FromNtStatus(NTSTATUS pStatus);

        [[nodiscard]] static std::wstring ToHex(const BYTE* pData, size_t pSize);
        [[nodiscard]] static std::wstring ToHex(std::span<const BYTE> pData);
        [[nodiscard]] static XResult<std::vector<BYTE>> FromHex(std::wstring_view pHexString);

        [[nodiscard]] static std::wstring Base64Encode(const BYTE* pData, size_t pSize);
        [[nodiscard]] static std::wstring Base64Encode(std::span<const BYTE> pData);
        [[nodiscard]] static XResult<std::vector<BYTE>> Base64Decode(std::wstring_view pBase64String);

        [[nodiscard]] static bool IsNullOrEmpty(const wchar_t* pStr) noexcept;
        [[nodiscard]] static bool IsNullOrWhitespace(const wchar_t* pStr) noexcept;

        template<typename... TArgs>
        [[nodiscard]] static std::wstring Format(std::wstring_view pFormat, TArgs&&... pArgs)
        {
            return std::vformat(pFormat, std::make_wformat_args(std::forward<TArgs>(pArgs)...));
        }
    };

} // namespace Tootega
