#pragma once
// ============================================================================
// XShellExtension.h - Windows Shell Extension Infrastructure
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================
//
// Provides the foundational infrastructure for building Windows Explorer 
// shell extensions including:
//   - COM object base class with reference counting
//   - Class factory for COM object creation
//   - DLL registration helpers
//   - Shell extension registration utilities
//
// ============================================================================

#include "../XPlatform.h"
#include "../XTypes.h"
#include "../XResult.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <propkey.h>
#include <propvarutil.h>
#include <propsys.h>
#include <thumbcache.h>
#include <commoncontrols.h>
#include <commctrl.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "comctl32.lib")

namespace Tootega::Shell
{
    // ========================================================================
    // Global Shell Extension State
    // ========================================================================

    class XShellModule final
    {
    public:
        static XShellModule& Instance() noexcept;

        TOOTEGA_DISABLE_COPY_MOVE(XShellModule);

        void Initialize(HMODULE pModule) noexcept;
        
        [[nodiscard]] HMODULE GetModule() const noexcept { return _Module; }
        [[nodiscard]] LONG GetRefCount() const noexcept { return _RefCount.load(std::memory_order_relaxed); }
        
        void AddRef() noexcept { _RefCount.fetch_add(1, std::memory_order_relaxed); }
        void Release() noexcept { _RefCount.fetch_sub(1, std::memory_order_relaxed); }
        
        [[nodiscard]] bool CanUnload() const noexcept { return _RefCount.load(std::memory_order_relaxed) == 0; }

        [[nodiscard]] std::wstring GetModulePath() const;

    private:
        XShellModule() = default;
        ~XShellModule() = default;

        HMODULE            _Module{nullptr};
        std::atomic<LONG>  _RefCount{0};
    };

    // ========================================================================
    // COM Object Base Class
    // ========================================================================

    template<typename... TInterfaces>
    class XComObject : public TInterfaces...
    {
    public:
        XComObject() : _RefCount(1)
        {
            XShellModule::Instance().AddRef();
        }

        virtual ~XComObject()
        {
            XShellModule::Instance().Release();
        }

        TOOTEGA_DISABLE_COPY_MOVE(XComObject);

        // IUnknown::AddRef
        STDMETHODIMP_(ULONG) AddRef() override
        {
            return InterlockedIncrement(&_RefCount);
        }

        // IUnknown::Release
        STDMETHODIMP_(ULONG) Release() override
        {
            LONG ref = InterlockedDecrement(&_RefCount);
            if (ref == 0)
                delete this;
            return ref;
        }

    protected:
        // Helper for QueryInterface implementation
        template<typename TInterface>
        bool TryQueryInterface(REFIID pRiid, void** pPpv)
        {
            if (IsEqualIID(pRiid, __uuidof(TInterface)))
            {
                *pPpv = static_cast<TInterface*>(this);
                AddRef();
                return true;
            }
            return false;
        }

        LONG _RefCount;
    };

    // ========================================================================
    // Smart COM Pointer
    // ========================================================================

    template<typename T>
    class XComPtr final
    {
    public:
        XComPtr() noexcept : _Ptr(nullptr) {}
        
        // Allow implicit construction from raw pointer for compatibility
        XComPtr(T* pPtr) noexcept : _Ptr(pPtr)
        {
            if (_Ptr)
                _Ptr->AddRef();
        }
        
        XComPtr(const XComPtr& pOther) noexcept : _Ptr(pOther._Ptr)
        {
            if (_Ptr)
                _Ptr->AddRef();
        }
        
        XComPtr(XComPtr&& pOther) noexcept : _Ptr(pOther._Ptr)
        {
            pOther._Ptr = nullptr;
        }
        
        ~XComPtr() noexcept
        {
            Release();
        }

        XComPtr& operator=(const XComPtr& pOther) noexcept
        {
            if (this != &pOther)
            {
                Release();
                _Ptr = pOther._Ptr;
                if (_Ptr)
                    _Ptr->AddRef();
            }
            return *this;
        }

        XComPtr& operator=(XComPtr&& pOther) noexcept
        {
            if (_Ptr != pOther._Ptr)
            {
                Release();
                _Ptr = pOther._Ptr;
                pOther._Ptr = nullptr;
            }
            return *this;
        }

        // Assignment from raw pointer for compatibility
        XComPtr& operator=(T* pPtr) noexcept
        {
            if (_Ptr != pPtr)
            {
                Release();
                _Ptr = pPtr;
                if (_Ptr)
                    _Ptr->AddRef();
            }
            return *this;
        }

        void Release() noexcept
        {
            if (_Ptr)
            {
                _Ptr->Release();
                _Ptr = nullptr;
            }
        }

        [[nodiscard]] T* Get() const noexcept { return _Ptr; }
        [[nodiscard]] T* operator->() const noexcept { return _Ptr; }
        [[nodiscard]] T** operator&() noexcept { Release(); return &_Ptr; }
        [[nodiscard]] explicit operator bool() const noexcept { return _Ptr != nullptr; }
        [[nodiscard]] operator T*() const noexcept { return _Ptr; }

        T* Detach() noexcept
        {
            T* tmp = _Ptr;
            _Ptr = nullptr;
            return tmp;
        }

        void Attach(T* pPtr) noexcept
        {
            Release();
            _Ptr = pPtr;
        }

        template<typename U>
        [[nodiscard]] HRESULT QueryInterface(U** pPpv) const noexcept
        {
            if (!_Ptr)
                return E_POINTER;
            return _Ptr->QueryInterface(__uuidof(U), reinterpret_cast<void**>(pPpv));
        }

    private:
        T* _Ptr;
    };

    // ========================================================================
    // Class Factory Template
    // ========================================================================

    template<typename TCreator>
    class XClassFactory : public IClassFactory
    {
    public:
        explicit XClassFactory(const CLSID& pClsid) noexcept
            : _RefCount(1)
            , _Clsid(pClsid)
        {
        }

        virtual ~XClassFactory() = default;

        // IUnknown
        STDMETHODIMP QueryInterface(REFIID pRiid, void** pPpv) override
        {
            if (!pPpv)
                return E_POINTER;

            if (IsEqualIID(pRiid, IID_IUnknown) || IsEqualIID(pRiid, IID_IClassFactory))
            {
                *pPpv = static_cast<IClassFactory*>(this);
                AddRef();
                return S_OK;
            }

            *pPpv = nullptr;
            return E_NOINTERFACE;
        }

        STDMETHODIMP_(ULONG) AddRef() override
        {
            return InterlockedIncrement(&_RefCount);
        }

        STDMETHODIMP_(ULONG) Release() override
        {
            LONG ref = InterlockedDecrement(&_RefCount);
            if (ref == 0)
                delete this;
            return ref;
        }

        // IClassFactory
        STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID pRiid, void** pPpv) override
        {
            if (!pPpv)
                return E_POINTER;
            *pPpv = nullptr;

            if (pUnkOuter)
                return CLASS_E_NOAGGREGATION;

            IUnknown* obj = TCreator::Create(_Clsid);
            if (!obj)
                return E_OUTOFMEMORY;

            HRESULT hr = obj->QueryInterface(pRiid, pPpv);
            obj->Release();
            return hr;
        }

        STDMETHODIMP LockServer(BOOL pLock) override
        {
            if (pLock)
                XShellModule::Instance().AddRef();
            else
                XShellModule::Instance().Release();
            return S_OK;
        }

    private:
        LONG  _RefCount;
        CLSID _Clsid;
    };

} // namespace Tootega::Shell

