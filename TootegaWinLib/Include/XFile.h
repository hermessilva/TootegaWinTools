#pragma once
// ============================================================================
// XFile.h - File system utilities
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XPlatform.h"
#include "XTypes.h"
#include "XResult.h"

namespace Tootega
{
    // ========================================================================
    // XFile - File system utility functions
    // ========================================================================

    class XFile final
    {
    public:
        XFile() = delete;

        [[nodiscard]] static XResult<std::vector<BYTE>> ReadAllBytes(std::wstring_view pPath);
        [[nodiscard]] static XResult<std::wstring> ReadAllText(std::wstring_view pPath);
        [[nodiscard]] static XResult<std::vector<std::wstring>> ReadAllLines(std::wstring_view pPath);

        [[nodiscard]] static XResult<void> WriteAllBytes(std::wstring_view pPath, std::span<const BYTE> pData);
        [[nodiscard]] static XResult<void> WriteAllText(std::wstring_view pPath, std::wstring_view pText);

        [[nodiscard]] static XResult<void> AppendText(std::wstring_view pPath, std::wstring_view pText);

        [[nodiscard]] static bool Exists(std::wstring_view pPath) noexcept;
        [[nodiscard]] static bool IsDirectory(std::wstring_view pPath) noexcept;
        [[nodiscard]] static bool IsFile(std::wstring_view pPath) noexcept;

        [[nodiscard]] static XResult<void> Delete(std::wstring_view pPath);
        [[nodiscard]] static XResult<void> Copy(std::wstring_view pSource, std::wstring_view pDest, bool pOverwrite = false);
        [[nodiscard]] static XResult<void> Move(std::wstring_view pSource, std::wstring_view pDest);

        [[nodiscard]] static XResult<ULONGLONG> GetSize(std::wstring_view pPath);
        [[nodiscard]] static XResult<FILETIME> GetLastWriteTime(std::wstring_view pPath);

        [[nodiscard]] static std::wstring GetDirectory(std::wstring_view pPath);
        [[nodiscard]] static std::wstring GetFileName(std::wstring_view pPath);
        [[nodiscard]] static std::wstring GetFileNameWithoutExtension(std::wstring_view pPath);
        [[nodiscard]] static std::wstring GetExtension(std::wstring_view pPath);
        [[nodiscard]] static std::wstring ChangeExtension(std::wstring_view pPath, std::wstring_view pNewExtension);
        [[nodiscard]] static std::wstring Combine(std::wstring_view pPath1, std::wstring_view pPath2);

        [[nodiscard]] static XResult<std::wstring> GetTempPath();
        [[nodiscard]] static XResult<std::wstring> GetTempFileName(std::wstring_view pPrefix = L"tmp");
        [[nodiscard]] static XResult<std::wstring> GetAppDataPath();
        [[nodiscard]] static XResult<std::wstring> GetLocalAppDataPath();
        [[nodiscard]] static XResult<std::wstring> GetModulePath(HMODULE pModule = nullptr);

        [[nodiscard]] static XResult<void> CreateDirectory(std::wstring_view pPath);
        [[nodiscard]] static XResult<void> CreateDirectoryRecursive(std::wstring_view pPath);
        [[nodiscard]] static XResult<void> DeleteDirectory(std::wstring_view pPath, bool pRecursive = false);

        [[nodiscard]] static XResult<std::vector<std::wstring>> EnumerateFiles(
            std::wstring_view pPath,
            std::wstring_view pPattern = L"*");

        [[nodiscard]] static XResult<std::vector<std::wstring>> EnumerateDirectories(
            std::wstring_view pPath,
            std::wstring_view pPattern = L"*");
    };

} // namespace Tootega
