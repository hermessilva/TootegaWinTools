// ============================================================================
// XFile.cpp - File system utilities implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XFile.h"
#include "XString.h"

namespace Tootega
{
    XResult<std::vector<BYTE>> XFile::ReadAllBytes(std::wstring_view pPath)
    {
        std::wstring path(pPath);
        HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file == INVALID_HANDLE_VALUE)
            return XError::FromLastError(L"CreateFileW failed");

        XUniqueHandle fileGuard(file);

        LARGE_INTEGER fileSize{};
        if (!GetFileSizeEx(file, &fileSize))
            return XError::FromLastError(L"GetFileSizeEx failed");

        if (fileSize.QuadPart > static_cast<LONGLONG>(std::numeric_limits<size_t>::max()))
            return XError::Application(1, L"File too large");

        std::vector<BYTE> result(static_cast<size_t>(fileSize.QuadPart));

        if (fileSize.QuadPart > 0)
        {
            DWORD bytesRead = 0;
            if (!ReadFile(file, result.data(), static_cast<DWORD>(result.size()), &bytesRead, nullptr))
                return XError::FromLastError(L"ReadFile failed");
        }

        return result;
    }

    XResult<std::wstring> XFile::ReadAllText(std::wstring_view pPath)
    {
        auto bytesResult = ReadAllBytes(pPath);
        if (!bytesResult)
            return bytesResult.Error();

        const auto& bytes = *bytesResult;
        if (bytes.empty())
            return std::wstring{};

        if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
            return std::wstring(reinterpret_cast<const wchar_t*>(bytes.data() + 2), (bytes.size() - 2) / sizeof(wchar_t));

        if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
            return XString::ToWide(std::string_view(reinterpret_cast<const char*>(bytes.data() + 3), bytes.size() - 3));

        return XString::ToWide(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    }

    XResult<std::vector<std::wstring>> XFile::ReadAllLines(std::wstring_view pPath)
    {
        auto textResult = ReadAllText(pPath);
        if (!textResult)
            return textResult.Error();

        return XString::Split(*textResult, L'\n');
    }

    XResult<void> XFile::WriteAllBytes(std::wstring_view pPath, std::span<const BYTE> pData)
    {
        std::wstring path(pPath);
        HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file == INVALID_HANDLE_VALUE)
            return XError::FromLastError(L"CreateFileW failed");

        XUniqueHandle fileGuard(file);

        if (!pData.empty())
        {
            DWORD bytesWritten = 0;
            if (!WriteFile(file, pData.data(), static_cast<DWORD>(pData.size()), &bytesWritten, nullptr))
                return XError::FromLastError(L"WriteFile failed");
        }

        return XResult<void>::Success();
    }

    XResult<void> XFile::WriteAllText(std::wstring_view pPath, std::wstring_view pText)
    {
        std::string utf8 = XString::ToUtf8(pText);
        std::vector<BYTE> data;
        data.reserve(3 + utf8.size());
        data.push_back(0xEF);
        data.push_back(0xBB);
        data.push_back(0xBF);
        data.insert(data.end(), utf8.begin(), utf8.end());
        return WriteAllBytes(pPath, data);
    }

    XResult<void> XFile::AppendText(std::wstring_view pPath, std::wstring_view pText)
    {
        std::wstring path(pPath);
        HANDLE file = CreateFileW(
            path.c_str(),
            FILE_APPEND_DATA,
            FILE_SHARE_READ,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file == INVALID_HANDLE_VALUE)
            return XError::FromLastError(L"CreateFileW failed");

        XUniqueHandle fileGuard(file);

        std::string utf8 = XString::ToUtf8(pText);
        if (!utf8.empty())
        {
            DWORD bytesWritten = 0;
            if (!WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &bytesWritten, nullptr))
                return XError::FromLastError(L"WriteFile failed");
        }

        return XResult<void>::Success();
    }

    bool XFile::Exists(std::wstring_view pPath) noexcept
    {
        std::wstring path(pPath);
        DWORD attrs = GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES;
    }

    bool XFile::IsDirectory(std::wstring_view pPath) noexcept
    {
        std::wstring path(pPath);
        DWORD attrs = GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
    }

    bool XFile::IsFile(std::wstring_view pPath) noexcept
    {
        std::wstring path(pPath);
        DWORD attrs = GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
    }

    XResult<void> XFile::Delete(std::wstring_view pPath)
    {
        std::wstring path(pPath);
        if (!DeleteFileW(path.c_str()))
            return XError::FromLastError(L"DeleteFileW failed");
        return XResult<void>::Success();
    }

    XResult<void> XFile::Copy(std::wstring_view pSource, std::wstring_view pDest, bool pOverwrite)
    {
        std::wstring source(pSource);
        std::wstring dest(pDest);
        if (!CopyFileW(source.c_str(), dest.c_str(), pOverwrite ? FALSE : TRUE))
            return XError::FromLastError(L"CopyFileW failed");
        return XResult<void>::Success();
    }

    XResult<void> XFile::Move(std::wstring_view pSource, std::wstring_view pDest)
    {
        std::wstring source(pSource);
        std::wstring dest(pDest);
        if (!MoveFileW(source.c_str(), dest.c_str()))
            return XError::FromLastError(L"MoveFileW failed");
        return XResult<void>::Success();
    }

    XResult<ULONGLONG> XFile::GetSize(std::wstring_view pPath)
    {
        std::wstring path(pPath);
        WIN32_FILE_ATTRIBUTE_DATA attrs{};
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attrs))
            return XError::FromLastError(L"GetFileAttributesExW failed");

        ULARGE_INTEGER size{};
        size.LowPart = attrs.nFileSizeLow;
        size.HighPart = attrs.nFileSizeHigh;
        return size.QuadPart;
    }

    XResult<FILETIME> XFile::GetLastWriteTime(std::wstring_view pPath)
    {
        std::wstring path(pPath);
        WIN32_FILE_ATTRIBUTE_DATA attrs{};
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attrs))
            return XError::FromLastError(L"GetFileAttributesExW failed");
        return attrs.ftLastWriteTime;
    }

    std::wstring XFile::GetDirectory(std::wstring_view pPath)
    {
        size_t pos = pPath.find_last_of(L"\\/");
        if (pos == std::wstring_view::npos)
            return {};
        return std::wstring(pPath.substr(0, pos));
    }

    std::wstring XFile::GetFileName(std::wstring_view pPath)
    {
        size_t pos = pPath.find_last_of(L"\\/");
        if (pos == std::wstring_view::npos)
            return std::wstring(pPath);
        return std::wstring(pPath.substr(pos + 1));
    }

    std::wstring XFile::GetFileNameWithoutExtension(std::wstring_view pPath)
    {
        std::wstring filename = GetFileName(pPath);
        size_t pos = filename.find_last_of(L'.');
        if (pos == std::wstring::npos)
            return filename;
        return filename.substr(0, pos);
    }

    std::wstring XFile::GetExtension(std::wstring_view pPath)
    {
        size_t pos = pPath.find_last_of(L'.');
        size_t sepPos = pPath.find_last_of(L"\\/");

        if (pos == std::wstring_view::npos)
            return {};
        if (sepPos != std::wstring_view::npos && pos < sepPos)
            return {};

        return std::wstring(pPath.substr(pos));
    }

    std::wstring XFile::ChangeExtension(std::wstring_view pPath, std::wstring_view pNewExtension)
    {
        std::wstring withoutExt = GetDirectory(pPath);
        if (!withoutExt.empty())
            withoutExt += L'\\';
        withoutExt += GetFileNameWithoutExtension(pPath);

        if (!pNewExtension.empty() && pNewExtension[0] != L'.')
            withoutExt += L'.';
        withoutExt += pNewExtension;

        return withoutExt;
    }

    std::wstring XFile::Combine(std::wstring_view pPath1, std::wstring_view pPath2)
    {
        if (pPath1.empty())
            return std::wstring(pPath2);
        if (pPath2.empty())
            return std::wstring(pPath1);

        std::wstring result(pPath1);

        bool path1HasSep = (result.back() == L'\\' || result.back() == L'/');
        bool path2HasSep = (pPath2.front() == L'\\' || pPath2.front() == L'/');

        if (path1HasSep && path2HasSep)
            result += pPath2.substr(1);
        else if (!path1HasSep && !path2HasSep)
        {
            result += L'\\';
            result += pPath2;
        }
        else
            result += pPath2;

        return result;
    }

    XResult<std::wstring> XFile::GetTempPath()
    {
        wchar_t buffer[MAX_PATH + 1]{};
        DWORD len = ::GetTempPathW(MAX_PATH + 1, buffer);
        if (len == 0)
            return XError::FromLastError(L"GetTempPathW failed");
        return std::wstring(buffer, len);
    }

    XResult<std::wstring> XFile::GetTempFileName(std::wstring_view pPrefix)
    {
        auto tempPathResult = GetTempPath();
        if (!tempPathResult)
            return tempPathResult.Error();

        std::wstring prefix(pPrefix);
        wchar_t buffer[MAX_PATH + 1]{};

        if (::GetTempFileNameW(tempPathResult->c_str(), prefix.c_str(), 0, buffer) == 0)
            return XError::FromLastError(L"GetTempFileNameW failed");

        return std::wstring(buffer);
    }

    XResult<std::wstring> XFile::GetAppDataPath()
    {
        wchar_t buffer[MAX_PATH]{};
        HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buffer);
        if (FAILED(hr))
            return XError::FromWin32(static_cast<DWORD>(hr), L"SHGetFolderPathW APPDATA failed");
        return std::wstring(buffer);
    }

    XResult<std::wstring> XFile::GetLocalAppDataPath()
    {
        wchar_t buffer[MAX_PATH]{};
        HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, buffer);
        if (FAILED(hr))
            return XError::FromWin32(static_cast<DWORD>(hr), L"SHGetFolderPathW LOCAL_APPDATA failed");
        return std::wstring(buffer);
    }

    XResult<std::wstring> XFile::GetModulePath(HMODULE pModule)
    {
        wchar_t buffer[MAX_PATH]{};
        DWORD len = GetModuleFileNameW(pModule, buffer, MAX_PATH);
        if (len == 0)
            return XError::FromLastError(L"GetModuleFileNameW failed");
        return GetDirectory(std::wstring_view(buffer, len));
    }

    XResult<void> XFile::CreateDirectory(std::wstring_view pPath)
    {
        std::wstring path(pPath);
        if (!::CreateDirectoryW(path.c_str(), nullptr))
        {
            DWORD err = GetLastError();
            if (err != ERROR_ALREADY_EXISTS)
                return XError::FromWin32(err, L"CreateDirectoryW failed");
        }
        return XResult<void>::Success();
    }

    XResult<void> XFile::CreateDirectoryRecursive(std::wstring_view pPath)
    {
        std::wstring path(pPath);

        if (path.empty())
            return XResult<void>::Success();

        if (path.size() >= 2 && path[1] == L':')
        {
            size_t start = (path.size() > 2 && (path[2] == L'\\' || path[2] == L'/')) ? 3 : 2;
            size_t pos = start;

            while ((pos = path.find_first_of(L"\\/", pos)) != std::wstring::npos)
            {
                std::wstring subPath = path.substr(0, pos);
                ::CreateDirectoryW(subPath.c_str(), nullptr);
                ++pos;
            }
        }

        return CreateDirectory(pPath);
    }

    XResult<void> XFile::DeleteDirectory(std::wstring_view pPath, bool pRecursive)
    {
        if (pRecursive)
        {
            auto filesResult = EnumerateFiles(pPath);
            if (filesResult)
            {
                for (const auto& file : *filesResult)
                    static_cast<void>(Delete(file));
            }

            auto dirsResult = EnumerateDirectories(pPath);
            if (dirsResult)
            {
                for (const auto& dir : *dirsResult)
                    static_cast<void>(DeleteDirectory(dir, true));
            }
        }

        std::wstring path(pPath);
        if (!RemoveDirectoryW(path.c_str()))
            return XError::FromLastError(L"RemoveDirectoryW failed");

        return XResult<void>::Success();
    }

    XResult<std::vector<std::wstring>> XFile::EnumerateFiles(std::wstring_view pPath, std::wstring_view pPattern)
    {
        std::wstring searchPath = Combine(pPath, pPattern);

        WIN32_FIND_DATAW findData{};
        HANDLE findHandle = FindFirstFileW(searchPath.c_str(), &findData);

        if (findHandle == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND)
                return std::vector<std::wstring>{};
            return XError::FromWin32(err, L"FindFirstFileW failed");
        }

        XUniqueFindFile findGuard(findHandle);
        std::vector<std::wstring> result;

        do
        {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                result.push_back(Combine(pPath, findData.cFileName));
        } while (FindNextFileW(findHandle, &findData));

        return result;
    }

    XResult<std::vector<std::wstring>> XFile::EnumerateDirectories(std::wstring_view pPath, std::wstring_view pPattern)
    {
        std::wstring searchPath = Combine(pPath, pPattern);

        WIN32_FIND_DATAW findData{};
        HANDLE findHandle = FindFirstFileW(searchPath.c_str(), &findData);

        if (findHandle == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND)
                return std::vector<std::wstring>{};
            return XError::FromWin32(err, L"FindFirstFileW failed");
        }

        XUniqueFindFile findGuard(findHandle);
        std::vector<std::wstring> result;

        do
        {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0)
                    result.push_back(Combine(pPath, findData.cFileName));
            }
        } while (FindNextFileW(findHandle, &findData));

        return result;
    }

} // namespace Tootega
