#pragma once
// ============================================================================
// XShellRegistry.h - Shell Extension Registry Utilities
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================
//
// Provides utilities for registering and unregistering shell extensions
// in the Windows Registry, including:
//   - CLSID registration
//   - ProgID registration
//   - File extension associations
//   - Shell extension handler registration
//   - System file associations
//
// ============================================================================

#include "../XPlatform.h"
#include "../XTypes.h"
#include "../XResult.h"

namespace Tootega::Shell
{
    // ========================================================================
    // Registry Entry Types
    // ========================================================================

    enum class XRegValueType
    {
        String,
        ExpandString,
        Dword
    };

    // ========================================================================
    // Registry Entry Definition
    // ========================================================================

    struct XRegistryEntry
    {
        HKEY            RootKey;
        std::wstring    KeyPath;
        std::wstring    ValueName;      // Empty for default value
        XRegValueType   ValueType;
        std::wstring    StringValue;    // For String/ExpandString types
        DWORD           DwordValue;     // For Dword type
        bool            UsesModulePath; // If true, StringValue contains %s for module path
    };

    // ========================================================================
    // Shell Extension Types
    // ========================================================================

    enum class XShellHandlerType
    {
        ContextMenu,
        PropertySheet,
        IconHandler,
        PreviewHandler,
        ThumbnailHandler,
        PropertyHandler,
        InfoTip,
        CopyHook,
        DropHandler,
        DataHandler
    };

    // ========================================================================
    // XShellRegistry - Registry Operations for Shell Extensions
    // ========================================================================

    class XShellRegistry final
    {
    public:
        XShellRegistry() = delete;

        // ====================================================================
        // CLSID Registration
        // ====================================================================

        [[nodiscard]] static XResult<void> RegisterCLSID(
            const GUID&        pClsid,
            std::wstring_view  pDescription,
            std::wstring_view  pModulePath,
            std::wstring_view  pThreadingModel = L"Apartment",
            bool               pPerUser = true);

        [[nodiscard]] static XResult<void> UnregisterCLSID(
            const GUID&        pClsid,
            bool               pPerUser = true);

        // ====================================================================
        // ProgID Registration
        // ====================================================================

        [[nodiscard]] static XResult<void> RegisterProgID(
            std::wstring_view  pProgID,
            std::wstring_view  pDescription,
            std::wstring_view  pFriendlyTypeName,
            const GUID&        pClsid,
            std::wstring_view  pDefaultIcon,
            bool               pPerUser = true);

        [[nodiscard]] static XResult<void> UnregisterProgID(
            std::wstring_view  pProgID,
            bool               pPerUser = true);

        // ====================================================================
        // File Extension Association
        // ====================================================================

        [[nodiscard]] static XResult<void> RegisterFileExtension(
            std::wstring_view  pExtension,
            std::wstring_view  pProgID,
            std::wstring_view  pPerceivedType,
            std::wstring_view  pContentType,
            bool               pPerUser = true);

        [[nodiscard]] static XResult<void> UnregisterFileExtension(
            std::wstring_view  pExtension,
            bool               pPerUser = true);

        // ====================================================================
        // Shell Handlers
        // ====================================================================

        [[nodiscard]] static XResult<void> RegisterShellHandler(
            std::wstring_view  pProgID,
            XShellHandlerType  pHandlerType,
            std::wstring_view  pHandlerName,
            const GUID&        pClsid,
            bool               pPerUser = true);

        [[nodiscard]] static XResult<void> UnregisterShellHandler(
            std::wstring_view  pProgID,
            XShellHandlerType  pHandlerType,
            std::wstring_view  pHandlerName,
            bool               pPerUser = true);

        // ====================================================================
        // Shell Folder Registration (for browsable extensions)
        // ====================================================================

        [[nodiscard]] static XResult<void> RegisterShellFolder(
            const GUID&        pClsid,
            std::wstring_view  pDescription,
            std::wstring_view  pModulePath,
            DWORD              pAttributes,
            bool               pPerUser = true);

        [[nodiscard]] static XResult<void> RegisterBrowsableCategory(
            const GUID&        pClsid,
            bool               pPerUser = true);

        // ====================================================================
        // System File Associations
        // ====================================================================

        [[nodiscard]] static XResult<void> RegisterSystemFileAssociation(
            std::wstring_view  pExtension,
            const GUID&        pClsid,
            bool               pPerUser = true);

        [[nodiscard]] static XResult<void> UnregisterSystemFileAssociation(
            std::wstring_view  pExtension,
            bool               pPerUser = true);

        // ====================================================================
        // Approved Extensions
        // ====================================================================

        [[nodiscard]] static XResult<void> RegisterApprovedExtension(
            const GUID&        pClsid,
            std::wstring_view  pDescription,
            bool               pPerUser = true);

        [[nodiscard]] static XResult<void> UnregisterApprovedExtension(
            const GUID&        pClsid,
            bool               pPerUser = true);

        // ====================================================================
        // Batch Operations
        // ====================================================================

        [[nodiscard]] static XResult<void> ApplyRegistryEntries(
            const std::vector<XRegistryEntry>& pEntries,
            std::wstring_view                  pModulePath);

        [[nodiscard]] static XResult<void> DeleteRegistryKeys(
            HKEY                               pRootKey,
            const std::vector<std::wstring>&   pKeyPaths);

        // ====================================================================
        // Notification
        // ====================================================================

        static void NotifyShellOfChanges() noexcept;

        // ====================================================================
        // Utilities
        // ====================================================================

        [[nodiscard]] static std::wstring GuidToRegistryString(const GUID& pGuid);
        [[nodiscard]] static std::wstring GetHandlerSubkey(XShellHandlerType pType);

    private:
        [[nodiscard]] static HKEY GetRootKey(bool pPerUser) noexcept;
        [[nodiscard]] static std::wstring GetClassesRoot(bool pPerUser);
    };

} // namespace Tootega::Shell

