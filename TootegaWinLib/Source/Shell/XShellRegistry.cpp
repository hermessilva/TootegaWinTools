// ============================================================================
// XShellRegistry.cpp - Shell Extension Registry Utilities Implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "../Include/Shell/XShellRegistry.h"
#include "../Include/XString.h"
#include <strsafe.h>

namespace Tootega::Shell
{
    // ========================================================================
    // Helper Functions
    // ========================================================================

    HKEY XShellRegistry::GetRootKey(bool pPerUser) noexcept
    {
        return pPerUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    }

    std::wstring XShellRegistry::GetClassesRoot(bool pPerUser)
    {
        return pPerUser ? L"Software\\Classes" : L"";
    }

    std::wstring XShellRegistry::GuidToRegistryString(const GUID& pGuid)
    {
        wchar_t buffer[39];
        StringFromGUID2(pGuid, buffer, 39);
        return buffer;
    }

    std::wstring XShellRegistry::GetHandlerSubkey(XShellHandlerType pType)
    {
        switch (pType)
        {
        case XShellHandlerType::ContextMenu:
            return L"ShellEx\\ContextMenuHandlers";
        case XShellHandlerType::PropertySheet:
            return L"ShellEx\\PropertySheetHandlers";
        case XShellHandlerType::IconHandler:
            return L"ShellEx\\IconHandler";
        case XShellHandlerType::PreviewHandler:
            return L"ShellEx\\{8895b1c6-b41f-4c1c-a562-0d564250836f}";
        case XShellHandlerType::ThumbnailHandler:
            return L"ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}";
        case XShellHandlerType::PropertyHandler:
            return L"ShellEx\\PropertyHandler";
        case XShellHandlerType::InfoTip:
            return L"ShellEx\\{00021500-0000-0000-C000-000000000046}";
        case XShellHandlerType::CopyHook:
            return L"ShellEx\\CopyHookHandlers";
        case XShellHandlerType::DropHandler:
            return L"ShellEx\\DropHandler";
        case XShellHandlerType::DataHandler:
            return L"ShellEx\\DataHandler";
        default:
            return L"ShellEx";
        }
    }

    // ========================================================================
    // CLSID Registration
    // ========================================================================

    XResult<void> XShellRegistry::RegisterCLSID(
        const GUID&        pClsid,
        std::wstring_view  pDescription,
        std::wstring_view  pModulePath,
        std::wstring_view  pThreadingModel,
        bool               pPerUser)
    {
        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring classesRoot = GetClassesRoot(pPerUser);
        std::wstring clsidStr = GuidToRegistryString(pClsid);

        std::wstring keyPath = classesRoot.empty()
            ? std::format(L"CLSID\\{}", clsidStr)
            : std::format(L"{}\\CLSID\\{}", classesRoot, clsidStr);

        // Create CLSID key with description
        HKEY hKey;
        LONG result = RegCreateKeyExW(rootKey, keyPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result != ERROR_SUCCESS)
            return XError::FromWin32(result, L"Failed to create CLSID key");

        // Set default value (description)
        std::wstring desc(pDescription);
        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(desc.c_str()),
            static_cast<DWORD>((desc.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);

        // Create InProcServer32 subkey
        std::wstring inprocPath = keyPath + L"\\InProcServer32";
        result = RegCreateKeyExW(rootKey, inprocPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result != ERROR_SUCCESS)
            return XError::FromWin32(result, L"Failed to create InProcServer32 key");

        // Set module path
        std::wstring modPath(pModulePath);
        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(modPath.c_str()),
            static_cast<DWORD>((modPath.size() + 1) * sizeof(wchar_t)));

        // Set threading model
        std::wstring threading(pThreadingModel);
        RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(threading.c_str()),
            static_cast<DWORD>((threading.size() + 1) * sizeof(wchar_t)));

        RegCloseKey(hKey);
        return {};
    }

    XResult<void> XShellRegistry::UnregisterCLSID(
        const GUID& pClsid,
        bool        pPerUser)
    {
        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring classesRoot = GetClassesRoot(pPerUser);
        std::wstring clsidStr = GuidToRegistryString(pClsid);

        std::wstring keyPath = classesRoot.empty()
            ? std::format(L"CLSID\\{}", clsidStr)
            : std::format(L"{}\\CLSID\\{}", classesRoot, clsidStr);

        LONG result = RegDeleteTreeW(rootKey, keyPath.c_str());
        if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND)
            return XError::FromWin32(result, L"Failed to delete CLSID key");

        return {};
    }

    // ========================================================================
    // ProgID Registration
    // ========================================================================

    XResult<void> XShellRegistry::RegisterProgID(
        std::wstring_view  pProgID,
        std::wstring_view  pDescription,
        std::wstring_view  pFriendlyTypeName,
        const GUID&        pClsid,
        std::wstring_view  pDefaultIcon,
        bool               pPerUser)
    {
        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring classesRoot = GetClassesRoot(pPerUser);
        std::wstring clsidStr = GuidToRegistryString(pClsid);

        std::wstring keyPath = classesRoot.empty()
            ? std::wstring(pProgID)
            : std::format(L"{}\\{}", classesRoot, pProgID);

        // Create ProgID key
        HKEY hKey;
        LONG result = RegCreateKeyExW(rootKey, keyPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result != ERROR_SUCCESS)
            return XError::FromWin32(result, L"Failed to create ProgID key");

        std::wstring desc(pDescription);
        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(desc.c_str()),
            static_cast<DWORD>((desc.size() + 1) * sizeof(wchar_t)));

        if (!pFriendlyTypeName.empty())
        {
            std::wstring friendly(pFriendlyTypeName);
            RegSetValueExW(hKey, L"FriendlyTypeName", 0, REG_SZ,
                reinterpret_cast<const BYTE*>(friendly.c_str()),
                static_cast<DWORD>((friendly.size() + 1) * sizeof(wchar_t)));
        }
        RegCloseKey(hKey);

        // Create CLSID subkey
        std::wstring clsidKeyPath = keyPath + L"\\CLSID";
        result = RegCreateKeyExW(rootKey, clsidKeyPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result != ERROR_SUCCESS)
            return XError::FromWin32(result, L"Failed to create CLSID subkey");

        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(clsidStr.c_str()),
            static_cast<DWORD>((clsidStr.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);

        // Create DefaultIcon subkey if provided
        if (!pDefaultIcon.empty())
        {
            std::wstring iconKeyPath = keyPath + L"\\DefaultIcon";
            result = RegCreateKeyExW(rootKey, iconKeyPath.c_str(), 0, nullptr,
                REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
            if (result == ERROR_SUCCESS)
            {
                std::wstring icon(pDefaultIcon);
                RegSetValueExW(hKey, nullptr, 0, REG_SZ,
                    reinterpret_cast<const BYTE*>(icon.c_str()),
                    static_cast<DWORD>((icon.size() + 1) * sizeof(wchar_t)));
                RegCloseKey(hKey);
            }
        }

        return {};
    }

    XResult<void> XShellRegistry::UnregisterProgID(
        std::wstring_view pProgID,
        bool              pPerUser)
    {
        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring classesRoot = GetClassesRoot(pPerUser);

        std::wstring keyPath = classesRoot.empty()
            ? std::wstring(pProgID)
            : std::format(L"{}\\{}", classesRoot, pProgID);

        LONG result = RegDeleteTreeW(rootKey, keyPath.c_str());
        if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND)
            return XError::FromWin32(result, L"Failed to delete ProgID key");

        return {};
    }

    // ========================================================================
    // File Extension Association
    // ========================================================================

    XResult<void> XShellRegistry::RegisterFileExtension(
        std::wstring_view pExtension,
        std::wstring_view pProgID,
        std::wstring_view pPerceivedType,
        std::wstring_view pContentType,
        bool              pPerUser)
    {
        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring classesRoot = GetClassesRoot(pPerUser);

        std::wstring keyPath = classesRoot.empty()
            ? std::wstring(pExtension)
            : std::format(L"{}\\{}", classesRoot, pExtension);

        HKEY hKey;
        LONG result = RegCreateKeyExW(rootKey, keyPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result != ERROR_SUCCESS)
            return XError::FromWin32(result, L"Failed to create extension key");

        std::wstring progid(pProgID);
        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(progid.c_str()),
            static_cast<DWORD>((progid.size() + 1) * sizeof(wchar_t)));

        if (!pPerceivedType.empty())
        {
            std::wstring perceived(pPerceivedType);
            RegSetValueExW(hKey, L"PerceivedType", 0, REG_SZ,
                reinterpret_cast<const BYTE*>(perceived.c_str()),
                static_cast<DWORD>((perceived.size() + 1) * sizeof(wchar_t)));
        }

        if (!pContentType.empty())
        {
            std::wstring content(pContentType);
            RegSetValueExW(hKey, L"Content Type", 0, REG_SZ,
                reinterpret_cast<const BYTE*>(content.c_str()),
                static_cast<DWORD>((content.size() + 1) * sizeof(wchar_t)));
        }

        RegCloseKey(hKey);

        // Create OpenWithProgids subkey
        std::wstring openWithPath = keyPath + L"\\OpenWithProgids";
        result = RegCreateKeyExW(rootKey, openWithPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result == ERROR_SUCCESS)
        {
            RegSetValueExW(hKey, progid.c_str(), 0, REG_SZ,
                reinterpret_cast<const BYTE*>(L""), sizeof(wchar_t));
            RegCloseKey(hKey);
        }

        return {};
    }

    XResult<void> XShellRegistry::UnregisterFileExtension(
        std::wstring_view pExtension,
        bool              pPerUser)
    {
        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring classesRoot = GetClassesRoot(pPerUser);

        std::wstring keyPath = classesRoot.empty()
            ? std::wstring(pExtension)
            : std::format(L"{}\\{}", classesRoot, pExtension);

        // Just clear the default value and remove OpenWithProgids
        HKEY hKey;
        if (RegOpenKeyExW(rootKey, keyPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
        {
            RegDeleteValueW(hKey, nullptr);
            RegDeleteValueW(hKey, L"PerceivedType");
            RegDeleteValueW(hKey, L"Content Type");
            RegCloseKey(hKey);
        }

        std::wstring openWithPath = keyPath + L"\\OpenWithProgids";
        RegDeleteTreeW(rootKey, openWithPath.c_str());

        return {};
    }

    // ========================================================================
    // Shell Handlers
    // ========================================================================

    XResult<void> XShellRegistry::RegisterShellHandler(
        std::wstring_view  pProgID,
        XShellHandlerType  pHandlerType,
        std::wstring_view  pHandlerName,
        const GUID&        pClsid,
        bool               pPerUser)
    {
        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring classesRoot = GetClassesRoot(pPerUser);
        std::wstring clsidStr = GuidToRegistryString(pClsid);
        std::wstring handlerSubkey = GetHandlerSubkey(pHandlerType);

        std::wstring keyPath = classesRoot.empty()
            ? std::format(L"{}\\{}\\{}", pProgID, handlerSubkey, pHandlerName)
            : std::format(L"{}\\{}\\{}\\{}", classesRoot, pProgID, handlerSubkey, pHandlerName);

        HKEY hKey;
        LONG result = RegCreateKeyExW(rootKey, keyPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result != ERROR_SUCCESS)
            return XError::FromWin32(result, L"Failed to create shell handler key");

        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(clsidStr.c_str()),
            static_cast<DWORD>((clsidStr.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);

        return {};
    }

    XResult<void> XShellRegistry::UnregisterShellHandler(
        std::wstring_view  pProgID,
        XShellHandlerType  pHandlerType,
        std::wstring_view  pHandlerName,
        bool               pPerUser)
    {
        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring classesRoot = GetClassesRoot(pPerUser);
        std::wstring handlerSubkey = GetHandlerSubkey(pHandlerType);

        std::wstring keyPath = classesRoot.empty()
            ? std::format(L"{}\\{}\\{}", pProgID, handlerSubkey, pHandlerName)
            : std::format(L"{}\\{}\\{}\\{}", classesRoot, pProgID, handlerSubkey, pHandlerName);

        LONG result = RegDeleteTreeW(rootKey, keyPath.c_str());
        if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND)
            return XError::FromWin32(result, L"Failed to delete shell handler key");

        return {};
    }

    // ========================================================================
    // Shell Folder Registration
    // ========================================================================

    XResult<void> XShellRegistry::RegisterShellFolder(
        const GUID&        pClsid,
        std::wstring_view  pDescription,
        std::wstring_view  pModulePath,
        DWORD              pAttributes,
        bool               pPerUser)
    {
        // First register the CLSID
        auto result = RegisterCLSID(pClsid, pDescription, pModulePath, L"Apartment", pPerUser);
        if (!result)
            return result;

        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring classesRoot = GetClassesRoot(pPerUser);
        std::wstring clsidStr = GuidToRegistryString(pClsid);

        // Add ShellFolder subkey with attributes
        std::wstring shellFolderPath = classesRoot.empty()
            ? std::format(L"CLSID\\{}\\ShellFolder", clsidStr)
            : std::format(L"{}\\CLSID\\{}\\ShellFolder", classesRoot, clsidStr);

        HKEY hKey;
        LONG regResult = RegCreateKeyExW(rootKey, shellFolderPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
        if (regResult != ERROR_SUCCESS)
            return XError::FromWin32(regResult, L"Failed to create ShellFolder key");

        RegSetValueExW(hKey, L"Attributes", 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&pAttributes), sizeof(DWORD));
        RegCloseKey(hKey);

        return {};
    }

    XResult<void> XShellRegistry::RegisterBrowsableCategory(
        const GUID& pClsid,
        bool        pPerUser)
    {
        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring classesRoot = GetClassesRoot(pPerUser);
        std::wstring clsidStr = GuidToRegistryString(pClsid);

        // Browsable Shell Extension category GUID
        const wchar_t* categoryGuid = L"{00021490-0000-0000-C000-000000000046}";

        std::wstring keyPath = classesRoot.empty()
            ? std::format(L"CLSID\\{}\\Implemented Categories\\{}", clsidStr, categoryGuid)
            : std::format(L"{}\\CLSID\\{}\\Implemented Categories\\{}", classesRoot, clsidStr, categoryGuid);

        HKEY hKey;
        LONG result = RegCreateKeyExW(rootKey, keyPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result != ERROR_SUCCESS)
            return XError::FromWin32(result, L"Failed to create Implemented Categories key");

        RegCloseKey(hKey);
        return {};
    }

    // ========================================================================
    // System File Associations
    // ========================================================================

    XResult<void> XShellRegistry::RegisterSystemFileAssociation(
        std::wstring_view pExtension,
        const GUID&       pClsid,
        bool              pPerUser)
    {
        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring classesRoot = GetClassesRoot(pPerUser);
        std::wstring clsidStr = GuidToRegistryString(pClsid);

        std::wstring keyPath = classesRoot.empty()
            ? std::format(L"SystemFileAssociations\\{}\\CLSID", pExtension)
            : std::format(L"{}\\SystemFileAssociations\\{}\\CLSID", classesRoot, pExtension);

        HKEY hKey;
        LONG result = RegCreateKeyExW(rootKey, keyPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result != ERROR_SUCCESS)
            return XError::FromWin32(result, L"Failed to create SystemFileAssociations key");

        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(clsidStr.c_str()),
            static_cast<DWORD>((clsidStr.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);

        return {};
    }

    XResult<void> XShellRegistry::UnregisterSystemFileAssociation(
        std::wstring_view pExtension,
        bool              pPerUser)
    {
        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring classesRoot = GetClassesRoot(pPerUser);

        std::wstring keyPath = classesRoot.empty()
            ? std::format(L"SystemFileAssociations\\{}", pExtension)
            : std::format(L"{}\\SystemFileAssociations\\{}", classesRoot, pExtension);

        LONG result = RegDeleteTreeW(rootKey, keyPath.c_str());
        if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND)
            return XError::FromWin32(result, L"Failed to delete SystemFileAssociations key");

        return {};
    }

    // ========================================================================
    // Approved Extensions
    // ========================================================================

    XResult<void> XShellRegistry::RegisterApprovedExtension(
        const GUID&        pClsid,
        std::wstring_view  pDescription,
        bool               pPerUser)
    {
        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring clsidStr = GuidToRegistryString(pClsid);

        std::wstring keyPath = pPerUser
            ? L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"
            : L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";

        HKEY hKey;
        LONG result = RegCreateKeyExW(rootKey, keyPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result != ERROR_SUCCESS)
            return XError::FromWin32(result, L"Failed to create Approved key");

        std::wstring desc(pDescription);
        RegSetValueExW(hKey, clsidStr.c_str(), 0, REG_SZ,
            reinterpret_cast<const BYTE*>(desc.c_str()),
            static_cast<DWORD>((desc.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);

        return {};
    }

    XResult<void> XShellRegistry::UnregisterApprovedExtension(
        const GUID& pClsid,
        bool        pPerUser)
    {
        HKEY rootKey = GetRootKey(pPerUser);
        std::wstring clsidStr = GuidToRegistryString(pClsid);

        std::wstring keyPath = pPerUser
            ? L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"
            : L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";

        HKEY hKey;
        if (RegOpenKeyExW(rootKey, keyPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
        {
            RegDeleteValueW(hKey, clsidStr.c_str());
            RegCloseKey(hKey);
        }

        return {};
    }

    // ========================================================================
    // Batch Operations
    // ========================================================================

    XResult<void> XShellRegistry::ApplyRegistryEntries(
        const std::vector<XRegistryEntry>& pEntries,
        std::wstring_view                  pModulePath)
    {
        for (const auto& entry : pEntries)
        {
            HKEY hKey;
            LONG result = RegCreateKeyExW(entry.RootKey, entry.KeyPath.c_str(), 0, nullptr,
                REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
            if (result != ERROR_SUCCESS)
                return XError::FromWin32(result, std::format(L"Failed to create key: {}", entry.KeyPath));

            const wchar_t* valueName = entry.ValueName.empty() ? nullptr : entry.ValueName.c_str();

            switch (entry.ValueType)
            {
            case XRegValueType::String:
            case XRegValueType::ExpandString:
            {
                std::wstring value = entry.StringValue;
                if (entry.UsesModulePath)
                {
                    // Replace %s with module path
                    size_t pos = value.find(L"%s");
                    if (pos != std::wstring::npos)
                        value.replace(pos, 2, pModulePath);
                }

                DWORD type = (entry.ValueType == XRegValueType::ExpandString) ? REG_EXPAND_SZ : REG_SZ;
                result = RegSetValueExW(hKey, valueName, 0, type,
                    reinterpret_cast<const BYTE*>(value.c_str()),
                    static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
                break;
            }
            case XRegValueType::Dword:
                result = RegSetValueExW(hKey, valueName, 0, REG_DWORD,
                    reinterpret_cast<const BYTE*>(&entry.DwordValue), sizeof(DWORD));
                break;
            }

            RegCloseKey(hKey);

            if (result != ERROR_SUCCESS)
                return XError::FromWin32(result, std::format(L"Failed to set value in: {}", entry.KeyPath));
        }

        return {};
    }

    XResult<void> XShellRegistry::DeleteRegistryKeys(
        HKEY                               pRootKey,
        const std::vector<std::wstring>&   pKeyPaths)
    {
        for (const auto& keyPath : pKeyPaths)
        {
            LONG result = RegDeleteTreeW(pRootKey, keyPath.c_str());
            if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND)
                return XError::FromWin32(result, std::format(L"Failed to delete key: {}", keyPath));
        }
        return {};
    }

    // ========================================================================
    // Notification
    // ========================================================================

    void XShellRegistry::NotifyShellOfChanges() noexcept
    {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }

} // namespace Tootega::Shell

