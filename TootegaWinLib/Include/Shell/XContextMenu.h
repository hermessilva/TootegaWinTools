#pragma once
// ============================================================================
// XContextMenu.h - Shell Context Menu Handler Infrastructure
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================
//
// Provides base classes for implementing Windows Explorer context menu
// handlers including:
//   - IContextMenu implementation base
//   - IShellExtInit implementation base
//   - Menu item management utilities
//   - Command string handling
//
// ============================================================================

#include "XShellExtension.h"

namespace Tootega::Shell
{
    // ========================================================================
    // Menu Command Definition
    // ========================================================================

    struct XMenuCommand
    {
        UINT            ID;
        std::wstring    Text;
        std::wstring    HelpText;
        std::wstring    Verb;
        UINT            Flags;          // MF_* flags
        bool            IsSeparator;

        XMenuCommand() noexcept
            : ID(0)
            , Flags(MF_STRING)
            , IsSeparator(false)
        {
        }

        static XMenuCommand Separator() noexcept
        {
            XMenuCommand cmd;
            cmd.IsSeparator = true;
            cmd.Flags = MF_SEPARATOR;
            return cmd;
        }
    };

    // ========================================================================
    // Submenu Definition
    // ========================================================================

    struct XSubmenu
    {
        std::wstring                Text;
        std::vector<XMenuCommand>   Commands;
        HBITMAP                     Icon;

        XSubmenu() noexcept : Icon(nullptr) {}
    };

    // ========================================================================
    // XContextMenuBase - Base class for context menu handlers
    // ========================================================================

    class XContextMenuBase : public XComObject<IContextMenu, IShellExtInit>
    {
    public:
        XContextMenuBase();
        virtual ~XContextMenuBase();

        // IUnknown
        STDMETHODIMP QueryInterface(REFIID pRiid, void** pPpv) override;

        // IShellExtInit
        STDMETHODIMP Initialize(
            PCIDLIST_ABSOLUTE pPidlFolder,
            IDataObject*      pDataObj,
            HKEY              pHkeyProgID) override;

        // IContextMenu
        STDMETHODIMP QueryContextMenu(
            HMENU pHmenu,
            UINT  pIndexMenu,
            UINT  pIdCmdFirst,
            UINT  pIdCmdLast,
            UINT  pUFlags) override;

        STDMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* pIci) override;

        STDMETHODIMP GetCommandString(
            UINT_PTR pIdCmd,
            UINT     pUType,
            UINT*    pReserved,
            CHAR*    pszName,
            UINT     pCchMax) override;

    protected:
        // Override these in derived classes
        virtual bool OnInitialize(const std::vector<std::wstring>& pSelectedFiles) = 0;
        virtual void OnBuildMenu(std::vector<XMenuCommand>& pCommands) = 0;
        virtual bool OnExecuteCommand(UINT pCommandID, HWND pHwnd) = 0;

        // Helpers for derived classes
        [[nodiscard]] const std::vector<std::wstring>& GetSelectedFiles() const noexcept
        {
            return _SelectedFiles;
        }

        [[nodiscard]] std::wstring GetFirstSelectedFile() const noexcept
        {
            return _SelectedFiles.empty() ? std::wstring() : _SelectedFiles[0];
        }

        // Build a submenu with commands
        HMENU BuildSubmenu(const std::vector<XMenuCommand>& pCommands, UINT pIdCmdFirst);

    private:
        std::vector<std::wstring>   _SelectedFiles;
        std::vector<XMenuCommand>   _Commands;
        UINT                        _FirstCommandID;
    };

    // ========================================================================
    // XDataObjectHelper - Utilities for working with IDataObject
    // ========================================================================

    class XDataObjectHelper final
    {
    public:
        XDataObjectHelper() = delete;

        [[nodiscard]] static std::vector<std::wstring> GetFilePaths(IDataObject* pDataObject);
        [[nodiscard]] static std::wstring GetFirstFilePath(IDataObject* pDataObject);
        [[nodiscard]] static bool HasFilePaths(IDataObject* pDataObject);
    };

} // namespace Tootega::Shell

