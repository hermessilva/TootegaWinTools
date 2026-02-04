// ============================================================================
// XGlobalEvent.cpp - Global Named Events implementation
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================

#include "XGlobalEvent.h"
#include <sddl.h>

namespace Tootega
{
    // ========================================================================
    // XGlobalEvent Implementation
    // ========================================================================

    XResult<XGlobalEvent> XGlobalEvent::Create(const XGlobalEventOptions& pOptions)
    {
        if (pOptions.Name.empty())
            return XError::Application(ERROR_INVALID_PARAMETER, L"Event name cannot be empty");

        std::wstring fullname = std::wstring(GlobalPrefix) + pOptions.Name;

        SECURITY_DESCRIPTOR sd{};
        EXPLICIT_ACCESSW ea{};
        PACL acl = nullptr;

        auto secresult = CreateSecurityAttributes(pOptions.AccessRights, sd, ea, acl);
        if (!secresult)
            return secresult.Error();

        SECURITY_ATTRIBUTES sa = *secresult;

        HANDLE handle = CreateEventW(
            &sa,
            pOptions.ManualReset ? TRUE : FALSE,
            pOptions.InitialState ? TRUE : FALSE,
            fullname.c_str());

        bool creatednew = (GetLastError() != ERROR_ALREADY_EXISTS);

        if (acl)
            LocalFree(acl);

        if (!handle)
            return XError::FromLastError(L"CreateEventW failed");

        return XGlobalEvent(XUniqueHandle(handle), pOptions.Name, creatednew);
    }

    XResult<XGlobalEvent> XGlobalEvent::Create(std::wstring_view pName, bool pManualReset, bool pInitialState)
    {
        XGlobalEventOptions opts(pName);
        opts.ManualReset = pManualReset;
        opts.InitialState = pInitialState;
        return Create(opts);
    }

    XResult<XGlobalEvent> XGlobalEvent::Open(std::wstring_view pName, XGlobalEventAccessRights pAccess)
    {
        if (pName.empty())
            return XError::Application(ERROR_INVALID_PARAMETER, L"Event name cannot be empty");

        std::wstring fullname = std::wstring(GlobalPrefix) + std::wstring(pName);

        HANDLE handle = OpenEventW(
            static_cast<DWORD>(pAccess),
            FALSE,
            fullname.c_str());

        if (!handle)
            return XError::FromLastError(std::format(L"OpenEventW failed for '{}'", pName));

        return XGlobalEvent(XUniqueHandle(handle), std::wstring(pName), false);
    }

    std::optional<XGlobalEvent> XGlobalEvent::TryOpen(std::wstring_view pName, XGlobalEventAccessRights pAccess) noexcept
    {
        if (pName.empty())
            return std::nullopt;

        std::wstring fullname = std::wstring(GlobalPrefix) + std::wstring(pName);

        HANDLE handle = OpenEventW(
            static_cast<DWORD>(pAccess),
            FALSE,
            fullname.c_str());

        if (!handle)
            return std::nullopt;

        return XGlobalEvent(XUniqueHandle(handle), std::wstring(pName), false);
    }

    bool XGlobalEvent::Exists(std::wstring_view pName) noexcept
    {
        if (pName.empty())
            return false;

        std::wstring fullname = std::wstring(GlobalPrefix) + std::wstring(pName);

        HANDLE handle = OpenEventW(SYNCHRONIZE, FALSE, fullname.c_str());
        if (!handle)
            return false;

        CloseHandle(handle);
        return true;
    }

    XResult<void> XGlobalEvent::Signal(std::wstring_view pName)
    {
        auto evtresult = Open(pName);
        if (!evtresult)
            return evtresult.Error();

        return evtresult->Set();
    }

    bool XGlobalEvent::TrySignal(std::wstring_view pName) noexcept
    {
        auto evt = TryOpen(pName);
        if (!evt)
            return false;

        auto result = evt->Set();
        return result.HasValue();
    }

    XResult<void> XGlobalEvent::Set()
    {
        if (!IsValid())
            return XError::Application(ERROR_INVALID_HANDLE, L"Event handle is invalid");

        if (!SetEvent(_Handle.get()))
            return XError::FromLastError(L"SetEvent failed");

        return XResult<void>::Success();
    }

    XResult<void> XGlobalEvent::Reset()
    {
        if (!IsValid())
            return XError::Application(ERROR_INVALID_HANDLE, L"Event handle is invalid");

        if (!ResetEvent(_Handle.get()))
            return XError::FromLastError(L"ResetEvent failed");

        return XResult<void>::Success();
    }

    bool XGlobalEvent::Wait(DWORD pTimeoutMs) const noexcept
    {
        if (!IsValid())
            return false;

        DWORD result = WaitForSingleObject(_Handle.get(), pTimeoutMs);
        return result == WAIT_OBJECT_0;
    }

    XGlobalEventResult XGlobalEvent::WaitForSignal(DWORD pTimeoutMs) const noexcept
    {
        if (!IsValid())
            return XGlobalEventResult::Failed(ERROR_INVALID_HANDLE);

        DWORD result = WaitForSingleObject(_Handle.get(), pTimeoutMs);

        switch (result)
        {
        case WAIT_OBJECT_0:
            return XGlobalEventResult::Signaled(0, _Name);

        case WAIT_TIMEOUT:
            return XGlobalEventResult::Timeout();

        case WAIT_ABANDONED:
            return XGlobalEventResult::Abandoned(0, _Name);

        case WAIT_FAILED:
        default:
            return XGlobalEventResult::Failed(GetLastError());
        }
    }

    void XGlobalEvent::Pulse()
    {
        if (IsValid())
            PulseEvent(_Handle.get());
    }

    XResult<SECURITY_ATTRIBUTES> XGlobalEvent::CreateSecurityAttributes(
        XGlobalEventAccessRights pRights,
        SECURITY_DESCRIPTOR& pSD,
        EXPLICIT_ACCESSW& pEA,
        PACL& pAcl)
    {
        if (!InitializeSecurityDescriptor(&pSD, SECURITY_DESCRIPTOR_REVISION))
            return XError::FromLastError(L"InitializeSecurityDescriptor failed");

        // Convert access rights to Windows permissions
        // For regular users to Open and Synchronize with events created by services,
        // we grant SYNCHRONIZE | EVENT_MODIFY_STATE which allows:
        // - SYNCHRONIZE: WaitForSingleObject, WaitForMultipleObjects
        // - EVENT_MODIFY_STATE: SetEvent, ResetEvent
        DWORD permissions = static_cast<DWORD>(pRights);

        ZeroMemory(&pEA, sizeof(EXPLICIT_ACCESSW));
        pEA.grfAccessPermissions = permissions;
        pEA.grfAccessMode = SET_ACCESS;
        pEA.grfInheritance = NO_INHERITANCE;
        pEA.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        pEA.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;

        PSID everyonesid = nullptr;
        SID_IDENTIFIER_AUTHORITY worldauth = SECURITY_WORLD_SID_AUTHORITY;

        if (!AllocateAndInitializeSid(&worldauth, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &everyonesid))
            return XError::FromLastError(L"AllocateAndInitializeSid failed");

        pEA.Trustee.ptstrName = static_cast<LPWSTR>(everyonesid);

        DWORD aclresult = SetEntriesInAclW(1, &pEA, nullptr, &pAcl);
        FreeSid(everyonesid);

        if (aclresult != ERROR_SUCCESS)
            return XError::FromWin32(aclresult, L"SetEntriesInAclW failed");

        if (!SetSecurityDescriptorDacl(&pSD, TRUE, pAcl, FALSE))
        {
            LocalFree(pAcl);
            pAcl = nullptr;
            return XError::FromLastError(L"SetSecurityDescriptorDacl failed");
        }

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = &pSD;
        sa.bInheritHandle = FALSE;

        return sa;
    }

    // ========================================================================
    // XGlobalEventMonitor Implementation
    // ========================================================================

    std::vector<std::wstring> XGlobalEventMonitor::GetEventNames() const
    {
        std::lock_guard<std::mutex> lock(_Mutex);
        std::vector<std::wstring> names;
        names.reserve(_Events.size());

        for (const auto& evt : _Events)
            names.push_back(evt->GetName());

        return names;
    }

    XGlobalEvent* XGlobalEventMonitor::Get(std::wstring_view pName) noexcept
    {
        std::lock_guard<std::mutex> lock(_Mutex);

        for (auto& evt : _Events)
        {
            if (evt->GetName() == pName)
                return evt.get();
        }

        return nullptr;
    }

    const XGlobalEvent* XGlobalEventMonitor::Get(std::wstring_view pName) const noexcept
    {
        std::lock_guard<std::mutex> lock(_Mutex);

        for (const auto& evt : _Events)
        {
            if (evt->GetName() == pName)
                return evt.get();
        }

        return nullptr;
    }

    bool XGlobalEventMonitor::Contains(std::wstring_view pName) const noexcept
    {
        return Get(pName) != nullptr;
    }

    XResult<XGlobalEvent*> XGlobalEventMonitor::Add(std::wstring_view pName, bool pManualReset, bool pInitialState)
    {
        XGlobalEventOptions opts(pName);
        opts.ManualReset = pManualReset;
        opts.InitialState = pInitialState;
        return Add(opts);
    }

    XResult<XGlobalEvent*> XGlobalEventMonitor::Add(const XGlobalEventOptions& pOptions)
    {
        std::lock_guard<std::mutex> lock(_Mutex);

        for (const auto& evt : _Events)
        {
            if (evt->GetName() == pOptions.Name)
                return XError::Application(ERROR_ALREADY_EXISTS, std::format(L"Event '{}' already monitored", pOptions.Name));
        }

        auto evtresult = XGlobalEvent::Create(pOptions);
        if (!evtresult)
            return evtresult.Error();

        auto ptr = std::make_unique<XGlobalEvent>(std::move(*evtresult));
        XGlobalEvent* rawptr = ptr.get();
        _Events.push_back(std::move(ptr));

        return rawptr;
    }

    bool XGlobalEventMonitor::Remove(std::wstring_view pName)
    {
        std::lock_guard<std::mutex> lock(_Mutex);

        auto it = std::find_if(_Events.begin(), _Events.end(),
            [&pName](const auto& pEvt) { return pEvt->GetName() == pName; });

        if (it == _Events.end())
            return false;

        _Events.erase(it);
        return true;
    }

    void XGlobalEventMonitor::Clear()
    {
        std::lock_guard<std::mutex> lock(_Mutex);
        _Events.clear();
    }

    void XGlobalEventMonitor::SetAll()
    {
        std::lock_guard<std::mutex> lock(_Mutex);

        for (auto& evt : _Events)
            static_cast<void>(evt->Set());
    }

    void XGlobalEventMonitor::ResetAll()
    {
        std::lock_guard<std::mutex> lock(_Mutex);

        for (auto& evt : _Events)
            static_cast<void>(evt->Reset());
    }

    XResult<void> XGlobalEventMonitor::Set(std::wstring_view pName)
    {
        std::lock_guard<std::mutex> lock(_Mutex);

        for (auto& evt : _Events)
        {
            if (evt->GetName() == pName)
                return evt->Set();
        }

        return XError::Application(ERROR_NOT_FOUND, std::format(L"Event '{}' not found", pName));
    }

    XResult<void> XGlobalEventMonitor::Reset(std::wstring_view pName)
    {
        std::lock_guard<std::mutex> lock(_Mutex);

        for (auto& evt : _Events)
        {
            if (evt->GetName() == pName)
                return evt->Reset();
        }

        return XError::Application(ERROR_NOT_FOUND, std::format(L"Event '{}' not found", pName));
    }

    XGlobalEventResult XGlobalEventMonitor::WaitAny(DWORD pTimeoutMs) const
    {
        std::vector<HANDLE> handles;
        std::vector<std::wstring> names;

        {
            std::lock_guard<std::mutex> lock(_Mutex);

            if (_Events.empty())
                return XGlobalEventResult::Timeout();

            handles.reserve(_Events.size());
            names.reserve(_Events.size());

            for (const auto& evt : _Events)
            {
                handles.push_back(evt->GetHandle());
                names.push_back(evt->GetName());
            }
        }

        DWORD result = WaitForMultipleObjects(
            static_cast<DWORD>(handles.size()),
            handles.data(),
            FALSE,
            pTimeoutMs);

        if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + handles.size())
        {
            int idx = static_cast<int>(result - WAIT_OBJECT_0);
            return XGlobalEventResult::Signaled(idx, names[idx]);
        }

        if (result >= WAIT_ABANDONED_0 && result < WAIT_ABANDONED_0 + handles.size())
        {
            int idx = static_cast<int>(result - WAIT_ABANDONED_0);
            return XGlobalEventResult::Abandoned(idx, names[idx]);
        }

        if (result == WAIT_TIMEOUT)
            return XGlobalEventResult::Timeout();

        return XGlobalEventResult::Failed(GetLastError());
    }

    bool XGlobalEventMonitor::WaitAll(DWORD pTimeoutMs) const
    {
        std::vector<HANDLE> handles;

        {
            std::lock_guard<std::mutex> lock(_Mutex);

            if (_Events.empty())
                return true;

            handles.reserve(_Events.size());

            for (const auto& evt : _Events)
                handles.push_back(evt->GetHandle());
        }

        DWORD result = WaitForMultipleObjects(
            static_cast<DWORD>(handles.size()),
            handles.data(),
            TRUE,
            pTimeoutMs);

        return result == WAIT_OBJECT_0;
    }

    // ========================================================================
    // XGlobalEventWatcher Implementation
    // ========================================================================

    XGlobalEventWatcher::XGlobalEventWatcher(std::unique_ptr<XGlobalEvent> pEvent, Callback pCallback)
        : _Event(std::move(pEvent))
        , _Callback(std::move(pCallback))
    {
        _StopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        _Thread = std::thread(&XGlobalEventWatcher::WatchLoop, this);
    }

    XGlobalEventWatcher::~XGlobalEventWatcher()
    {
        Stop();

        if (_Thread.joinable())
            _Thread.join();

        if (_StopEvent)
            CloseHandle(_StopEvent);
    }

    const std::wstring& XGlobalEventWatcher::GetEventName() const noexcept
    {
        static const std::wstring empty;
        return _Event ? _Event->GetName() : empty;
    }

    bool XGlobalEventWatcher::IsRunning() const noexcept
    {
        return _Running.load(std::memory_order_acquire);
    }

    void XGlobalEventWatcher::Stop()
    {
        _Running.store(false, std::memory_order_release);

        if (_StopEvent)
            SetEvent(_StopEvent);
    }

    XResult<std::unique_ptr<XGlobalEventWatcher>> XGlobalEventWatcher::Create(
        std::wstring_view pName,
        Callback pCallback)
    {
        auto evtresult = XGlobalEvent::Create(pName);
        if (!evtresult)
            return evtresult.Error();

        auto evt = std::make_unique<XGlobalEvent>(std::move(*evtresult));
        return std::unique_ptr<XGlobalEventWatcher>(new XGlobalEventWatcher(std::move(evt), std::move(pCallback)));
    }

    XResult<std::unique_ptr<XGlobalEventWatcher>> XGlobalEventWatcher::Create(
        std::unique_ptr<XGlobalEvent> pEvent,
        Callback pCallback)
    {
        if (!pEvent || !pEvent->IsValid())
            return XError::Application(ERROR_INVALID_HANDLE, L"Invalid event handle");

        return std::unique_ptr<XGlobalEventWatcher>(new XGlobalEventWatcher(std::move(pEvent), std::move(pCallback)));
    }

    XResult<std::unique_ptr<XGlobalEventWatcher>> XGlobalEventWatcher::Watch(
        std::wstring_view pName,
        std::function<void()> pCallback)
    {
        return Create(pName, [cb = std::move(pCallback)](XGlobalEvent&) { cb(); });
    }

    void XGlobalEventWatcher::WatchLoop()
    {
        HANDLE handles[2] = { _Event->GetHandle(), _StopEvent };

        while (_Running.load(std::memory_order_acquire))
        {
            DWORD result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

            if (result == WAIT_OBJECT_0)
            {
                if (_Running.load(std::memory_order_acquire) && _Callback)
                    _Callback(*_Event);
            }
            else if (result == WAIT_OBJECT_0 + 1)
            {
                break;
            }
            else if (result == WAIT_FAILED)
            {
                Sleep(100);
            }
        }
    }

} // namespace Tootega
