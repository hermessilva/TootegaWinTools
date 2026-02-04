// ============================================================================
// XMemory.cpp - Memory management utilities implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XMemory.h"

namespace Tootega
{
    void* XMemory::AllocateHeap(size_t pSize) noexcept
    {
        if (pSize == 0)
            return nullptr;

        return HeapAlloc(GetProcessHeap(), 0, pSize);
    }

    void* XMemory::AllocateHeapZeroed(size_t pSize) noexcept
    {
        if (pSize == 0)
            return nullptr;

        return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, pSize);
    }

    void* XMemory::ReallocateHeap(void* pPtr, size_t pNewSize) noexcept
    {
        if (pPtr == nullptr)
            return AllocateHeap(pNewSize);

        if (pNewSize == 0)
        {
            FreeHeap(pPtr);
            return nullptr;
        }

        return HeapReAlloc(GetProcessHeap(), 0, pPtr, pNewSize);
    }

    void XMemory::FreeHeap(void* pPtr) noexcept
    {
        if (pPtr != nullptr)
            HeapFree(GetProcessHeap(), 0, pPtr);
    }

    void XMemory::SecureZero(void* pPtr, size_t pSize) noexcept
    {
        if (pPtr != nullptr && pSize > 0)
            SecureZeroMemory(pPtr, pSize);
    }

    bool XMemory::IsAligned(const void* pPtr, size_t pAlignment) noexcept
    {
        return (reinterpret_cast<uintptr_t>(pPtr) & (pAlignment - 1)) == 0;
    }

    // ========================================================================
    // XSecureBuffer Implementation
    // ========================================================================

    XSecureBuffer::XSecureBuffer(size_t pSize)
    {
        if (pSize > 0)
        {
            _Data = static_cast<BYTE*>(XMemory::AllocateHeapZeroed(pSize));
            if (_Data)
                _Size = pSize;
        }
    }

    XSecureBuffer::XSecureBuffer(const BYTE* pData, size_t pSize)
    {
        if (pData && pSize > 0)
        {
            _Data = static_cast<BYTE*>(XMemory::AllocateHeap(pSize));
            if (_Data)
            {
                memcpy(_Data, pData, pSize);
                _Size = pSize;
            }
        }
    }

    XSecureBuffer::XSecureBuffer(std::span<const BYTE> pData)
        : XSecureBuffer(pData.data(), pData.size())
    {
    }

    XSecureBuffer::~XSecureBuffer() noexcept
    {
        Release();
    }

    XSecureBuffer::XSecureBuffer(XSecureBuffer&& pOther) noexcept
        : _Data(pOther._Data)
        , _Size(pOther._Size)
    {
        pOther._Data = nullptr;
        pOther._Size = 0;
    }

    XSecureBuffer& XSecureBuffer::operator=(XSecureBuffer&& pOther) noexcept
    {
        if (this != &pOther)
        {
            Release();
            _Data = pOther._Data;
            _Size = pOther._Size;
            pOther._Data = nullptr;
            pOther._Size = 0;
        }
        return *this;
    }

    bool XSecureBuffer::Resize(size_t pNewSize)
    {
        if (pNewSize == 0)
        {
            Clear();
            return true;
        }

        if (pNewSize == _Size)
            return true;

        BYTE* newData = static_cast<BYTE*>(XMemory::AllocateHeapZeroed(pNewSize));
        if (!newData)
            return false;

        if (_Data && _Size > 0)
        {
            size_t copySize = (pNewSize < _Size) ? pNewSize : _Size;
            memcpy(newData, _Data, copySize);
        }

        Release();
        _Data = newData;
        _Size = pNewSize;
        return true;
    }

    void XSecureBuffer::Clear() noexcept
    {
        Release();
    }

    void XSecureBuffer::Release() noexcept
    {
        if (_Data)
        {
            XMemory::SecureZero(_Data, _Size);
            XMemory::FreeHeap(_Data);
            _Data = nullptr;
        }
        _Size = 0;
    }

} // namespace Tootega
