#pragma once
// ============================================================================
// XResult.h - Result type for robust error handling
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XPlatform.h"

namespace Tootega
{
    // ========================================================================
    // XError - Structured error information
    // ========================================================================

    class XError final
    {
    public:
        enum class Category
        {
            None,
            Win32,
            NtStatus,
            Security,
            Application
        };

        constexpr XError() noexcept = default;

        XError(Category pCategory, DWORD pCode, std::wstring pMessage = {}) noexcept
            : _Category(pCategory)
            , _Code(pCode)
            , _Message(std::move(pMessage))
        {
        }

        [[nodiscard]] static XError FromWin32(DWORD pCode, std::wstring pMessage = {}) noexcept
        {
            return XError(Category::Win32, pCode, std::move(pMessage));
        }

        [[nodiscard]] static XError FromLastError(std::wstring pMessage = {}) noexcept
        {
            return FromWin32(GetLastError(), std::move(pMessage));
        }

        [[nodiscard]] static XError FromNtStatus(NTSTATUS pStatus, std::wstring pMessage = {}) noexcept
        {
            return XError(Category::NtStatus, static_cast<DWORD>(pStatus), std::move(pMessage));
        }

        [[nodiscard]] static XError FromSecurity(SECURITY_STATUS pStatus, std::wstring pMessage = {}) noexcept
        {
            return XError(Category::Security, static_cast<DWORD>(pStatus), std::move(pMessage));
        }

        [[nodiscard]] static XError Application(DWORD pCode, std::wstring pMessage) noexcept
        {
            return XError(Category::Application, pCode, std::move(pMessage));
        }

        [[nodiscard]] bool IsSuccess() const noexcept
        {
            return _Category == Category::None || _Code == 0;
        }

        [[nodiscard]] bool IsError() const noexcept
        {
            return !IsSuccess();
        }

        [[nodiscard]] Category GetCategory() const noexcept { return _Category; }
        [[nodiscard]] DWORD GetCode() const noexcept { return _Code; }
        [[nodiscard]] const std::wstring& GetMessage() const noexcept { return _Message; }

        [[nodiscard]] std::wstring FormatMessage() const
        {
            if (_Category == Category::None)
                return L"Success";

            std::wstring result;
            result.reserve(256);

            switch (_Category)
            {
            case Category::Win32:
                result = std::format(L"Win32 Error 0x{:08X}", _Code);
                break;
            case Category::NtStatus:
                result = std::format(L"NTSTATUS 0x{:08X}", _Code);
                break;
            case Category::Security:
                result = std::format(L"Security Error 0x{:08X}", _Code);
                break;
            case Category::Application:
                result = std::format(L"Application Error {}", _Code);
                break;
            default:
                result = std::format(L"Unknown Error 0x{:08X}", _Code);
                break;
            }

            if (!_Message.empty())
            {
                result += L": ";
                result += _Message;
            }

            return result;
        }

        explicit operator bool() const noexcept { return IsError(); }

    private:
        Category     _Category{Category::None};
        DWORD        _Code{0};
        std::wstring _Message;
    };

    // ========================================================================
    // XResult<T> - Result type with value or error
    // ========================================================================

    template<typename T>
    class XResult final
    {
    public:
        XResult(const T& pValue)
            : _HasValue(true)
        {
            new (&_Storage) T(pValue);
        }

        XResult(T&& pValue) noexcept(std::is_nothrow_move_constructible_v<T>)
            : _HasValue(true)
        {
            new (&_Storage) T(std::move(pValue));
        }

        XResult(const XError& pError)
            : _Error(pError)
            , _HasValue(false)
        {
        }

        XResult(XError&& pError) noexcept
            : _Error(std::move(pError))
            , _HasValue(false)
        {
        }

        ~XResult()
        {
            if (_HasValue)
                reinterpret_cast<T*>(&_Storage)->~T();
        }

        XResult(const XResult& pOther)
            : _Error(pOther._Error)
            , _HasValue(pOther._HasValue)
        {
            if (_HasValue)
                new (&_Storage) T(*reinterpret_cast<const T*>(&pOther._Storage));
        }

        XResult(XResult&& pOther) noexcept(std::is_nothrow_move_constructible_v<T>)
            : _Error(std::move(pOther._Error))
            , _HasValue(pOther._HasValue)
        {
            if (_HasValue)
                new (&_Storage) T(std::move(*reinterpret_cast<T*>(&pOther._Storage)));
        }

        XResult& operator=(const XResult& pOther)
        {
            if (this != &pOther)
            {
                if (_HasValue)
                    reinterpret_cast<T*>(&_Storage)->~T();

                _HasValue = pOther._HasValue;
                _Error = pOther._Error;
                if (_HasValue)
                    new (&_Storage) T(*reinterpret_cast<const T*>(&pOther._Storage));
            }
            return *this;
        }

        XResult& operator=(XResult&& pOther) noexcept(std::is_nothrow_move_constructible_v<T>)
        {
            if (this != &pOther)
            {
                if (_HasValue)
                    reinterpret_cast<T*>(&_Storage)->~T();

                _HasValue = pOther._HasValue;
                _Error = std::move(pOther._Error);
                if (_HasValue)
                    new (&_Storage) T(std::move(*reinterpret_cast<T*>(&pOther._Storage)));
            }
            return *this;
        }

        [[nodiscard]] bool HasValue() const noexcept { return _HasValue; }
        [[nodiscard]] bool HasError() const noexcept { return !_HasValue; }

        [[nodiscard]] explicit operator bool() const noexcept { return _HasValue; }

        [[nodiscard]] T& Value() &
        {
            if (!_HasValue)
                throw std::runtime_error("XResult has no value");
            return *reinterpret_cast<T*>(&_Storage);
        }

        [[nodiscard]] const T& Value() const&
        {
            if (!_HasValue)
                throw std::runtime_error("XResult has no value");
            return *reinterpret_cast<const T*>(&_Storage);
        }

        [[nodiscard]] T&& Value() &&
        {
            if (!_HasValue)
                throw std::runtime_error("XResult has no value");
            return std::move(*reinterpret_cast<T*>(&_Storage));
        }

        [[nodiscard]] T ValueOr(T pDefault) const&
        {
            return _HasValue ? *reinterpret_cast<const T*>(&_Storage) : std::move(pDefault);
        }

        [[nodiscard]] const XError& Error() const noexcept { return _Error; }

        [[nodiscard]] T* operator->() noexcept
        {
            return _HasValue ? reinterpret_cast<T*>(&_Storage) : nullptr;
        }

        [[nodiscard]] const T* operator->() const noexcept
        {
            return _HasValue ? reinterpret_cast<const T*>(&_Storage) : nullptr;
        }

        [[nodiscard]] T& operator*() & { return Value(); }
        [[nodiscard]] const T& operator*() const& { return Value(); }
        [[nodiscard]] T&& operator*() && { return std::move(Value()); }

    private:
        alignas(T) char _Storage[sizeof(T)];
        XError          _Error;
        bool            _HasValue;
    };

    // ========================================================================
    // XResult<void> - Specialization for void
    // ========================================================================

    template<>
    class XResult<void> final
    {
    public:
        XResult() noexcept
            : _HasValue(true)
        {
        }

        XResult(const XError& pError) noexcept
            : _Error(pError)
            , _HasValue(false)
        {
        }

        [[nodiscard]] static XResult Success() noexcept { return XResult(); }
        [[nodiscard]] static XResult Failure(const XError& pError) noexcept { return XResult(pError); }

        [[nodiscard]] constexpr bool HasValue() const noexcept { return _HasValue; }
        [[nodiscard]] constexpr bool HasError() const noexcept { return !_HasValue; }
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return _HasValue; }
        [[nodiscard]] constexpr const XError& Error() const noexcept { return _Error; }

    private:
        XError _Error;
        bool   _HasValue;
    };

    // ========================================================================
    // Helper Macros
    // ========================================================================

#define TOOTEGA_TRY(expr) \
    do { \
        auto _result = (expr); \
        if (!_result) \
            return _result.Error(); \
    } while (false)

#define TOOTEGA_TRY_WIN32(expr) \
    do { \
        DWORD _err = (expr); \
        if (_err != ERROR_SUCCESS) \
            return Tootega::XError::FromWin32(_err); \
    } while (false)

#define TOOTEGA_TRY_NTSTATUS(expr) \
    do { \
        NTSTATUS _status = (expr); \
        if (!NT_SUCCESS(_status)) \
            return Tootega::XError::FromNtStatus(_status); \
    } while (false)

} // namespace Tootega
