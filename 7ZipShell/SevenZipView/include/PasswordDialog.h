#pragma once
/*
** SevenZipView - Windows Explorer Shell Extension
** Password Dialog for Encrypted Archives
*/

#ifndef SEVENZIPVIEW_PASSWORDDIALOG_H
#define SEVENZIPVIEW_PASSWORDDIALOG_H

#include "Common.h"

namespace SevenZipView {

// Password dialog result
struct PasswordResult {
    bool Success;           // True if user entered password, false if cancelled
    std::wstring Password;  // The entered password
    bool Remember;          // If user wants to remember (for session)
    
    PasswordResult() : Success(false), Remember(false) {}
};

// Password dialog class
class PasswordDialog {
public:
    PasswordDialog();
    ~PasswordDialog();
    
    // Show the dialog and get password
    // Returns true if user entered password, false if cancelled
    PasswordResult Show(HWND parent, const std::wstring& archiveName);
    
    // Static helper to show dialog
    static PasswordResult Prompt(HWND parent, const std::wstring& archivePath);
    
private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    INT_PTR HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    std::wstring _ArchiveName;
    PasswordResult _Result;
};

// Password cache for session (cleared when DLL unloads)
class PasswordCache {
public:
    static PasswordCache& Instance();
    
    // Store password for an archive
    void Store(const std::wstring& archivePath, const std::wstring& password);
    
    // Get stored password (returns empty if not found)
    std::wstring Get(const std::wstring& archivePath) const;
    
    // Check if password is cached
    bool Has(const std::wstring& archivePath) const;
    
    // Remove cached password
    void Remove(const std::wstring& archivePath);
    
    // Clear all cached passwords
    void Clear();
    
private:
    PasswordCache() = default;
    ~PasswordCache() = default;
    PasswordCache(const PasswordCache&) = delete;
    PasswordCache& operator=(const PasswordCache&) = delete;
    
    mutable std::mutex _Mutex;
    std::unordered_map<std::wstring, std::wstring> _Passwords;
};

} // namespace SevenZipView

#endif // SEVENZIPVIEW_PASSWORDDIALOG_H
