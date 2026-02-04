#pragma once
// ============================================================================
// XTypes.h - Common type definitions, enums and RAII wrappers
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XPlatform.h"

namespace Tootega
{
    // ========================================================================
    // NTSTATUS Helpers
    // ========================================================================

#ifndef NT_SUCCESS
#define NT_SUCCESS(status) ((status) >= 0)
#endif

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS                  ((NTSTATUS)0x00000000L)
#endif
#ifndef STATUS_NOT_SUPPORTED
#define STATUS_NOT_SUPPORTED            ((NTSTATUS)0xC00000BBL)
#endif
#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL         ((NTSTATUS)0xC0000023L)
#endif
#ifndef STATUS_INSUFFICIENT_RESOURCES
#define STATUS_INSUFFICIENT_RESOURCES   ((NTSTATUS)0xC000009AL)
#endif
#ifndef STATUS_INTERNAL_ERROR
#define STATUS_INTERNAL_ERROR           ((NTSTATUS)0xC00000E5L)
#endif
#ifndef STATUS_INVALID_SIGNATURE
#define STATUS_INVALID_SIGNATURE        ((NTSTATUS)0xC000A000L)
#endif
#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER        ((NTSTATUS)0xC000000DL)
#endif
#ifndef STATUS_NO_MEMORY
#define STATUS_NO_MEMORY                ((NTSTATUS)0xC0000017L)
#endif
#ifndef STATUS_ACCESS_DENIED
#define STATUS_ACCESS_DENIED            ((NTSTATUS)0xC0000022L)
#endif
#ifndef STATUS_OBJECT_NAME_NOT_FOUND
#define STATUS_OBJECT_NAME_NOT_FOUND    ((NTSTATUS)0xC0000034L)
#endif

    // ========================================================================
    // RAII Deleters
    // ========================================================================

    struct XHandleDeleter final
    {
        void operator()(HANDLE pHandle) const noexcept
        {
            if (pHandle && pHandle != INVALID_HANDLE_VALUE)
                CloseHandle(pHandle);
        }
    };

    struct XCertStoreDeleter final
    {
        void operator()(HCERTSTORE pStore) const noexcept
        {
            if (pStore)
                CertCloseStore(pStore, 0);
        }
    };

    struct XCertContextDeleter final
    {
        void operator()(PCCERT_CONTEXT pContext) const noexcept
        {
            if (pContext)
                CertFreeCertificateContext(pContext);
        }
    };

    struct XNCryptKeyDeleter final
    {
        void operator()(NCRYPT_KEY_HANDLE pKey) const noexcept
        {
            if (pKey)
                NCryptFreeObject(pKey);
        }
    };

    struct XNCryptProviderDeleter final
    {
        void operator()(NCRYPT_PROV_HANDLE pProvider) const noexcept
        {
            if (pProvider)
                NCryptFreeObject(pProvider);
        }
    };

    struct XBCryptKeyDeleter final
    {
        void operator()(BCRYPT_KEY_HANDLE pKey) const noexcept
        {
            if (pKey)
                BCryptDestroyKey(pKey);
        }
    };

    struct XBCryptAlgDeleter final
    {
        void operator()(BCRYPT_ALG_HANDLE pAlg) const noexcept
        {
            if (pAlg)
                BCryptCloseAlgorithmProvider(pAlg, 0);
        }
    };

    struct XBCryptHashDeleter final
    {
        void operator()(BCRYPT_HASH_HANDLE pHash) const noexcept
        {
            if (pHash)
                BCryptDestroyHash(pHash);
        }
    };

    struct XLocalAllocDeleter final
    {
        void operator()(HLOCAL pMem) const noexcept
        {
            if (pMem)
                LocalFree(pMem);
        }
    };

    struct XHeapAllocDeleter final
    {
        void operator()(void* pMem) const noexcept
        {
            if (pMem)
                HeapFree(GetProcessHeap(), 0, pMem);
        }
    };

    struct XRegKeyDeleter final
    {
        void operator()(HKEY pKey) const noexcept
        {
            if (pKey)
                RegCloseKey(pKey);
        }
    };

    struct XFindFileDeleter final
    {
        void operator()(HANDLE pHandle) const noexcept
        {
            if (pHandle && pHandle != INVALID_HANDLE_VALUE)
                FindClose(pHandle);
        }
    };

    struct XFileMappingDeleter final
    {
        void operator()(void* pView) const noexcept
        {
            if (pView)
                UnmapViewOfFile(pView);
        }
    };

    // ========================================================================
    // RAII Smart Pointer Types
    // ========================================================================

    using XUniqueHandle       = std::unique_ptr<void, XHandleDeleter>;
    using XUniqueCertStore    = std::unique_ptr<void, XCertStoreDeleter>;
    using XUniqueCertContext  = std::unique_ptr<const CERT_CONTEXT, XCertContextDeleter>;
    using XUniqueNCryptKey    = std::unique_ptr<std::remove_pointer_t<NCRYPT_KEY_HANDLE>, XNCryptKeyDeleter>;
    using XUniqueNCryptProv   = std::unique_ptr<std::remove_pointer_t<NCRYPT_PROV_HANDLE>, XNCryptProviderDeleter>;
    using XUniqueBCryptKey    = std::unique_ptr<std::remove_pointer_t<BCRYPT_KEY_HANDLE>, XBCryptKeyDeleter>;
    using XUniqueBCryptAlg    = std::unique_ptr<std::remove_pointer_t<BCRYPT_ALG_HANDLE>, XBCryptAlgDeleter>;
    using XUniqueBCryptHash   = std::unique_ptr<std::remove_pointer_t<BCRYPT_HASH_HANDLE>, XBCryptHashDeleter>;
    using XUniqueLocalAlloc   = std::unique_ptr<void, XLocalAllocDeleter>;
    using XUniqueHeapAlloc    = std::unique_ptr<void, XHeapAllocDeleter>;
    using XUniqueRegKey       = std::unique_ptr<std::remove_pointer_t<HKEY>, XRegKeyDeleter>;
    using XUniqueFindFile     = std::unique_ptr<void, XFindFileDeleter>;
    using XUniqueFileMapping  = std::unique_ptr<void, XFileMappingDeleter>;

    // ========================================================================
    // Utility Functions
    // ========================================================================

    [[nodiscard]] inline XUniqueHandle MakeUniqueHandle(HANDLE pHandle) noexcept
    {
        return XUniqueHandle(pHandle);
    }

    [[nodiscard]] inline XUniqueCertStore MakeUniqueCertStore(HCERTSTORE pStore) noexcept
    {
        return XUniqueCertStore(pStore);
    }

    [[nodiscard]] inline XUniqueCertContext MakeUniqueCertContext(PCCERT_CONTEXT pContext) noexcept
    {
        return XUniqueCertContext(pContext);
    }

    [[nodiscard]] inline XUniqueRegKey MakeUniqueRegKey(HKEY pKey) noexcept
    {
        return XUniqueRegKey(pKey);
    }

    // ========================================================================
    // GUID Utilities
    // ========================================================================

    [[nodiscard]] inline std::wstring GuidToString(const GUID& pGuid) noexcept
    {
        wchar_t buffer[39];
        swprintf_s(buffer, 39,
            L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
            pGuid.Data1, pGuid.Data2, pGuid.Data3,
            pGuid.Data4[0], pGuid.Data4[1], pGuid.Data4[2], pGuid.Data4[3],
            pGuid.Data4[4], pGuid.Data4[5], pGuid.Data4[6], pGuid.Data4[7]);
        return buffer;
    }

    [[nodiscard]] inline GUID GenerateGuid() noexcept
    {
        GUID guid{};
        (void)CoCreateGuid(&guid);
        return guid;
    }

    // ========================================================================
    // Error Code Utilities
    // ========================================================================

    [[nodiscard]] inline DWORD NtStatusToWin32(NTSTATUS pStatus) noexcept
    {
        if (NT_SUCCESS(pStatus))
            return ERROR_SUCCESS;

        switch (pStatus)
        {
        case STATUS_BUFFER_TOO_SMALL:
            return ERROR_INSUFFICIENT_BUFFER;
        case STATUS_INSUFFICIENT_RESOURCES:
        case STATUS_NO_MEMORY:
            return ERROR_NOT_ENOUGH_MEMORY;
        case STATUS_INVALID_PARAMETER:
            return ERROR_INVALID_PARAMETER;
        case STATUS_ACCESS_DENIED:
            return ERROR_ACCESS_DENIED;
        case STATUS_OBJECT_NAME_NOT_FOUND:
            return ERROR_NOT_FOUND;
        case STATUS_NOT_SUPPORTED:
            return ERROR_NOT_SUPPORTED;
        case STATUS_INTERNAL_ERROR:
            return ERROR_INTERNAL_ERROR;
        case STATUS_INVALID_SIGNATURE:
            return static_cast<DWORD>(NTE_BAD_SIGNATURE);
        default:
            return ERROR_GEN_FAILURE;
        }
    }

    // ========================================================================
    // Scope Guard
    // ========================================================================

    template<typename TFunc>
    class XScopeGuard final
    {
    public:
        explicit XScopeGuard(TFunc&& pFunc) noexcept
            : _Func(std::move(pFunc))
            , _Active(true)
        {
        }

        ~XScopeGuard()
        {
            if (_Active)
                _Func();
        }

        TOOTEGA_DISABLE_COPY_MOVE(XScopeGuard);

        void Dismiss() noexcept { _Active = false; }
        void Execute()
        {
            if (_Active)
            {
                _Active = false;
                _Func();
            }
        }

    private:
        TFunc _Func;
        bool  _Active;
    };

    template<typename TFunc>
    [[nodiscard]] XScopeGuard<TFunc> MakeScopeGuard(TFunc&& pFunc) noexcept
    {
        return XScopeGuard<TFunc>(std::forward<TFunc>(pFunc));
    }

} // namespace Tootega
