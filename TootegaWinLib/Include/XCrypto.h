#pragma once
// ============================================================================
// XCrypto.h - Cryptographic utilities using CNG/NCrypt
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XPlatform.h"
#include "XTypes.h"
#include "XResult.h"

namespace Tootega
{
    // ========================================================================
    // XCrypto - Cryptographic utility functions
    // ========================================================================

    class XCrypto final
    {
    public:
        XCrypto() = delete;

        [[nodiscard]] static XResult<std::vector<BYTE>> ComputeSHA256(const BYTE* pData, size_t pSize);
        [[nodiscard]] static XResult<std::vector<BYTE>> ComputeSHA256(std::span<const BYTE> pData);

        [[nodiscard]] static XResult<std::vector<BYTE>> ComputeSHA384(const BYTE* pData, size_t pSize);
        [[nodiscard]] static XResult<std::vector<BYTE>> ComputeSHA384(std::span<const BYTE> pData);

        [[nodiscard]] static XResult<std::vector<BYTE>> ComputeSHA512(const BYTE* pData, size_t pSize);
        [[nodiscard]] static XResult<std::vector<BYTE>> ComputeSHA512(std::span<const BYTE> pData);

        [[nodiscard]] static XResult<std::vector<BYTE>> ComputeHMACSHA256(
            std::span<const BYTE> pKey,
            std::span<const BYTE> pData);

        [[nodiscard]] static XResult<std::vector<BYTE>> GenerateRandomBytes(size_t pCount);

        [[nodiscard]] static XResult<std::wstring> GetCertificateThumbprint(PCCERT_CONTEXT pCertContext);
        [[nodiscard]] static XResult<std::wstring> GetCertificateSubject(PCCERT_CONTEXT pCertContext);
        [[nodiscard]] static XResult<std::wstring> GetCertificateIssuer(PCCERT_CONTEXT pCertContext);
        [[nodiscard]] static XResult<std::wstring> GetCertificateSerialNumber(PCCERT_CONTEXT pCertContext);

        [[nodiscard]] static bool IsCertificateValid(PCCERT_CONTEXT pCertContext);
        [[nodiscard]] static bool IsCertificateExpired(PCCERT_CONTEXT pCertContext);

        [[nodiscard]] static XResult<XUniqueCertContext> FindCertificateByThumbprint(
            HCERTSTORE    pStore,
            std::wstring_view pThumbprint);

        [[nodiscard]] static XResult<XUniqueCertContext> FindCertificateBySubject(
            HCERTSTORE    pStore,
            std::wstring_view pSubject);

        [[nodiscard]] static std::wstring ThumbprintToString(std::span<const BYTE> pThumbprint);
        [[nodiscard]] static XResult<std::vector<BYTE>> ThumbprintFromString(std::wstring_view pThumbprintStr);

    private:
        [[nodiscard]] static XResult<std::vector<BYTE>> ComputeHash(
            LPCWSTR pAlgorithm,
            const BYTE* pData,
            size_t pSize);
    };

} // namespace Tootega
