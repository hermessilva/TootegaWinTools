#pragma once
// ============================================================================
// XPlatform.h - Platform detection and Windows SDK configuration
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

// ============================================================================
// Windows Configuration - Reduce bloat
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef STRICT
#define STRICT
#endif

// ============================================================================
// Windows SDK Headers
// ============================================================================

#include <windows.h>
#include <wincrypt.h>
#include <ncrypt.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <userenv.h>

// ============================================================================
// Standard Library Headers
// ============================================================================

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <format>
#include <chrono>
#include <optional>
#include <span>
#include <array>
#include <algorithm>
#include <type_traits>
#include <concepts>
#include <source_location>

// ============================================================================
// Platform Detection
// ============================================================================

namespace Tootega
{
    struct XPlatformInfo final
    {
        static constexpr bool IsWindows = true;
        static constexpr bool Is64Bit = sizeof(void*) == 8;
        static constexpr bool IsDebug =
#ifdef _DEBUG
            true;
#else
            false;
#endif

        static constexpr const wchar_t* PlatformName = L"Windows";
        static constexpr const wchar_t* Architecture = Is64Bit ? L"x64" : L"x86";
    };

} // namespace Tootega

// ============================================================================
// Common Macros
// ============================================================================

#define TOOTEGA_UNUSED(x) (void)(x)
#define TOOTEGA_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define TOOTEGA_STRINGIFY(x) #x
#define TOOTEGA_WSTRINGIFY(x) L ## #x

// Disable copy
#define TOOTEGA_DISABLE_COPY(ClassName) \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete

// Disable move
#define TOOTEGA_DISABLE_MOVE(ClassName) \
    ClassName(ClassName&&) = delete; \
    ClassName& operator=(ClassName&&) = delete

// Disable copy and move
#define TOOTEGA_DISABLE_COPY_MOVE(ClassName) \
    TOOTEGA_DISABLE_COPY(ClassName); \
    TOOTEGA_DISABLE_MOVE(ClassName)
