// ============================================================================
// XString.cpp - String manipulation utilities implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XString.h"

namespace Tootega
{
    std::wstring XString::ToWide(std::string_view pStr)
    {
        if (pStr.empty())
            return {};

        int size = MultiByteToWideChar(CP_UTF8, 0, pStr.data(), static_cast<int>(pStr.size()), nullptr, 0);
        if (size <= 0)
            return {};

        std::wstring result(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, pStr.data(), static_cast<int>(pStr.size()), result.data(), size);
        return result;
    }

    std::string XString::ToUtf8(std::wstring_view pStr)
    {
        if (pStr.empty())
            return {};

        int size = WideCharToMultiByte(CP_UTF8, 0, pStr.data(), static_cast<int>(pStr.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0)
            return {};

        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, pStr.data(), static_cast<int>(pStr.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    std::wstring XString::ToLower(std::wstring_view pStr)
    {
        std::wstring result(pStr);
        CharLowerBuffW(result.data(), static_cast<DWORD>(result.size()));
        return result;
    }

    std::wstring XString::ToUpper(std::wstring_view pStr)
    {
        std::wstring result(pStr);
        CharUpperBuffW(result.data(), static_cast<DWORD>(result.size()));
        return result;
    }

    std::wstring XString::Trim(std::wstring_view pStr)
    {
        return TrimRight(TrimLeft(pStr));
    }

    std::wstring XString::TrimLeft(std::wstring_view pStr)
    {
        auto it = pStr.begin();
        while (it != pStr.end() && iswspace(*it))
            ++it;
        return std::wstring(it, pStr.end());
    }

    std::wstring XString::TrimRight(std::wstring_view pStr)
    {
        auto it = pStr.end();
        while (it != pStr.begin() && iswspace(*(it - 1)))
            --it;
        return std::wstring(pStr.begin(), it);
    }

    bool XString::StartsWith(std::wstring_view pStr, std::wstring_view pPrefix) noexcept
    {
        if (pPrefix.size() > pStr.size())
            return false;
        return pStr.substr(0, pPrefix.size()) == pPrefix;
    }

    bool XString::EndsWith(std::wstring_view pStr, std::wstring_view pSuffix) noexcept
    {
        if (pSuffix.size() > pStr.size())
            return false;
        return pStr.substr(pStr.size() - pSuffix.size()) == pSuffix;
    }

    bool XString::Contains(std::wstring_view pStr, std::wstring_view pSubstr) noexcept
    {
        return pStr.find(pSubstr) != std::wstring_view::npos;
    }

    bool XString::EqualsIgnoreCase(std::wstring_view pA, std::wstring_view pB) noexcept
    {
        if (pA.size() != pB.size())
            return false;
        return _wcsnicmp(pA.data(), pB.data(), pA.size()) == 0;
    }

    std::vector<std::wstring> XString::Split(std::wstring_view pStr, wchar_t pDelimiter)
    {
        std::vector<std::wstring> result;
        size_t start = 0;
        size_t end = 0;

        while ((end = pStr.find(pDelimiter, start)) != std::wstring_view::npos)
        {
            result.emplace_back(pStr.substr(start, end - start));
            start = end + 1;
        }

        result.emplace_back(pStr.substr(start));
        return result;
    }

    std::vector<std::wstring> XString::Split(std::wstring_view pStr, std::wstring_view pDelimiter)
    {
        std::vector<std::wstring> result;

        if (pDelimiter.empty())
        {
            result.emplace_back(pStr);
            return result;
        }

        size_t start = 0;
        size_t end = 0;

        while ((end = pStr.find(pDelimiter, start)) != std::wstring_view::npos)
        {
            result.emplace_back(pStr.substr(start, end - start));
            start = end + pDelimiter.size();
        }

        result.emplace_back(pStr.substr(start));
        return result;
    }

    std::wstring XString::Join(const std::vector<std::wstring>& pParts, std::wstring_view pSeparator)
    {
        if (pParts.empty())
            return {};

        size_t totalSize = 0;
        for (const auto& part : pParts)
            totalSize += part.size();
        totalSize += pSeparator.size() * (pParts.size() - 1);

        std::wstring result;
        result.reserve(totalSize);

        for (size_t i = 0; i < pParts.size(); ++i)
        {
            if (i > 0)
                result += pSeparator;
            result += pParts[i];
        }

        return result;
    }

    std::wstring XString::Replace(std::wstring_view pStr, std::wstring_view pOld, std::wstring_view pNew)
    {
        if (pOld.empty())
            return std::wstring(pStr);

        std::wstring result(pStr);
        size_t pos = result.find(pOld);
        if (pos != std::wstring::npos)
            result.replace(pos, pOld.size(), pNew);

        return result;
    }

    std::wstring XString::ReplaceAll(std::wstring_view pStr, std::wstring_view pOld, std::wstring_view pNew)
    {
        if (pOld.empty())
            return std::wstring(pStr);

        std::wstring result(pStr);
        size_t pos = 0;

        while ((pos = result.find(pOld, pos)) != std::wstring::npos)
        {
            result.replace(pos, pOld.size(), pNew);
            pos += pNew.size();
        }

        return result;
    }

    std::wstring XString::FromErrorCode(DWORD pErrorCode)
    {
        LPWSTR buffer = nullptr;
        DWORD size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            pErrorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&buffer),
            0,
            nullptr);

        if (size == 0 || buffer == nullptr)
            return std::format(L"Error 0x{:08X}", pErrorCode);

        std::wstring result(buffer, size);
        LocalFree(buffer);

        while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n'))
            result.pop_back();

        return result;
    }

    std::wstring XString::FromNtStatus(NTSTATUS pStatus)
    {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll)
            return std::format(L"NTSTATUS 0x{:08X}", static_cast<DWORD>(pStatus));

        LPWSTR buffer = nullptr;
        DWORD size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
            ntdll,
            static_cast<DWORD>(pStatus),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&buffer),
            0,
            nullptr);

        if (size == 0 || buffer == nullptr)
            return std::format(L"NTSTATUS 0x{:08X}", static_cast<DWORD>(pStatus));

        std::wstring result(buffer, size);
        LocalFree(buffer);

        while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n'))
            result.pop_back();

        return result;
    }

    std::wstring XString::ToHex(const BYTE* pData, size_t pSize)
    {
        if (!pData || pSize == 0)
            return {};

        std::wstring result;
        result.reserve(pSize * 2);

        for (size_t i = 0; i < pSize; ++i)
            result += std::format(L"{:02X}", pData[i]);

        return result;
    }

    std::wstring XString::ToHex(std::span<const BYTE> pData)
    {
        return ToHex(pData.data(), pData.size());
    }

    XResult<std::vector<BYTE>> XString::FromHex(std::wstring_view pHexString)
    {
        if (pHexString.size() % 2 != 0)
            return XError::Application(1, L"Hex string must have even length");

        std::vector<BYTE> result;
        result.reserve(pHexString.size() / 2);

        for (size_t i = 0; i < pHexString.size(); i += 2)
        {
            wchar_t hex[3] = {pHexString[i], pHexString[i + 1], L'\0'};
            wchar_t* end = nullptr;
            unsigned long value = wcstoul(hex, &end, 16);

            if (end != hex + 2)
                return XError::Application(2, L"Invalid hex character");

            result.push_back(static_cast<BYTE>(value));
        }

        return result;
    }

    std::wstring XString::Base64Encode(const BYTE* pData, size_t pSize)
    {
        if (!pData || pSize == 0)
            return {};

        DWORD base64Size = 0;
        if (!CryptBinaryToStringW(pData, static_cast<DWORD>(pSize), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &base64Size))
            return {};

        std::wstring result(base64Size, L'\0');
        if (!CryptBinaryToStringW(pData, static_cast<DWORD>(pSize), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, result.data(), &base64Size))
            return {};

        while (!result.empty() && result.back() == L'\0')
            result.pop_back();

        return result;
    }

    std::wstring XString::Base64Encode(std::span<const BYTE> pData)
    {
        return Base64Encode(pData.data(), pData.size());
    }

    XResult<std::vector<BYTE>> XString::Base64Decode(std::wstring_view pBase64String)
    {
        if (pBase64String.empty())
            return std::vector<BYTE>{};

        DWORD binarySize = 0;
        if (!CryptStringToBinaryW(pBase64String.data(), static_cast<DWORD>(pBase64String.size()), CRYPT_STRING_BASE64, nullptr, &binarySize, nullptr, nullptr))
            return XError::FromLastError(L"CryptStringToBinaryW size query failed");

        std::vector<BYTE> result(binarySize);
        if (!CryptStringToBinaryW(pBase64String.data(), static_cast<DWORD>(pBase64String.size()), CRYPT_STRING_BASE64, result.data(), &binarySize, nullptr, nullptr))
            return XError::FromLastError(L"CryptStringToBinaryW decode failed");

        result.resize(binarySize);
        return result;
    }

    bool XString::IsNullOrEmpty(const wchar_t* pStr) noexcept
    {
        return pStr == nullptr || *pStr == L'\0';
    }

    bool XString::IsNullOrWhitespace(const wchar_t* pStr) noexcept
    {
        if (pStr == nullptr)
            return true;

        while (*pStr != L'\0')
        {
            if (!iswspace(*pStr))
                return false;
            ++pStr;
        }

        return true;
    }

} // namespace Tootega
