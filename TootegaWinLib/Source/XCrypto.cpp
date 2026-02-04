// ============================================================================
// XCrypto.cpp - Cryptographic utilities implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XCrypto.h"
#include "XString.h"

namespace Tootega
{
    XResult<std::vector<BYTE>> XCrypto::ComputeHash(LPCWSTR pAlgorithm, const BYTE* pData, size_t pSize)
    {
        BCRYPT_ALG_HANDLE alg = nullptr;
        NTSTATUS status = BCryptOpenAlgorithmProvider(&alg, pAlgorithm, nullptr, 0);
        if (!NT_SUCCESS(status))
            return XError::FromNtStatus(status, L"BCryptOpenAlgorithmProvider failed");

        XUniqueBCryptAlg algGuard(alg);

        DWORD hashLength = 0;
        DWORD resultLen = 0;
        status = BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PBYTE>(&hashLength), sizeof(hashLength), &resultLen, 0);
        if (!NT_SUCCESS(status))
            return XError::FromNtStatus(status, L"BCryptGetProperty BCRYPT_HASH_LENGTH failed");

        BCRYPT_HASH_HANDLE hash = nullptr;
        status = BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
        if (!NT_SUCCESS(status))
            return XError::FromNtStatus(status, L"BCryptCreateHash failed");

        XUniqueBCryptHash hashGuard(hash);

        status = BCryptHashData(hash, const_cast<PUCHAR>(pData), static_cast<ULONG>(pSize), 0);
        if (!NT_SUCCESS(status))
            return XError::FromNtStatus(status, L"BCryptHashData failed");

        std::vector<BYTE> result(hashLength);
        status = BCryptFinishHash(hash, result.data(), hashLength, 0);
        if (!NT_SUCCESS(status))
            return XError::FromNtStatus(status, L"BCryptFinishHash failed");

        return result;
    }

    XResult<std::vector<BYTE>> XCrypto::ComputeSHA256(const BYTE* pData, size_t pSize)
    {
        return ComputeHash(BCRYPT_SHA256_ALGORITHM, pData, pSize);
    }

    XResult<std::vector<BYTE>> XCrypto::ComputeSHA256(std::span<const BYTE> pData)
    {
        return ComputeSHA256(pData.data(), pData.size());
    }

    XResult<std::vector<BYTE>> XCrypto::ComputeSHA384(const BYTE* pData, size_t pSize)
    {
        return ComputeHash(BCRYPT_SHA384_ALGORITHM, pData, pSize);
    }

    XResult<std::vector<BYTE>> XCrypto::ComputeSHA384(std::span<const BYTE> pData)
    {
        return ComputeSHA384(pData.data(), pData.size());
    }

    XResult<std::vector<BYTE>> XCrypto::ComputeSHA512(const BYTE* pData, size_t pSize)
    {
        return ComputeHash(BCRYPT_SHA512_ALGORITHM, pData, pSize);
    }

    XResult<std::vector<BYTE>> XCrypto::ComputeSHA512(std::span<const BYTE> pData)
    {
        return ComputeSHA512(pData.data(), pData.size());
    }

    XResult<std::vector<BYTE>> XCrypto::ComputeHMACSHA256(std::span<const BYTE> pKey, std::span<const BYTE> pData)
    {
        BCRYPT_ALG_HANDLE alg = nullptr;
        NTSTATUS status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
        if (!NT_SUCCESS(status))
            return XError::FromNtStatus(status, L"BCryptOpenAlgorithmProvider HMAC failed");

        XUniqueBCryptAlg algGuard(alg);

        DWORD hashLength = 0;
        DWORD resultLen = 0;
        status = BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PBYTE>(&hashLength), sizeof(hashLength), &resultLen, 0);
        if (!NT_SUCCESS(status))
            return XError::FromNtStatus(status, L"BCryptGetProperty BCRYPT_HASH_LENGTH failed");

        BCRYPT_HASH_HANDLE hash = nullptr;
        status = BCryptCreateHash(alg, &hash, nullptr, 0, const_cast<PUCHAR>(pKey.data()), static_cast<ULONG>(pKey.size()), 0);
        if (!NT_SUCCESS(status))
            return XError::FromNtStatus(status, L"BCryptCreateHash HMAC failed");

        XUniqueBCryptHash hashGuard(hash);

        status = BCryptHashData(hash, const_cast<PUCHAR>(pData.data()), static_cast<ULONG>(pData.size()), 0);
        if (!NT_SUCCESS(status))
            return XError::FromNtStatus(status, L"BCryptHashData failed");

        std::vector<BYTE> result(hashLength);
        status = BCryptFinishHash(hash, result.data(), hashLength, 0);
        if (!NT_SUCCESS(status))
            return XError::FromNtStatus(status, L"BCryptFinishHash failed");

        return result;
    }

    XResult<std::vector<BYTE>> XCrypto::GenerateRandomBytes(size_t pCount)
    {
        if (pCount == 0)
            return std::vector<BYTE>{};

        std::vector<BYTE> result(pCount);
        NTSTATUS status = BCryptGenRandom(nullptr, result.data(), static_cast<ULONG>(pCount), BCRYPT_USE_SYSTEM_PREFERRED_RNG);

        if (!NT_SUCCESS(status))
            return XError::FromNtStatus(status, L"BCryptGenRandom failed");

        return result;
    }

    XResult<std::wstring> XCrypto::GetCertificateThumbprint(PCCERT_CONTEXT pCertContext)
    {
        if (!pCertContext)
            return XError::Application(1, L"Certificate context is null");

        BYTE hash[20]{};
        DWORD hashSize = sizeof(hash);

        if (!CertGetCertificateContextProperty(pCertContext, CERT_SHA1_HASH_PROP_ID, hash, &hashSize))
            return XError::FromLastError(L"CertGetCertificateContextProperty SHA1 failed");

        return ThumbprintToString({hash, hashSize});
    }

    XResult<std::wstring> XCrypto::GetCertificateSubject(PCCERT_CONTEXT pCertContext)
    {
        if (!pCertContext)
            return XError::Application(1, L"Certificate context is null");

        DWORD size = CertNameToStrW(
            X509_ASN_ENCODING,
            &pCertContext->pCertInfo->Subject,
            CERT_X500_NAME_STR,
            nullptr,
            0);

        if (size <= 1)
            return std::wstring{};

        std::wstring result(size - 1, L'\0');
        CertNameToStrW(
            X509_ASN_ENCODING,
            &pCertContext->pCertInfo->Subject,
            CERT_X500_NAME_STR,
            result.data(),
            size);

        return result;
    }

    XResult<std::wstring> XCrypto::GetCertificateIssuer(PCCERT_CONTEXT pCertContext)
    {
        if (!pCertContext)
            return XError::Application(1, L"Certificate context is null");

        DWORD size = CertNameToStrW(
            X509_ASN_ENCODING,
            &pCertContext->pCertInfo->Issuer,
            CERT_X500_NAME_STR,
            nullptr,
            0);

        if (size <= 1)
            return std::wstring{};

        std::wstring result(size - 1, L'\0');
        CertNameToStrW(
            X509_ASN_ENCODING,
            &pCertContext->pCertInfo->Issuer,
            CERT_X500_NAME_STR,
            result.data(),
            size);

        return result;
    }

    XResult<std::wstring> XCrypto::GetCertificateSerialNumber(PCCERT_CONTEXT pCertContext)
    {
        if (!pCertContext)
            return XError::Application(1, L"Certificate context is null");

        const CRYPT_INTEGER_BLOB& serial = pCertContext->pCertInfo->SerialNumber;

        std::wstring result;
        result.reserve(serial.cbData * 2);

        for (DWORD i = serial.cbData; i > 0; --i)
            result += std::format(L"{:02X}", serial.pbData[i - 1]);

        return result;
    }

    bool XCrypto::IsCertificateValid(PCCERT_CONTEXT pCertContext)
    {
        if (!pCertContext)
            return false;

        return CertVerifyTimeValidity(nullptr, pCertContext->pCertInfo) == 0;
    }

    bool XCrypto::IsCertificateExpired(PCCERT_CONTEXT pCertContext)
    {
        if (!pCertContext)
            return true;

        return CertVerifyTimeValidity(nullptr, pCertContext->pCertInfo) == 1;
    }

    XResult<XUniqueCertContext> XCrypto::FindCertificateByThumbprint(HCERTSTORE pStore, std::wstring_view pThumbprint)
    {
        auto hashResult = ThumbprintFromString(pThumbprint);
        if (!hashResult)
            return hashResult.Error();

        CRYPT_HASH_BLOB hashBlob{};
        hashBlob.cbData = static_cast<DWORD>(hashResult->size());
        hashBlob.pbData = hashResult->data();

        PCCERT_CONTEXT cert = CertFindCertificateInStore(
            pStore,
            X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
            0,
            CERT_FIND_SHA1_HASH,
            &hashBlob,
            nullptr);

        if (!cert)
            return XError::FromLastError(L"CertFindCertificateInStore by thumbprint failed");

        return MakeUniqueCertContext(cert);
    }

    XResult<XUniqueCertContext> XCrypto::FindCertificateBySubject(HCERTSTORE pStore, std::wstring_view pSubject)
    {
        std::wstring subject(pSubject);

        PCCERT_CONTEXT cert = CertFindCertificateInStore(
            pStore,
            X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
            0,
            CERT_FIND_SUBJECT_STR,
            subject.c_str(),
            nullptr);

        if (!cert)
            return XError::FromLastError(L"CertFindCertificateInStore by subject failed");

        return MakeUniqueCertContext(cert);
    }

    std::wstring XCrypto::ThumbprintToString(std::span<const BYTE> pThumbprint)
    {
        return XString::ToHex(pThumbprint);
    }

    XResult<std::vector<BYTE>> XCrypto::ThumbprintFromString(std::wstring_view pThumbprintStr)
    {
        std::wstring cleaned;
        cleaned.reserve(pThumbprintStr.size());

        for (wchar_t c : pThumbprintStr)
        {
            if (c != L' ' && c != L':' && c != L'-')
                cleaned += c;
        }

        return XString::FromHex(cleaned);
    }

} // namespace Tootega
