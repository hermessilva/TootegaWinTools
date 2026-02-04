/*
** SevenZipView - Windows Explorer Shell Extension
** Password Dialog Implementation
** Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
*/

#include "PasswordDialog.h"
#include <windowsx.h>
#include <commctrl.h>

namespace SevenZipView {

// Dialog control IDs
#define IDC_PASSWORD_EDIT       1001
#define IDC_SHOW_PASSWORD       1002
#define IDC_REMEMBER_PASSWORD   1003
#define IDC_ARCHIVE_NAME        1004
#define IDC_ICON_LOCK           1005

// Dialog dimensions (compact design)
static const int DIALOG_WIDTH = 280;
static const int DIALOG_HEIGHT = 115;

//==============================================================================
// PasswordDialog Implementation
//==============================================================================

PasswordDialog::PasswordDialog() {
}

PasswordDialog::~PasswordDialog() {
}

PasswordResult PasswordDialog::Show(HWND parent, const std::wstring& archiveName) {
    _ArchiveName = archiveName;
    _Result = PasswordResult();
    
    // Register dialog class
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = DefDlgProcW;
    wc.cbWndExtra = DLGWINDOWEXTRA;
    wc.hInstance = g_hModule;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SevenZipViewPasswordDialog";
    RegisterClassExW(&wc);
    
    // Create dialog template in memory
    struct {
        DLGTEMPLATE dlg;
        WORD menu;
        WORD windowClass;
        WCHAR title[64];
    } dlgTemplate = {};
    
    dlgTemplate.dlg.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlgTemplate.dlg.dwExtendedStyle = 0;
    dlgTemplate.dlg.cdit = 0;
    dlgTemplate.dlg.x = 0;
    dlgTemplate.dlg.y = 0;
    dlgTemplate.dlg.cx = DIALOG_WIDTH;
    dlgTemplate.dlg.cy = DIALOG_HEIGHT;
    dlgTemplate.menu = 0;
    dlgTemplate.windowClass = 0;
    wcscpy_s(dlgTemplate.title, L"Password Required");
    
    // Show dialog
    INT_PTR result = DialogBoxIndirectParamW(
        g_hModule,
        &dlgTemplate.dlg,
        parent,
        DialogProc,
        reinterpret_cast<LPARAM>(this)
    );
    
    if (result == IDOK) {
        _Result.Success = true;
    }
    
    return _Result;
}

PasswordResult PasswordDialog::Prompt(HWND parent, const std::wstring& archivePath) {
    // Extract just the filename for display
    std::wstring archiveName = archivePath;
    size_t pos = archivePath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        archiveName = archivePath.substr(pos + 1);
    }
    
    // NOTE: Password caching is disabled - file path is not a reliable key
    // because the same path can have different files with different passwords.
    // The LZMA C SDK doesn't support decryption anyway, so caching is not needed.
    
    PasswordDialog dialog;
    PasswordResult result = dialog.Show(parent, archiveName);
    
    // NOTE: Not caching password - see comment above
    
    return result;
}

INT_PTR CALLBACK PasswordDialog::DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PasswordDialog* pThis = nullptr;
    
    if (msg == WM_INITDIALOG) {
        pThis = reinterpret_cast<PasswordDialog*>(lParam);
        SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<PasswordDialog*>(GetWindowLongPtrW(hwnd, DWLP_USER));
    }
    
    if (pThis) {
        return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    }
    
    return FALSE;
}

INT_PTR PasswordDialog::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        // Set window icon
        HICON hIcon = LoadIconW(nullptr, IDI_SHIELD);
        if (hIcon) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }
        
        // Calculate client area
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        int width = clientRect.right - clientRect.left;
        int m = 10; // margin
        int y = 8;
        
        // Archive name (compact, single line with ellipsis)
        std::wstring archiveLabel = _ArchiveName;
        CreateWindowExW(
            0, L"STATIC", archiveLabel.c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS,
            m, y, width - m * 2, 16,
            hwnd, (HMENU)IDC_ARCHIVE_NAME, g_hModule, nullptr
        );
        y += 22;
        
        // Password edit (full width)
        HWND hPassword = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_PASSWORD | ES_AUTOHSCROLL,
            m, y, width - m * 2, 22,
            hwnd, (HMENU)IDC_PASSWORD_EDIT, g_hModule, nullptr
        );
        SendMessageW(hPassword, EM_SETPASSWORDCHAR, 0x25CF, 0);
        y += 26;
        
        // Checkboxes side by side
        CreateWindowExW(
            0, L"BUTTON", L"Show",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            m, y, 50, 16,
            hwnd, (HMENU)IDC_SHOW_PASSWORD, g_hModule, nullptr
        );
        CreateWindowExW(
            0, L"BUTTON", L"Remember",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            m + 55, y, 75, 16,
            hwnd, (HMENU)IDC_REMEMBER_PASSWORD, g_hModule, nullptr
        );
        
        // Buttons on the right
        int btnW = 60, btnH = 22;
        CreateWindowExW(
            0, L"BUTTON", L"OK",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            width - m - btnW * 2 - 4, y - 2, btnW, btnH,
            hwnd, (HMENU)IDOK, g_hModule, nullptr
        );
        CreateWindowExW(
            0, L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            width - m - btnW, y - 2, btnW, btnH,
            hwnd, (HMENU)IDCANCEL, g_hModule, nullptr
        );
        
        // Set font for all controls
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        EnumChildWindows(hwnd, [](HWND hChild, LPARAM lParam) -> BOOL {
            SendMessageW(hChild, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
        }, (LPARAM)hFont);
        
        // Focus password field
        SetFocus(hPassword);
        
        // Center dialog
        RECT rcOwner, rcDlg;
        HWND hOwner = GetParent(hwnd);
        if (!hOwner) hOwner = GetDesktopWindow();
        GetWindowRect(hOwner, &rcOwner);
        GetWindowRect(hwnd, &rcDlg);
        int cx = rcOwner.left + (rcOwner.right - rcOwner.left - (rcDlg.right - rcDlg.left)) / 2;
        int cy = rcOwner.top + (rcOwner.bottom - rcOwner.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hwnd, HWND_TOP, cx, cy, 0, 0, SWP_NOSIZE);
        
        return FALSE; // Don't set default focus
    }
    
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            // Get password
            HWND hPassword = GetDlgItem(hwnd, IDC_PASSWORD_EDIT);
            int len = GetWindowTextLengthW(hPassword);
            if (len > 0) {
                _Result.Password.resize(len + 1);
                GetWindowTextW(hPassword, &_Result.Password[0], len + 1);
                _Result.Password.resize(len);
            }
            
            // Get remember checkbox state
            HWND hRemember = GetDlgItem(hwnd, IDC_REMEMBER_PASSWORD);
            _Result.Remember = (SendMessageW(hRemember, BM_GETCHECK, 0, 0) == BST_CHECKED);
            
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        
        case IDC_SHOW_PASSWORD: {
            HWND hPassword = GetDlgItem(hwnd, IDC_PASSWORD_EDIT);
            HWND hCheck = GetDlgItem(hwnd, IDC_SHOW_PASSWORD);
            bool show = (SendMessageW(hCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
            
            // Toggle password visibility (U+25CF BLACK CIRCLE)
            SendMessageW(hPassword, EM_SETPASSWORDCHAR, show ? 0 : 0x25CF, 0);
            InvalidateRect(hPassword, nullptr, TRUE);
            return TRUE;
        }
        }
        break;
        
    case WM_CLOSE:
        EndDialog(hwnd, IDCANCEL);
        return TRUE;
    }
    
    return FALSE;
}

//==============================================================================
// PasswordCache Implementation
//==============================================================================

PasswordCache& PasswordCache::Instance() {
    static PasswordCache instance;
    return instance;
}

void PasswordCache::Store(const std::wstring& archivePath, const std::wstring& password) {
    std::lock_guard<std::mutex> lock(_Mutex);
    _Passwords[archivePath] = password;
}

std::wstring PasswordCache::Get(const std::wstring& archivePath) const {
    std::lock_guard<std::mutex> lock(_Mutex);
    auto it = _Passwords.find(archivePath);
    if (it != _Passwords.end()) {
        return it->second;
    }
    return L"";
}

bool PasswordCache::Has(const std::wstring& archivePath) const {
    std::lock_guard<std::mutex> lock(_Mutex);
    return _Passwords.find(archivePath) != _Passwords.end();
}

void PasswordCache::Remove(const std::wstring& archivePath) {
    std::lock_guard<std::mutex> lock(_Mutex);
    _Passwords.erase(archivePath);
}

void PasswordCache::Clear() {
    std::lock_guard<std::mutex> lock(_Mutex);
    _Passwords.clear();
}

} // namespace SevenZipView
