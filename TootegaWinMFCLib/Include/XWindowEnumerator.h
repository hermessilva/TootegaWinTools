#pragma once

#include <vector>
#include <string>

struct XWindowInfo
{
    HWND    Handle;
    CString Title;
    CString ClassName;
    DWORD   ProcessID;
    HICON   Icon;
};

class XWindowEnumerator
{
public:
    XWindowEnumerator() = default;
    ~XWindowEnumerator() = default;

    void Refresh();
    const std::vector<XWindowInfo>& GetWindows() const { return _Windows; }
    
    static bool IsValidCaptureWindow(HWND pHwnd);
    static CString GetWindowTitle(HWND pHwnd);
    static CString GetWindowClassName(HWND pHwnd);
    static HICON GetWindowIcon(HWND pHwnd);

private:
    std::vector<XWindowInfo> _Windows;

    static BOOL CALLBACK EnumWindowsProc(HWND pHwnd, LPARAM pLParam);
};
