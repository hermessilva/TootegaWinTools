#include "pch.h"
#include "XWindowEnumerator.h"

void XWindowEnumerator::Refresh()
{
    _Windows.clear();
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(this));
}

BOOL CALLBACK XWindowEnumerator::EnumWindowsProc(HWND pHwnd, LPARAM pLParam)
{
    if (!IsValidCaptureWindow(pHwnd))
        return TRUE;

    auto self = reinterpret_cast<XWindowEnumerator*>(pLParam);

    XWindowInfo info;
    info.Handle = pHwnd;
    info.Title = GetWindowTitle(pHwnd);
    info.ClassName = GetWindowClassName(pHwnd);
    GetWindowThreadProcessId(pHwnd, &info.ProcessID);
    info.Icon = GetWindowIcon(pHwnd);

    if (!info.Title.IsEmpty())
        self->_Windows.push_back(info);

    return TRUE;
}

bool XWindowEnumerator::IsValidCaptureWindow(HWND pHwnd)
{
    if (!IsWindowVisible(pHwnd))
        return false;

    if (GetWindow(pHwnd, GW_OWNER) != nullptr)
        return false;

    LONG exStyle = GetWindowLong(pHwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW)
        return false;

    TCHAR className[256];
    GetClassName(pHwnd, className, 256);

    if (_tcscmp(className, _T("Progman")) == 0 ||
        _tcscmp(className, _T("WorkerW")) == 0 ||
        _tcscmp(className, _T("Shell_TrayWnd")) == 0 ||
        _tcscmp(className, _T("Shell_SecondaryTrayWnd")) == 0)
        return false;

    RECT rect;
    if (!GetWindowRect(pHwnd, &rect))
        return false;

    if (rect.right - rect.left <= 0 || rect.bottom - rect.top <= 0)
        return false;

    return true;
}

CString XWindowEnumerator::GetWindowTitle(HWND pHwnd)
{
    CString title;
    int len = GetWindowTextLength(pHwnd);
    if (len > 0)
    {
        GetWindowText(pHwnd, title.GetBuffer(len + 1), len + 1);
        title.ReleaseBuffer();
    }
    return title;
}

CString XWindowEnumerator::GetWindowClassName(HWND pHwnd)
{
    CString className;
    GetClassName(pHwnd, className.GetBuffer(256), 256);
    className.ReleaseBuffer();
    return className;
}

HICON XWindowEnumerator::GetWindowIcon(HWND pHwnd)
{
    HICON hIcon = reinterpret_cast<HICON>(SendMessage(pHwnd, WM_GETICON, ICON_SMALL, 0));
    if (!hIcon)
        hIcon = reinterpret_cast<HICON>(SendMessage(pHwnd, WM_GETICON, ICON_BIG, 0));
    if (!hIcon)
        hIcon = reinterpret_cast<HICON>(GetClassLongPtr(pHwnd, GCLP_HICONSM));
    if (!hIcon)
        hIcon = reinterpret_cast<HICON>(GetClassLongPtr(pHwnd, GCLP_HICON));
    return hIcon;
}
