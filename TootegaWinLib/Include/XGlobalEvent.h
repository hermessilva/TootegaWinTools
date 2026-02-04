#pragma once
// ============================================================================
// XGlobalEvent.h - Global Named Events for cross-process synchronization
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================
//
// Provides management of global named events using Windows kernel objects.
// Global events use the "Global\" prefix for system-wide visibility across
// all sessions, enabling cross-process and cross-session synchronization.
//
// Features:
//   - Create, open, signal, reset, and wait on global events
//   - Support for both manual-reset and auto-reset events
//   - Async wait with timeout and cancellation support
//   - Multi-event monitoring (WaitAny/WaitAll)
//   - Continuous background watching with callbacks
//   - Proper security descriptors for cross-session access
//
// ============================================================================

#include "XPlatform.h"
#include "XResult.h"
#include "XTypes.h"
#include <aclapi.h>
#include <thread>

namespace Tootega
{
    // ========================================================================
    // XGlobalEventAccessRights - Access rights for global events
    // ========================================================================

    enum class XGlobalEventAccessRights : DWORD
    {
        None = 0,
        Synchronize = SYNCHRONIZE,
        Modify = EVENT_MODIFY_STATE,
        FullControl = EVENT_ALL_ACCESS
    };

    inline XGlobalEventAccessRights operator|(XGlobalEventAccessRights pLeft, XGlobalEventAccessRights pRight) noexcept
    {
        return static_cast<XGlobalEventAccessRights>(static_cast<DWORD>(pLeft) | static_cast<DWORD>(pRight));
    }

    inline XGlobalEventAccessRights operator&(XGlobalEventAccessRights pLeft, XGlobalEventAccessRights pRight) noexcept
    {
        return static_cast<XGlobalEventAccessRights>(static_cast<DWORD>(pLeft) & static_cast<DWORD>(pRight));
    }

    inline bool HasFlag(XGlobalEventAccessRights pValue, XGlobalEventAccessRights pFlag) noexcept
    {
        return (static_cast<DWORD>(pValue) & static_cast<DWORD>(pFlag)) == static_cast<DWORD>(pFlag);
    }

    // ========================================================================
    // XGlobalEventWaitStatus - Result status of wait operations
    // ========================================================================

    enum class XGlobalEventWaitStatus
    {
        Signaled,
        Timeout,
        Abandoned,
        Failed,
        Canceled
    };

    // ========================================================================
    // XGlobalEventResult - Result of wait operations
    // ========================================================================

    struct XGlobalEventResult final
    {
        XGlobalEventWaitStatus Status = XGlobalEventWaitStatus::Timeout;
        int SignaledIndex = -1;
        std::wstring EventName;
        DWORD ErrorCode = 0;

        [[nodiscard]] bool IsSignaled() const noexcept { return Status == XGlobalEventWaitStatus::Signaled; }
        [[nodiscard]] bool IsTimeout() const noexcept { return Status == XGlobalEventWaitStatus::Timeout; }
        [[nodiscard]] bool IsAbandoned() const noexcept { return Status == XGlobalEventWaitStatus::Abandoned; }
        [[nodiscard]] bool IsFailed() const noexcept { return Status == XGlobalEventWaitStatus::Failed; }
        [[nodiscard]] bool IsCanceled() const noexcept { return Status == XGlobalEventWaitStatus::Canceled; }

        static XGlobalEventResult Signaled(int pIndex = 0, std::wstring pEventName = {}) noexcept
        {
            return { XGlobalEventWaitStatus::Signaled, pIndex, std::move(pEventName), 0 };
        }

        static XGlobalEventResult Timeout() noexcept
        {
            return { XGlobalEventWaitStatus::Timeout, -1, {}, 0 };
        }

        static XGlobalEventResult Abandoned(int pIndex = 0, std::wstring pEventName = {}) noexcept
        {
            return { XGlobalEventWaitStatus::Abandoned, pIndex, std::move(pEventName), 0 };
        }

        static XGlobalEventResult Failed(DWORD pErrorCode) noexcept
        {
            return { XGlobalEventWaitStatus::Failed, -1, {}, pErrorCode };
        }

        static XGlobalEventResult Canceled() noexcept
        {
            return { XGlobalEventWaitStatus::Canceled, -1, {}, 0 };
        }
    };

    // ========================================================================
    // XGlobalEventOptions - Configuration for creating global events
    // ========================================================================

    struct XGlobalEventOptions final
    {
        std::wstring Name;
        bool ManualReset = true;
        bool InitialState = false;
        XGlobalEventAccessRights AccessRights = XGlobalEventAccessRights::FullControl;

        explicit XGlobalEventOptions(std::wstring_view pName)
            : Name(pName)
        {
        }

        static XGlobalEventOptions AutoReset(std::wstring_view pName)
        {
            XGlobalEventOptions opts(pName);
            opts.ManualReset = false;
            opts.InitialState = false;
            return opts;
        }

        static XGlobalEventOptions ManualResetEvent(std::wstring_view pName)
        {
            XGlobalEventOptions opts(pName);
            opts.ManualReset = true;
            opts.InitialState = false;
            return opts;
        }

        static XGlobalEventOptions Signaled(std::wstring_view pName)
        {
            XGlobalEventOptions opts(pName);
            opts.ManualReset = true;
            opts.InitialState = true;
            return opts;
        }
    };

    // ========================================================================
    // XGlobalEvent - Main class for global named events
    // ========================================================================

    class XGlobalEvent final
    {
    public:
        static constexpr std::wstring_view GlobalPrefix = L"Global\\";
        static constexpr std::wstring_view LocalPrefix = L"Local\\";

        TOOTEGA_DISABLE_COPY(XGlobalEvent);

        XGlobalEvent(XGlobalEvent&& pOther) noexcept
            : _Handle(std::move(pOther._Handle))
            , _Name(std::move(pOther._Name))
            , _IsCreatedNew(pOther._IsCreatedNew)
        {
            pOther._IsCreatedNew = false;
        }

        XGlobalEvent& operator=(XGlobalEvent&& pOther) noexcept
        {
            if (this != &pOther)
            {
                _Handle = std::move(pOther._Handle);
                _Name = std::move(pOther._Name);
                _IsCreatedNew = pOther._IsCreatedNew;
                pOther._IsCreatedNew = false;
            }
            return *this;
        }

        ~XGlobalEvent() = default;

        [[nodiscard]] const std::wstring& GetName() const noexcept { return _Name; }
        [[nodiscard]] std::wstring GetFullName() const { return std::wstring(GlobalPrefix) + _Name; }
        [[nodiscard]] bool IsCreatedNew() const noexcept { return _IsCreatedNew; }
        [[nodiscard]] bool IsValid() const noexcept { return _Handle.get() != nullptr; }
        [[nodiscard]] HANDLE GetHandle() const noexcept { return _Handle.get(); }

        [[nodiscard]] bool IsSet() const noexcept
        {
            if (!IsValid())
                return false;
            return WaitForSingleObject(_Handle.get(), 0) == WAIT_OBJECT_0;
        }

        // ====================================================================
        // Factory Methods
        // ====================================================================

        [[nodiscard]] static XResult<XGlobalEvent> Create(const XGlobalEventOptions& pOptions);
        [[nodiscard]] static XResult<XGlobalEvent> Create(std::wstring_view pName, bool pManualReset = true, bool pInitialState = false);
        [[nodiscard]] static XResult<XGlobalEvent> Open(std::wstring_view pName, XGlobalEventAccessRights pAccess = XGlobalEventAccessRights::FullControl);
        [[nodiscard]] static std::optional<XGlobalEvent> TryOpen(std::wstring_view pName, XGlobalEventAccessRights pAccess = XGlobalEventAccessRights::FullControl) noexcept;
        [[nodiscard]] static bool Exists(std::wstring_view pName) noexcept;

        // ====================================================================
        // Static Signal Methods
        // ====================================================================

        [[nodiscard]] static XResult<void> Signal(std::wstring_view pName);
        [[nodiscard]] static bool TrySignal(std::wstring_view pName) noexcept;

        // ====================================================================
        // Instance Methods
        // ====================================================================

        [[nodiscard]] XResult<void> Set();
        [[nodiscard]] XResult<void> Reset();
        [[nodiscard]] bool Wait(DWORD pTimeoutMs = INFINITE) const noexcept;
        [[nodiscard]] XGlobalEventResult WaitForSignal(DWORD pTimeoutMs = INFINITE) const noexcept;
        void Pulse();

    private:
        XGlobalEvent(XUniqueHandle pHandle, std::wstring pName, bool pIsCreatedNew) noexcept
            : _Handle(std::move(pHandle))
            , _Name(std::move(pName))
            , _IsCreatedNew(pIsCreatedNew)
        {
        }

        [[nodiscard]] static XResult<SECURITY_ATTRIBUTES> CreateSecurityAttributes(
            XGlobalEventAccessRights pRights,
            SECURITY_DESCRIPTOR& pSD,
            EXPLICIT_ACCESSW& pEA,
            PACL& pAcl);

        XUniqueHandle _Handle;
        std::wstring _Name;
        bool _IsCreatedNew = false;
    };

    // ========================================================================
    // XGlobalEventMonitor - Monitor multiple global events
    // ========================================================================

    class XGlobalEventMonitor final
    {
    public:
        TOOTEGA_DISABLE_COPY_MOVE(XGlobalEventMonitor);

        XGlobalEventMonitor() = default;
        ~XGlobalEventMonitor() = default;

        [[nodiscard]] size_t GetCount() const noexcept { return _Events.size(); }
        [[nodiscard]] bool IsEmpty() const noexcept { return _Events.empty(); }

        [[nodiscard]] std::vector<std::wstring> GetEventNames() const;
        [[nodiscard]] XGlobalEvent* Get(std::wstring_view pName) noexcept;
        [[nodiscard]] const XGlobalEvent* Get(std::wstring_view pName) const noexcept;
        [[nodiscard]] bool Contains(std::wstring_view pName) const noexcept;

        XResult<XGlobalEvent*> Add(std::wstring_view pName, bool pManualReset = true, bool pInitialState = false);
        XResult<XGlobalEvent*> Add(const XGlobalEventOptions& pOptions);
        bool Remove(std::wstring_view pName);
        void Clear();

        void SetAll();
        void ResetAll();
        XResult<void> Set(std::wstring_view pName);
        XResult<void> Reset(std::wstring_view pName);

        [[nodiscard]] XGlobalEventResult WaitAny(DWORD pTimeoutMs = INFINITE) const;
        [[nodiscard]] bool WaitAll(DWORD pTimeoutMs = INFINITE) const;

    private:
        std::vector<std::unique_ptr<XGlobalEvent>> _Events;
        mutable std::mutex _Mutex;
    };

    // ========================================================================
    // XGlobalEventWatcher - Continuous background watching with callbacks
    // ========================================================================

    class XGlobalEventWatcher final
    {
    public:
        using Callback = std::function<void(XGlobalEvent&)>;

        TOOTEGA_DISABLE_COPY_MOVE(XGlobalEventWatcher);

        ~XGlobalEventWatcher();

        [[nodiscard]] const std::wstring& GetEventName() const noexcept;
        [[nodiscard]] bool IsRunning() const noexcept;

        void Stop();

        [[nodiscard]] static XResult<std::unique_ptr<XGlobalEventWatcher>> Create(
            std::wstring_view pName,
            Callback pCallback);

        [[nodiscard]] static XResult<std::unique_ptr<XGlobalEventWatcher>> Create(
            std::unique_ptr<XGlobalEvent> pEvent,
            Callback pCallback);

        [[nodiscard]] static XResult<std::unique_ptr<XGlobalEventWatcher>> Watch(
            std::wstring_view pName,
            std::function<void()> pCallback);

    private:
        XGlobalEventWatcher(std::unique_ptr<XGlobalEvent> pEvent, Callback pCallback);

        void WatchLoop();

        std::unique_ptr<XGlobalEvent> _Event;
        Callback _Callback;
        std::atomic<bool> _Running{ true };
        std::thread _Thread;
        HANDLE _StopEvent = nullptr;
    };

} // namespace Tootega
