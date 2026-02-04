// ============================================================================
// XRegistry.cpp - Windows Registry utilities implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XRegistry.h"

namespace Tootega
{
    XResult<XUniqueRegKey> XRegistry::OpenKey(HKEY pRootKey, std::wstring_view pSubKey, REGSAM pAccess)
    {
        HKEY key = nullptr;
        LSTATUS status = RegOpenKeyExW(pRootKey, std::wstring(pSubKey).c_str(), 0, pAccess, &key);

        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegOpenKeyExW failed");

        return MakeUniqueRegKey(key);
    }

    XResult<XUniqueRegKey> XRegistry::CreateKey(HKEY pRootKey, std::wstring_view pSubKey, REGSAM pAccess)
    {
        HKEY key = nullptr;
        DWORD disposition = 0;
        LSTATUS status = RegCreateKeyExW(
            pRootKey,
            std::wstring(pSubKey).c_str(),
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            pAccess,
            nullptr,
            &key,
            &disposition);

        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegCreateKeyExW failed");

        return MakeUniqueRegKey(key);
    }

    XResult<std::wstring> XRegistry::GetString(HKEY pRootKey, std::wstring_view pSubKey, std::wstring_view pValueName)
    {
        auto keyResult = OpenKey(pRootKey, pSubKey, KEY_READ);
        if (!keyResult)
            return keyResult.Error();

        DWORD type = 0;
        DWORD size = 0;
        std::wstring valueName(pValueName);

        LSTATUS status = RegQueryValueExW(keyResult->get(), valueName.c_str(), nullptr, &type, nullptr, &size);
        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegQueryValueExW size query failed");

        if (type != REG_SZ && type != REG_EXPAND_SZ)
            return XError::Application(1, L"Registry value is not a string");

        std::wstring result(size / sizeof(wchar_t), L'\0');
        status = RegQueryValueExW(keyResult->get(), valueName.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(result.data()), &size);
        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegQueryValueExW failed");

        while (!result.empty() && result.back() == L'\0')
            result.pop_back();

        return result;
    }

    XResult<DWORD> XRegistry::GetDword(HKEY pRootKey, std::wstring_view pSubKey, std::wstring_view pValueName)
    {
        auto keyResult = OpenKey(pRootKey, pSubKey, KEY_READ);
        if (!keyResult)
            return keyResult.Error();

        DWORD type = 0;
        DWORD value = 0;
        DWORD size = sizeof(value);
        std::wstring valueName(pValueName);

        LSTATUS status = RegQueryValueExW(keyResult->get(), valueName.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegQueryValueExW failed");

        if (type != REG_DWORD)
            return XError::Application(1, L"Registry value is not a DWORD");

        return value;
    }

    XResult<std::vector<BYTE>> XRegistry::GetBinary(HKEY pRootKey, std::wstring_view pSubKey, std::wstring_view pValueName)
    {
        auto keyResult = OpenKey(pRootKey, pSubKey, KEY_READ);
        if (!keyResult)
            return keyResult.Error();

        DWORD type = 0;
        DWORD size = 0;
        std::wstring valueName(pValueName);

        LSTATUS status = RegQueryValueExW(keyResult->get(), valueName.c_str(), nullptr, &type, nullptr, &size);
        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegQueryValueExW size query failed");

        if (type != REG_BINARY)
            return XError::Application(1, L"Registry value is not binary");

        std::vector<BYTE> result(size);
        status = RegQueryValueExW(keyResult->get(), valueName.c_str(), nullptr, &type, result.data(), &size);
        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegQueryValueExW failed");

        return result;
    }

    XResult<std::vector<std::wstring>> XRegistry::GetMultiString(HKEY pRootKey, std::wstring_view pSubKey, std::wstring_view pValueName)
    {
        auto keyResult = OpenKey(pRootKey, pSubKey, KEY_READ);
        if (!keyResult)
            return keyResult.Error();

        DWORD type = 0;
        DWORD size = 0;
        std::wstring valueName(pValueName);

        LSTATUS status = RegQueryValueExW(keyResult->get(), valueName.c_str(), nullptr, &type, nullptr, &size);
        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegQueryValueExW size query failed");

        if (type != REG_MULTI_SZ)
            return XError::Application(1, L"Registry value is not a multi-string");

        std::vector<wchar_t> buffer(size / sizeof(wchar_t));
        status = RegQueryValueExW(keyResult->get(), valueName.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &size);
        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegQueryValueExW failed");

        std::vector<std::wstring> result;
        const wchar_t* ptr = buffer.data();
        while (*ptr != L'\0')
        {
            result.emplace_back(ptr);
            ptr += wcslen(ptr) + 1;
        }

        return result;
    }

    XResult<void> XRegistry::SetString(HKEY pRootKey, std::wstring_view pSubKey, std::wstring_view pValueName, std::wstring_view pValue)
    {
        auto keyResult = CreateKey(pRootKey, pSubKey, KEY_WRITE);
        if (!keyResult)
            return keyResult.Error();

        std::wstring valueName(pValueName);
        std::wstring value(pValue);

        LSTATUS status = RegSetValueExW(
            keyResult->get(),
            valueName.c_str(),
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));

        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegSetValueExW failed");

        return XResult<void>::Success();
    }

    XResult<void> XRegistry::SetDword(HKEY pRootKey, std::wstring_view pSubKey, std::wstring_view pValueName, DWORD pValue)
    {
        auto keyResult = CreateKey(pRootKey, pSubKey, KEY_WRITE);
        if (!keyResult)
            return keyResult.Error();

        std::wstring valueName(pValueName);

        LSTATUS status = RegSetValueExW(
            keyResult->get(),
            valueName.c_str(),
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE*>(&pValue),
            sizeof(pValue));

        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegSetValueExW failed");

        return XResult<void>::Success();
    }

    XResult<void> XRegistry::SetBinary(HKEY pRootKey, std::wstring_view pSubKey, std::wstring_view pValueName, std::span<const BYTE> pData)
    {
        auto keyResult = CreateKey(pRootKey, pSubKey, KEY_WRITE);
        if (!keyResult)
            return keyResult.Error();

        std::wstring valueName(pValueName);

        LSTATUS status = RegSetValueExW(
            keyResult->get(),
            valueName.c_str(),
            0,
            REG_BINARY,
            pData.data(),
            static_cast<DWORD>(pData.size()));

        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegSetValueExW failed");

        return XResult<void>::Success();
    }

    XResult<void> XRegistry::DeleteValue(HKEY pRootKey, std::wstring_view pSubKey, std::wstring_view pValueName)
    {
        auto keyResult = OpenKey(pRootKey, pSubKey, KEY_SET_VALUE);
        if (!keyResult)
            return keyResult.Error();

        std::wstring valueName(pValueName);
        LSTATUS status = RegDeleteValueW(keyResult->get(), valueName.c_str());

        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegDeleteValueW failed");

        return XResult<void>::Success();
    }

    XResult<void> XRegistry::DeleteKey(HKEY pRootKey, std::wstring_view pSubKey)
    {
        std::wstring subKey(pSubKey);
        LSTATUS status = RegDeleteKeyW(pRootKey, subKey.c_str());

        if (status != ERROR_SUCCESS)
            return XError::FromWin32(static_cast<DWORD>(status), L"RegDeleteKeyW failed");

        return XResult<void>::Success();
    }

    XResult<bool> XRegistry::KeyExists(HKEY pRootKey, std::wstring_view pSubKey)
    {
        HKEY key = nullptr;
        std::wstring subKey(pSubKey);
        LSTATUS status = RegOpenKeyExW(pRootKey, subKey.c_str(), 0, KEY_READ, &key);

        if (status == ERROR_SUCCESS)
        {
            RegCloseKey(key);
            return true;
        }

        if (status == ERROR_FILE_NOT_FOUND)
            return false;

        return XError::FromWin32(static_cast<DWORD>(status), L"RegOpenKeyExW failed");
    }

    XResult<bool> XRegistry::ValueExists(HKEY pRootKey, std::wstring_view pSubKey, std::wstring_view pValueName)
    {
        auto keyResult = OpenKey(pRootKey, pSubKey, KEY_READ);
        if (!keyResult)
        {
            if (keyResult.Error().GetCode() == ERROR_FILE_NOT_FOUND)
                return false;
            return keyResult.Error();
        }

        std::wstring valueName(pValueName);
        LSTATUS status = RegQueryValueExW(keyResult->get(), valueName.c_str(), nullptr, nullptr, nullptr, nullptr);

        if (status == ERROR_SUCCESS)
            return true;

        if (status == ERROR_FILE_NOT_FOUND)
            return false;

        return XError::FromWin32(static_cast<DWORD>(status), L"RegQueryValueExW failed");
    }

    XResult<std::vector<std::wstring>> XRegistry::EnumerateSubKeys(HKEY pRootKey, std::wstring_view pSubKey)
    {
        auto keyResult = OpenKey(pRootKey, pSubKey, KEY_READ);
        if (!keyResult)
            return keyResult.Error();

        std::vector<std::wstring> result;
        wchar_t name[256];
        DWORD index = 0;

        while (true)
        {
            DWORD nameLen = ARRAYSIZE(name);
            LSTATUS status = RegEnumKeyExW(keyResult->get(), index, name, &nameLen, nullptr, nullptr, nullptr, nullptr);

            if (status == ERROR_NO_MORE_ITEMS)
                break;

            if (status != ERROR_SUCCESS)
                return XError::FromWin32(static_cast<DWORD>(status), L"RegEnumKeyExW failed");

            result.emplace_back(name, nameLen);
            ++index;
        }

        return result;
    }

    XResult<std::vector<std::wstring>> XRegistry::EnumerateValues(HKEY pRootKey, std::wstring_view pSubKey)
    {
        auto keyResult = OpenKey(pRootKey, pSubKey, KEY_READ);
        if (!keyResult)
            return keyResult.Error();

        std::vector<std::wstring> result;
        wchar_t name[16384];
        DWORD index = 0;

        while (true)
        {
            DWORD nameLen = ARRAYSIZE(name);
            LSTATUS status = RegEnumValueW(keyResult->get(), index, name, &nameLen, nullptr, nullptr, nullptr, nullptr);

            if (status == ERROR_NO_MORE_ITEMS)
                break;

            if (status != ERROR_SUCCESS)
                return XError::FromWin32(static_cast<DWORD>(status), L"RegEnumValueW failed");

            result.emplace_back(name, nameLen);
            ++index;
        }

        return result;
    }

} // namespace Tootega
