// ============================================================================
// XContextMenu.cpp - Shell Context Menu Handler Implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "../Include/Shell/XContextMenu.h"
#include <strsafe.h>

namespace Tootega::Shell
{
    // ========================================================================
    // XContextMenuBase Implementation
    // ========================================================================

    XContextMenuBase::XContextMenuBase()
        : _FirstCommandID(0)
    {
    }

    XContextMenuBase::~XContextMenuBase()
    {
    }

    STDMETHODIMP XContextMenuBase::QueryInterface(REFIID pRiid, void** pPpv)
    {
        if (!pPpv)
            return E_POINTER;

        if (IsEqualIID(pRiid, IID_IUnknown))
            *pPpv = static_cast<IUnknown*>(static_cast<IContextMenu*>(this));
        else if (IsEqualIID(pRiid, IID_IContextMenu))
            *pPpv = static_cast<IContextMenu*>(this);
        else if (IsEqualIID(pRiid, IID_IShellExtInit))
            *pPpv = static_cast<IShellExtInit*>(this);
        else
        {
            *pPpv = nullptr;
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    STDMETHODIMP XContextMenuBase::Initialize(
        PCIDLIST_ABSOLUTE pPidlFolder,
        IDataObject*      pDataObj,
        HKEY              pHkeyProgID)
    {
        _SelectedFiles.clear();

        if (!pDataObj)
            return E_INVALIDARG;

        _SelectedFiles = XDataObjectHelper::GetFilePaths(pDataObj);
        if (_SelectedFiles.empty())
            return E_FAIL;

        if (!OnInitialize(_SelectedFiles))
            return E_FAIL;

        return S_OK;
    }

    STDMETHODIMP XContextMenuBase::QueryContextMenu(
        HMENU pHmenu,
        UINT  pIndexMenu,
        UINT  pIdCmdFirst,
        UINT  pIdCmdLast,
        UINT  pUFlags)
    {
        if (pUFlags & CMF_DEFAULTONLY)
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);

        _Commands.clear();
        _FirstCommandID = pIdCmdFirst;

        OnBuildMenu(_Commands);

        if (_Commands.empty())
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);

        UINT cmdID = pIdCmdFirst;
        for (auto& cmd : _Commands)
        {
            cmd.ID = cmdID++;

            if (cmd.IsSeparator)
            {
                InsertMenuW(pHmenu, pIndexMenu++, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
            }
            else
            {
                InsertMenuW(pHmenu, pIndexMenu++, MF_BYPOSITION | cmd.Flags,
                    cmd.ID, cmd.Text.c_str());
            }
        }

        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, static_cast<USHORT>(_Commands.size()));
    }

    STDMETHODIMP XContextMenuBase::InvokeCommand(CMINVOKECOMMANDINFO* pIci)
    {
        if (!pIci)
            return E_POINTER;

        // Handle string verb
        if (HIWORD(pIci->lpVerb) != 0)
        {
            // Look up by verb name
            std::string verb(pIci->lpVerb);
            std::wstring wideVerb(verb.begin(), verb.end());

            for (const auto& cmd : _Commands)
            {
                if (!cmd.IsSeparator && cmd.Verb == wideVerb)
                {
                    return OnExecuteCommand(cmd.ID - _FirstCommandID, pIci->hwnd) ? S_OK : E_FAIL;
                }
            }
            return E_INVALIDARG;
        }

        UINT cmdOffset = LOWORD(pIci->lpVerb);
        return OnExecuteCommand(cmdOffset, pIci->hwnd) ? S_OK : E_FAIL;
    }

    STDMETHODIMP XContextMenuBase::GetCommandString(
        UINT_PTR pIdCmd,
        UINT     pUType,
        UINT*    pReserved,
        CHAR*    pszName,
        UINT     pCchMax)
    {
        if (pIdCmd >= _Commands.size())
            return E_INVALIDARG;

        const auto& cmd = _Commands[pIdCmd];

        switch (pUType)
        {
        case GCS_HELPTEXTA:
            WideCharToMultiByte(CP_ACP, 0, cmd.HelpText.c_str(), -1,
                pszName, pCchMax, nullptr, nullptr);
            return S_OK;

        case GCS_HELPTEXTW:
            StringCchCopyW(reinterpret_cast<LPWSTR>(pszName), pCchMax, cmd.HelpText.c_str());
            return S_OK;

        case GCS_VERBA:
            WideCharToMultiByte(CP_ACP, 0, cmd.Verb.c_str(), -1,
                pszName, pCchMax, nullptr, nullptr);
            return S_OK;

        case GCS_VERBW:
            StringCchCopyW(reinterpret_cast<LPWSTR>(pszName), pCchMax, cmd.Verb.c_str());
            return S_OK;

        case GCS_VALIDATEA:
        case GCS_VALIDATEW:
            return S_OK;

        default:
            return E_NOTIMPL;
        }
    }

    HMENU XContextMenuBase::BuildSubmenu(const std::vector<XMenuCommand>& pCommands, UINT pIdCmdFirst)
    {
        HMENU hSubMenu = CreatePopupMenu();
        if (!hSubMenu)
            return nullptr;

        UINT position = 0;
        for (const auto& cmd : pCommands)
        {
            if (cmd.IsSeparator)
            {
                InsertMenuW(hSubMenu, position++, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
            }
            else
            {
                InsertMenuW(hSubMenu, position++, MF_BYPOSITION | cmd.Flags,
                    pIdCmdFirst + cmd.ID, cmd.Text.c_str());
            }
        }

        return hSubMenu;
    }

    // ========================================================================
    // XDataObjectHelper Implementation
    // ========================================================================

    std::vector<std::wstring> XDataObjectHelper::GetFilePaths(IDataObject* pDataObject)
    {
        std::vector<std::wstring> files;

        if (!pDataObject)
            return files;

        FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM medium = {};

        if (FAILED(pDataObject->GetData(&fmt, &medium)))
            return files;

        HDROP hDrop = static_cast<HDROP>(medium.hGlobal);
        UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);

        files.reserve(count);
        wchar_t path[MAX_PATH];

        for (UINT i = 0; i < count; ++i)
        {
            if (DragQueryFileW(hDrop, i, path, MAX_PATH) > 0)
                files.emplace_back(path);
        }

        ReleaseStgMedium(&medium);
        return files;
    }

    std::wstring XDataObjectHelper::GetFirstFilePath(IDataObject* pDataObject)
    {
        auto files = GetFilePaths(pDataObject);
        return files.empty() ? std::wstring() : files[0];
    }

    bool XDataObjectHelper::HasFilePaths(IDataObject* pDataObject)
    {
        if (!pDataObject)
            return false;

        FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        return pDataObject->QueryGetData(&fmt) == S_OK;
    }

} // namespace Tootega::Shell

