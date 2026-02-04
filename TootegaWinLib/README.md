<h1 align="center">📚 TootegaWinLib</h1>

<p align="center">
  <strong>A Modern C++ Foundation Library for Windows Native Development</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#architecture">Architecture</a> •
  <a href="#modules">Modules</a> •
  <a href="#usage">Usage</a> •
  <a href="#building">Building</a> •
  <a href="#license">License</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%2010%2F11-blue?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/Architecture-x64-green?style=flat-square" alt="Architecture">
  <img src="https://img.shields.io/badge/C%2B%2B-17%2F20-orange?style=flat-square" alt="C++ Standard">
  <img src="https://img.shields.io/badge/License-Proprietary-red?style=flat-square" alt="License">
</p>

---

## Overview

**TootegaWinLib** is the foundational C++ library that powers all Tootega Windows native tools. Built with modern C++17/20 standards, it provides a robust, type-safe, and high-performance foundation for building Windows system-level applications, shell extensions, services, and security tools.

This library is **Windows-exclusive** by design, leveraging the full power of Win32, COM, CNG, and Windows Shell APIs without cross-platform abstractions that would compromise performance or capabilities.

---

## Features

### 🎯 Core Design Principles

| Principle | Description |
|-----------|-------------|
| **Zero-Allocation Hot Paths** | Critical paths avoid heap allocations using `Span<T>`, `string_view`, and stack buffers |
| **RAII Everywhere** | Every Windows resource (handles, certificates, keys) wrapped in smart RAII types |
| **Result Types** | No exceptions for error handling; uses `XResult<T>` for explicit success/failure |
| **Static Linking** | Designed for static linking to produce self-contained binaries |
| **Modern C++** | Uses C++17/20 features: `std::format`, `std::optional`, `std::span`, concepts |

### 🔧 What It Provides

- **Platform Configuration** — Windows SDK setup, platform detection, common macros
- **RAII Wrappers** — Smart handles for all Windows resources
- **Result Type** — Robust error handling without exceptions
- **String Utilities** — Encoding conversion, formatting, manipulation
- **Registry Access** — Type-safe registry operations
- **Cryptography** — SHA256/384/512, HMAC, certificates via CNG
- **File System** — File operations, path manipulation, directory enumeration
- **Process Management** — Launch processes in user sessions from SYSTEM context
- **Logging** — Thread-safe multi-target logging (file, console, Event Log)
- **Event Log** — Windows Event Log integration and forensic analysis
- **Global Events** — Cross-process/cross-session synchronization primitives
- **Named Pipes** — Client/server IPC infrastructure
- **Shell Extensions** — Complete infrastructure for Windows Explorer extensions
- **Elevation Utilities** — UAC checks and programmatic elevation

---

## Architecture

```
TootegaWinLib/
├── Include/
│   ├── TootegaWinLib.h          # Master include (includes all)
│   │
│   ├── XPlatform.h              # Platform detection, SDK config
│   ├── XTypes.h                 # RAII deleters, common enums
│   ├── XResult.h                # XResult<T> and XError classes
│   │
│   ├── XString.h                # String manipulation
│   ├── XStringConversion.h      # Encoding conversions
│   ├── XMemory.h                # Memory utilities, secure buffers
│   ├── XFile.h                  # File system operations
│   ├── XRegistry.h              # Windows Registry access
│   │
│   ├── XCrypto.h                # Cryptographic operations (CNG)
│   ├── XLogger.h                # Thread-safe logging
│   ├── XEventLog.h              # Windows Event Log writer
│   ├── XEventLogForensic.h      # Event Log reader/exporter
│   │
│   ├── XProcess.h               # Process launching utilities
│   ├── XElevation.h             # UAC elevation utilities
│   ├── XGlobalEvent.h           # Global named events
│   │
│   ├── XCapturePipeServer.h     # Named pipe server
│   ├── XCapturePipeClient.h     # Named pipe client
│   ├── XCaptureProtocol.h       # IPC protocol definitions
│   │
│   ├── XShell.h                 # Shell extension aggregation
│   └── Shell/
│       ├── XShellExtension.h    # COM base classes, class factory
│       ├── XShellRegistry.h     # Shell registration utilities
│       ├── XContextMenu.h       # IContextMenu base class
│       ├── XPropertyHandler.h   # IPropertyStore base class
│       ├── XIconHandler.h       # IExtractIconW base class
│       ├── XPreviewHandler.h    # IPreviewHandler base class
│       └── XShellFolder.h       # IShellFolder2 base class
│
└── Source/
    ├── XString.cpp
    ├── XStringConversion.cpp
    ├── XMemory.cpp
    ├── XFile.cpp
    ├── XRegistry.cpp
    ├── XCrypto.cpp
    ├── XLogger.cpp
    ├── XEventLog.cpp
    ├── XEventLogForensic.cpp
    ├── XProcess.cpp
    ├── XGlobalEvent.cpp
    ├── XCapturePipeServer.cpp
    ├── XCapturePipeClient.cpp
    └── Shell/
        └── ... (shell implementation files)
```

---

## Modules

### 📋 XResult — Error Handling

The `XResult<T>` type provides explicit success/failure handling without exceptions:

```cpp
#include <XResult.h>

// Function that may fail
XResult<std::wstring> ReadConfig(std::wstring_view pPath)
{
    if (!XFile::Exists(pPath))
        return XError::FromWin32(ERROR_FILE_NOT_FOUND, L"Config file missing");
    
    return XFile::ReadAllText(pPath);
}

// Usage
auto result = ReadConfig(L"C:\\config.ini");
if (result.HasValue())
    ProcessConfig(result.Value());
else
    LOG_ERROR(L"Failed: {}", result.Error().FormatMessage());
```

**XError Categories:**
- `Win32` — Windows API errors (`GetLastError()`)
- `NtStatus` — NT status codes
- `Security` — Security API errors
- `Application` — Custom application errors

---

### 🔤 XString — String Utilities

Comprehensive string manipulation and encoding:

```cpp
#include <XString.h>

// Encoding conversion
std::wstring wide = XString::ToWide("UTF-8 text");
std::string utf8 = XString::ToUtf8(L"Wide text");

// Case conversion
auto lower = XString::ToLower(L"HELLO");  // L"hello"
auto upper = XString::ToUpper(L"hello");  // L"HELLO"

// Trim, split, join
auto trimmed = XString::Trim(L"  text  ");
auto parts = XString::Split(L"a,b,c", L',');
auto joined = XString::Join(parts, L"-");  // L"a-b-c"

// Hex/Base64 encoding
auto hex = XString::ToHex(data);
auto b64 = XString::Base64Encode(data);

// Error messages
auto msg = XString::FromErrorCode(ERROR_ACCESS_DENIED);
```

---

### 🔐 XCrypto — Cryptography

Secure cryptographic operations using Windows CNG:

```cpp
#include <XCrypto.h>

// Hash computation
auto sha256 = XCrypto::ComputeSHA256(data);
auto sha512 = XCrypto::ComputeSHA512(data);
auto hmac = XCrypto::ComputeHMACSHA256(key, data);

// Random generation
auto randomBytes = XCrypto::GenerateRandomBytes(32);

// Certificate operations
auto thumbprint = XCrypto::GetCertificateThumbprint(certContext);
auto subject = XCrypto::GetCertificateSubject(certContext);
bool valid = XCrypto::IsCertificateValid(certContext);
```

---

### 📂 XFile — File System

High-level file system operations:

```cpp
#include <XFile.h>

// Read/Write
auto bytes = XFile::ReadAllBytes(L"file.bin");
auto text = XFile::ReadAllText(L"file.txt");
XFile::WriteAllText(L"output.txt", L"Content");

// Path operations
auto dir = XFile::GetDirectory(L"C:\\folder\\file.txt");  // L"C:\\folder"
auto name = XFile::GetFileName(L"C:\\folder\\file.txt");  // L"file.txt"
auto combined = XFile::Combine(L"C:\\folder", L"file.txt");

// Queries
bool exists = XFile::Exists(path);
bool isDir = XFile::IsDirectory(path);
auto size = XFile::GetSize(path);

// Enumeration
auto files = XFile::EnumerateFiles(L"C:\\folder", L"*.txt");
auto dirs = XFile::EnumerateDirectories(L"C:\\folder");
```

---

### 📝 XRegistry — Registry Access

Type-safe Windows Registry operations:

```cpp
#include <XRegistry.h>

// Read values
auto str = XRegistry::GetString(HKEY_LOCAL_MACHINE, subKey, valueName);
auto dword = XRegistry::GetDword(HKEY_CURRENT_USER, subKey, valueName);
auto binary = XRegistry::GetBinary(HKEY_LOCAL_MACHINE, subKey, valueName);

// Write values
XRegistry::SetString(HKEY_LOCAL_MACHINE, subKey, valueName, L"value");
XRegistry::SetDword(HKEY_CURRENT_USER, subKey, valueName, 42);

// Queries
bool exists = XRegistry::KeyExists(HKEY_LOCAL_MACHINE, subKey).ValueOr(false);
auto subKeys = XRegistry::EnumerateSubKeys(HKEY_LOCAL_MACHINE, subKey);
```

---

### 📊 XLogger — Logging Infrastructure

Thread-safe logging with multiple output targets:

```cpp
#include <XLogger.h>

// Initialize
XLogger::Instance().Initialize(L"MyApp", L"C:\\Logs");
XLogger::Instance().SetMinLevel(XLogLevel::Info);
XLogger::Instance().SetTargets(XLogTarget::File | XLogTarget::Console);

// Log messages
LOG_INFO(L"Application started");
LOG_WARNING(L"Resource usage: {}%", usage);
LOG_ERROR(L"Operation failed: {}", errorCode);

// Cleanup
XLogger::Instance().Shutdown();
```

**Log Targets:**
- `Console` — Standard output (when available)
- `File` — Rotating log files
- `DebugOutput` — `OutputDebugString` for debugger
- `EventLog` — Windows Event Log

---

### 🖥️ XShell — Shell Extensions

Complete infrastructure for building Windows Explorer shell extensions:

```cpp
#include <XShell.h>
using namespace Tootega::Shell;

// COM object with automatic reference counting
class MyContextMenu : public XComObject<IContextMenu, IShellExtInit>
{
public:
    // IShellExtInit
    STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE, IDataObject*, HKEY) override;
    
    // IContextMenu
    STDMETHODIMP QueryContextMenu(HMENU, UINT, UINT, UINT, UINT) override;
    STDMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO*) override;
    STDMETHODIMP GetCommandString(UINT_PTR, UINT, UINT*, CHAR*, UINT) override;
};

// Register shell extension
XShellRegistry::RegisterContextMenuHandler(
    L".myext",
    CLSID_MyContextMenu,
    L"My Context Menu Handler");
```

**Shell Components:**
- `XComObject<T...>` — COM base class with reference counting
- `XClassFactory<T>` — Generic class factory for COM objects
- `XShellModule` — Global module state management
- `XShellRegistry` — Shell extension registration utilities

---

### 🔄 XGlobalEvent — Cross-Process Synchronization

Global named events for system-wide synchronization:

```cpp
#include <XGlobalEvent.h>

// Create/open events
auto event = XGlobalEvent::Create(L"MyGlobalEvent", true, false);
auto existing = XGlobalEvent::Open(L"MyGlobalEvent");

// Signal and wait
XGlobalEvent::Signal(L"MyGlobalEvent");
auto result = XGlobalEvent::Wait(L"MyGlobalEvent", 5000);

if (result.IsSignaled())
    ProcessSignal();
else if (result.IsTimeout())
    HandleTimeout();
```

---

### 🚀 XProcess — Process Management

Launch processes in user sessions from SYSTEM context (for services):

```cpp
#include <XProcess.h>

// Get active console session
auto sessionId = XProcessLauncher::GetActiveConsoleSessionId();

// Launch process in user session (from SYSTEM service)
auto result = XProcessLauncher::LaunchAgentInActiveSession(
    L"C:\\Program Files\\MyApp\\Agent.exe",
    L"--arg1 --arg2");

if (result.HasValue())
    DWORD processId = result.Value();
```

---

### 🔑 XElevation — UAC Utilities

Check and request administrator privileges:

```cpp
#include <XElevation.h>

// Check privileges
if (!XElevation::IsRunningAsAdmin())
{
    // Request elevation via ShellExecute "runas"
    XElevation::RestartElevated(argc, argv);
    return 0;
}

// Get elevation type
auto type = XElevation::GetElevationType();
if (type == TokenElevationTypeLimited)
    // User is admin but not elevated
```

---

## Usage

### Including the Library

For most use cases, include the master header:

```cpp
#include <TootegaWinLib.h>

// All modules available under Tootega namespace
using namespace Tootega;
```

For shell extensions only:

```cpp
#include <XShell.h>

// Shell components under Tootega::Shell namespace
using namespace Tootega::Shell;
```

### Linking

Add `TootegaWinLib` as a project reference or link the static library:

```xml
<!-- In .vcxproj -->
<ItemGroup>
  <ProjectReference Include="..\TootegaWinLib\TootegaWinLib.vcxproj" />
</ItemGroup>
```

---

## Building

### Prerequisites

| Requirement | Version | Notes |
|-------------|---------|-------|
| **Visual Studio** | 2022 (v143) | Required for C++17/20 and ATL |
| **Windows SDK** | 10.0.19041.0+ | May 2020 Update SDK or newer |
| **C++ Standard** | C++17 minimum | Recommend C++20 for full features |
| **Platform** | x64 | 32-bit not supported |

### Build from Command Line

```powershell
# Build Release
MSBuild TootegaWinLib.vcxproj /p:Configuration=Release /p:Platform=x64

# Build Debug
MSBuild TootegaWinLib.vcxproj /p:Configuration=Debug /p:Platform=x64
```

### Build from Visual Studio

1. Open `TootegaWinLib.slnx` in Visual Studio 2022
2. Select **Release | x64** configuration
3. Build Solution (Ctrl+Shift+B)

### Output

```
x64/
├── Release/
│   ├── TootegaWinLib.lib    # Static library
│   └── TootegaWinLib.pdb    # Debug symbols
└── Debug/
    ├── TootegaWinLib.lib
    └── TootegaWinLib.pdb
```

---

## Dependent Projects

This library is used by:

| Project | Description |
|---------|-------------|
| [SevenZipView](../7ZipShell/README.md) | Windows Explorer shell extension for 7-Zip archives |
| *TootegaKSP* | Cryptographic Key Storage Provider (planned) |
| *TootegaMonitor* | System monitoring service (planned) |

---

## License

**Copyright © 1999-2026 Tootega Pesquisa e Inovação. All rights reserved.**

This library is proprietary software. Unauthorized copying, modification, distribution, or use is strictly prohibited without explicit written permission from Tootega Pesquisa e Inovação.

---

<p align="center">
  Made with ❤️ by <a href="https://tootega.com.br">Tootega</a>
</p>

<p align="center">
  <sub>Copyright © 1999-2026 Tootega Pesquisa e Inovação. All rights reserved.</sub>
</p>
