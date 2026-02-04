#pragma once
// ============================================================================
// XMemory.h - Memory management utilities
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XPlatform.h"
#include "XTypes.h"

namespace Tootega
{
    // ========================================================================
    // XMemory - Memory utility functions
    // ========================================================================

    class XMemory final
    {
    public:
        XMemory() = delete;

        [[nodiscard]] static void* AllocateHeap(size_t pSize) noexcept;
        [[nodiscard]] static void* AllocateHeapZeroed(size_t pSize) noexcept;
        [[nodiscard]] static void* ReallocateHeap(void* pPtr, size_t pNewSize) noexcept;
        static void FreeHeap(void* pPtr) noexcept;

        static void SecureZero(void* pPtr, size_t pSize) noexcept;

        template<typename T>
        static void SecureZero(T& pValue) noexcept
        {
            SecureZero(&pValue, sizeof(T));
        }

        template<typename T, size_t N>
        static void SecureZero(T (&pArray)[N]) noexcept
        {
            SecureZero(pArray, sizeof(pArray));
        }

        template<typename T>
        [[nodiscard]] static T* AllocateArray(size_t pCount) noexcept
        {
            return static_cast<T*>(AllocateHeapZeroed(pCount * sizeof(T)));
        }

        template<typename T>
        static void FreeArray(T* pArray) noexcept
        {
            FreeHeap(pArray);
        }

        [[nodiscard]] static bool IsAligned(const void* pPtr, size_t pAlignment) noexcept;

        [[nodiscard]] static constexpr size_t AlignUp(size_t pValue, size_t pAlignment) noexcept
        {
            return (pValue + pAlignment - 1) & ~(pAlignment - 1);
        }

        [[nodiscard]] static constexpr size_t AlignDown(size_t pValue, size_t pAlignment) noexcept
        {
            return pValue & ~(pAlignment - 1);
        }
    };

    // ========================================================================
    // XSecureBuffer - Secure memory buffer that zeros on destruction
    // ========================================================================

    class XSecureBuffer final
    {
    public:
        XSecureBuffer() noexcept = default;
        explicit XSecureBuffer(size_t pSize);
        XSecureBuffer(const BYTE* pData, size_t pSize);
        XSecureBuffer(std::span<const BYTE> pData);

        ~XSecureBuffer() noexcept;

        XSecureBuffer(XSecureBuffer&& pOther) noexcept;
        XSecureBuffer& operator=(XSecureBuffer&& pOther) noexcept;

        TOOTEGA_DISABLE_COPY(XSecureBuffer);

        [[nodiscard]] bool Resize(size_t pNewSize);
        void Clear() noexcept;

        [[nodiscard]] BYTE* Data() noexcept { return _Data; }
        [[nodiscard]] const BYTE* Data() const noexcept { return _Data; }
        [[nodiscard]] size_t Size() const noexcept { return _Size; }
        [[nodiscard]] bool Empty() const noexcept { return _Size == 0; }

        [[nodiscard]] BYTE& operator[](size_t pIndex) noexcept { return _Data[pIndex]; }
        [[nodiscard]] const BYTE& operator[](size_t pIndex) const noexcept { return _Data[pIndex]; }

        [[nodiscard]] std::span<BYTE> Span() noexcept { return {_Data, _Size}; }
        [[nodiscard]] std::span<const BYTE> Span() const noexcept { return {_Data, _Size}; }

        [[nodiscard]] BYTE* begin() noexcept { return _Data; }
        [[nodiscard]] BYTE* end() noexcept { return _Data + _Size; }
        [[nodiscard]] const BYTE* begin() const noexcept { return _Data; }
        [[nodiscard]] const BYTE* end() const noexcept { return _Data + _Size; }

    private:
        void Release() noexcept;

        BYTE*  _Data{nullptr};
        size_t _Size{0};
    };

} // namespace Tootega
