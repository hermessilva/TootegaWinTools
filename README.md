<h1 align="center">🛠️ Tootega Windows Tools</h1>

<p align="center">
  <strong>Native Windows Utilities and System Tools Collection</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%2010%2F11-blue?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/Architecture-x64-green?style=flat-square" alt="Architecture">
  <img src="https://img.shields.io/badge/C%2B%2B-17%2F20-orange?style=flat-square" alt="C++ Standard">
  <img src="https://img.shields.io/badge/License-MIT%20%2F%20Proprietary-yellow?style=flat-square" alt="License">
</p>

---

## Overview

This repository contains a collection of **Windows-native tools** developed by Tootega Pesquisa e Inovação. All projects are built using modern C++17/20 and target the Windows platform exclusively, leveraging Win32 APIs, COM, Windows Shell, and Windows security infrastructure.

---

## 📦 Projects

### Core Libraries

| Project | Description | Documentation |
|---------|-------------|---------------|
| **TootegaWinLib** | Foundation C++ library for Windows development. Provides RAII wrappers, error handling, string utilities, cryptography, registry access, logging, shell extension infrastructure, and more. | [README](TootegaWinLib/README.md) |
| **TootegaWinMFCLib** | MFC-based UI framework library. Provides application framework, screen capture, video recording, and video editing components using Media Foundation. | [README](TootegaWinMFCLib/README.md) |

### Applications

| Project | Description | Documentation |
|---------|-------------|---------------|
| **7ZipShell (SevenZipView)** | Windows Explorer shell extension for browsing `.7z` archives as virtual folders. Includes preview handler, property handler, context menu, and custom icons. | [README](7ZipShell/README.md) |
| **TootegaVideoTool** | Professional screen capture and video editing application. MDI interface with real-time preview, H.264 encoding, frame-accurate editing, and export with cropping. | [README](TootegaVideoTool/README.md) |

---

## 🔗 Submodules

> **Note:** Additional projects in this workspace may exist as **Git submodules**. These are independent repositories linked here for convenience. To initialize submodules after cloning:
>
> ```powershell
> git submodule update --init --recursive
> ```
>
> Each submodule has its own repository, issues, and release cycle. Refer to the individual submodule's README for documentation.

---

## 🏗️ Architecture

```
Tools/
├── README.md                    # This file
├── TootegaWinTools.slnx         # Master solution (all projects)
│
├── TootegaWinLib/               # Core library (static lib) - ACTIVE
│   ├── README.md
│   ├── TootegaWinLib.vcxproj
│   ├── Include/                 # Public headers
│   └── Source/                  # Implementation
│
├── TootegaWinMFCLib/            # MFC UI framework library (static lib) - ACTIVE
│   ├── README.md
│   ├── TootegaWinMFCLib.vcxproj
│   ├── Include/                 # Public headers (MFC components)
│   └── Source/                  # Implementation
│
├── ByTokenCommon/               # Legacy compatibility layer - DEPRECATED
│   ├── README.md
│   ├── ByTokenCommon.vcxproj    # Header-only wrapper
│   └── Include/
│       └── ByTokenCommon.h      # Includes TootegaWinLib + compatibility macros
│
├── 7ZipShell/                   # Shell extension for 7z archives
│   ├── README.md
│   ├── SevenZipView.slnx
│   ├── SevenZipView/            # DLL project
│   ├── Installer/               # Setup executable
│   └── ...
│
├── TootegaVideoTool/            # Screen capture and video editor app
│   ├── README.md
│   ├── TootegaVideoTool.vcxproj
│   ├── Include/                 # Application headers
│   ├── Source/                  # Application source
│   └── Resource/                # MFC resources
│
└── x64/                         # Build output
    ├── Release/
    └── Debug/
```

---

## 🔧 Requirements

### Development Environment

| Component | Minimum Version | Recommended | Notes |
|-----------|-----------------|-------------|-------|
| **Operating System** | Windows 10 (1903) | Windows 11 23H2 | Development and runtime |
| **Visual Studio** | 2022 (17.0) | 2022 (17.9+) | Required for v143 toolset and C++20 modules |
| **Windows SDK** | 10.0.19041.0 | 10.0.22621.0 | May 2020 Update SDK minimum |
| **C++ Toolset** | v143 | v143 | Visual Studio 2022 toolset |
| **C++ Standard** | C++17 | C++20 | Uses `std::format`, `std::span`, concepts |

### Compiler Features Required

| Feature | Standard | Usage |
|---------|----------|-------|
| `std::format` | C++20 | String formatting throughout |
| `std::span` | C++20 | Safe buffer passing |
| `std::optional` | C++17 | Optional return values |
| `std::string_view` | C++17 | Non-owning string references |
| `std::source_location` | C++20 | Logging with call site info |
| Concepts | C++20 | Template constraints |
| `if constexpr` | C++17 | Compile-time branching |
| Structured bindings | C++17 | Destructuring assignments |
| `[[nodiscard]]` | C++17 | Enforce return value checking |

### Windows SDK Components

| Component | Purpose |
|-----------|---------|
| **Shell API** | `IShellFolder`, `IContextMenu`, `IPreviewHandler`, `IPropertyStore` |
| **COM** | Component Object Model infrastructure |
| **CNG** | Cryptography Next Generation (`bcrypt.h`, `ncrypt.h`) |
| **WinCrypt** | Certificate management (`wincrypt.h`) |
| **Event Log** | Windows Event Log API (`winevt.h`) |
| **WTS API** | Terminal Services for session management |

### Runtime Requirements

| Requirement | Notes |
|-------------|-------|
| **OS** | Windows 10 version 1903 (19H1) or later |
| **Architecture** | x64 only (32-bit not supported) |
| **VC++ Runtime** | Not required (static CRT linking) |
| **Admin Rights** | Required for shell extension installation |

---

## 🚀 Building

### Clone the Repository

```powershell
# Clone with submodules
git clone --recurse-submodules https://github.com/tootega/Tools.git
cd Tools

# Or initialize submodules after clone
git submodule update --init --recursive
```

### Build All Projects

```powershell
# Build Release (all projects)
MSBuild TootegaWinTools.slnx /p:Configuration=Release /p:Platform=x64

# Build Debug
MSBuild TootegaWinTools.slnx /p:Configuration=Debug /p:Platform=x64
```

### Build Individual Projects

```powershell
# Build TootegaWinLib only
MSBuild TootegaWinLib\TootegaWinLib.vcxproj /p:Configuration=Release /p:Platform=x64

# Build SevenZipView only
MSBuild 7ZipShell\SevenZipView.slnx /p:Configuration=Release /p:Platform=x64
```

### Visual Studio

1. Open `TootegaWinTools.slnx` in Visual Studio 2022
2. Select **Release | x64** configuration
3. Build Solution (**Ctrl+Shift+B**)

---

## 📋 Project Descriptions

### TootegaWinLib

The **foundation library** for all Windows tools. Provides:

- **XResult** — Robust error handling without exceptions
- **XString** — String manipulation and encoding conversion
- **XCrypto** — Cryptographic operations via Windows CNG
- **XFile** — File system utilities
- **XRegistry** — Type-safe registry access
- **XLogger** — Thread-safe multi-target logging
- **XShell** — Complete shell extension infrastructure
- **XProcess** — Launch processes in user sessions from SYSTEM
- **XGlobalEvent** — Cross-process synchronization

📖 [Full documentation](TootegaWinLib/README.md)

---

### TootegaWinMFCLib

The **MFC UI framework library** for desktop applications. Provides:

- **XApplication** — MDI application framework with Media Foundation integration
- **XVideoRecorder** — Hardware-accelerated screen capture and H.264 encoding
- **XVideoEditorDocument** — Frame-accurate video file handling
- **XPreviewPanel** — Live window preview with selection rectangle
- **XThumbnailStrip** — Visual timeline for video navigation
- **XWindowEnumerator** — Window discovery and enumeration

📖 [Full documentation](TootegaWinMFCLib/README.md)

---

### TootegaVideoTool

A **professional screen capture and video editing application**:

- Window-based screen recording with region selection
- Real-time preview with configurable FPS (1-60)
- H.264/MP4 hardware-accelerated encoding
- Frame-accurate video editing with mark in/out
- Export trimmed segments with optional cropping
- MDI interface for multiple simultaneous projects

📖 [Full documentation](TootegaVideoTool/README.md)

---

### 7ZipShell (SevenZipView)

A **Windows Explorer shell extension** for `.7z` archives:

- Browse archives as virtual folders in Explorer
- Preview text files in the preview pane
- View archive properties (file count, compression ratio, encryption)
- Context menu for extraction operations
- Custom file icons for `.7z` files

📖 [Full documentation](7ZipShell/README.md)

---

## 📄 License

| Project | License |
|---------|---------|
| **TootegaWinLib** | Proprietary — © Tootega Pesquisa e Inovação |
| **7ZipShell** | MIT License |

### Third-Party Components

- **7-Zip LZMA SDK** — Public Domain (Igor Pavlov)

---

## 🏢 About Tootega

**Tootega Pesquisa e Inovação** develops enterprise software solutions with a focus on security, performance, and Windows system integration.

- **Website:** [tootega.com.br](https://tootega.com.br)
- **Founded:** 1999

---

<p align="center">
  Made with ❤️ by <a href="https://tootega.com.br">Tootega</a>
</p>

<p align="center">
  <sub>Copyright © 1999-2026 Tootega Pesquisa e Inovação. All rights reserved.</sub>
</p>
