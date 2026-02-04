// ============================================================================
// XShellExtension.cpp - Windows Shell Extension Infrastructure Implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "../Include/Shell/XShellExtension.h"

namespace Tootega::Shell
{
    // ========================================================================
    // XShellModule Implementation
    // ========================================================================

    XShellModule& XShellModule::Instance() noexcept
    {
        static XShellModule instance;
        return instance;
    }

    void XShellModule::Initialize(HMODULE pModule) noexcept
    {
        _Module = pModule;
        DisableThreadLibraryCalls(pModule);
    }

    std::wstring XShellModule::GetModulePath() const
    {
        if (!_Module)
            return {};

        wchar_t path[MAX_PATH];
        DWORD len = GetModuleFileNameW(_Module, path, MAX_PATH);
        if (len == 0 || len >= MAX_PATH)
            return {};

        return path;
    }

} // namespace Tootega::Shell

