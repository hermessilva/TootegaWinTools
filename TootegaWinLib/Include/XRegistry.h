#pragma once
// ============================================================================
// XRegistry.h - Windows Registry utilities
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XPlatform.h"
#include "XTypes.h"
#include "XResult.h"

namespace Tootega
{
    // ========================================================================
    // XRegistry - Registry access utilities
    // ========================================================================

    class XRegistry final
    {
    public:
        XRegistry() = delete;

        [[nodiscard]] static XResult<std::wstring> GetString(
            HKEY        pRootKey,
            std::wstring_view pSubKey,
            std::wstring_view pValueName);

        [[nodiscard]] static XResult<DWORD> GetDword(
            HKEY        pRootKey,
            std::wstring_view pSubKey,
            std::wstring_view pValueName);

        [[nodiscard]] static XResult<std::vector<BYTE>> GetBinary(
            HKEY        pRootKey,
            std::wstring_view pSubKey,
            std::wstring_view pValueName);

        [[nodiscard]] static XResult<std::vector<std::wstring>> GetMultiString(
            HKEY        pRootKey,
            std::wstring_view pSubKey,
            std::wstring_view pValueName);

        [[nodiscard]] static XResult<void> SetString(
            HKEY        pRootKey,
            std::wstring_view pSubKey,
            std::wstring_view pValueName,
            std::wstring_view pValue);

        [[nodiscard]] static XResult<void> SetDword(
            HKEY        pRootKey,
            std::wstring_view pSubKey,
            std::wstring_view pValueName,
            DWORD       pValue);

        [[nodiscard]] static XResult<void> SetBinary(
            HKEY        pRootKey,
            std::wstring_view pSubKey,
            std::wstring_view pValueName,
            std::span<const BYTE> pData);

        [[nodiscard]] static XResult<void> DeleteValue(
            HKEY        pRootKey,
            std::wstring_view pSubKey,
            std::wstring_view pValueName);

        [[nodiscard]] static XResult<void> DeleteKey(
            HKEY        pRootKey,
            std::wstring_view pSubKey);

        [[nodiscard]] static XResult<bool> KeyExists(
            HKEY        pRootKey,
            std::wstring_view pSubKey);

        [[nodiscard]] static XResult<bool> ValueExists(
            HKEY        pRootKey,
            std::wstring_view pSubKey,
            std::wstring_view pValueName);

        [[nodiscard]] static XResult<std::vector<std::wstring>> EnumerateSubKeys(
            HKEY        pRootKey,
            std::wstring_view pSubKey);

        [[nodiscard]] static XResult<std::vector<std::wstring>> EnumerateValues(
            HKEY        pRootKey,
            std::wstring_view pSubKey);

    private:
        [[nodiscard]] static XResult<XUniqueRegKey> OpenKey(
            HKEY        pRootKey,
            std::wstring_view pSubKey,
            REGSAM      pAccess);

        [[nodiscard]] static XResult<XUniqueRegKey> CreateKey(
            HKEY        pRootKey,
            std::wstring_view pSubKey,
            REGSAM      pAccess);
    };

} // namespace Tootega
