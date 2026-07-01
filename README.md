<h1 align="center">🛠️ Tootega Windows Tools</h1>

<p align="center">
  <strong>Native Windows Utilities, Shell Extensions and System Tools</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%2010%2F11-blue?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/Architecture-x64-green?style=flat-square" alt="Architecture">
  <img src="https://img.shields.io/badge/C%2B%2B-17%2F20-orange?style=flat-square" alt="C++ Standard">
  <img src="https://img.shields.io/badge/Toolset-v143%20(VS%202022)-purple?style=flat-square" alt="Toolset">
  <img src="https://img.shields.io/badge/License-MIT%20%2F%20Proprietary-yellow?style=flat-square" alt="License">
</p>

---

## Overview

This repository is a collection of **Windows-native tools** developed by **Tootega Pesquisa e Inovação**.
Everything is written in modern **C++17/20**, targets Windows exclusively (x64), and links the CRT
statically — no VC++ redistributable required at runtime. The projects lean heavily on **Win32**, **COM**,
the **Windows Shell** namespace, **Media Foundation**, **ODBC**, and **CNG** cryptography.

Highlights:

- **Three Explorer shell extensions** that turn opaque files into browsable folders — `.7z` archives,
  `.db`/`.sqlite` databases, and SQL Server LocalDB `.mdf` files.
- A **professional screen-capture and video editor** built on Media Foundation.
- Two **reusable foundation libraries** (pure Win32 and MFC) that power the applications above.

---

## 📦 Projects

### Core Libraries

| Project | Description | Docs |
|---------|-------------|------|
| **TootegaWinLib** | Foundation C++ library for Windows: RAII wrappers, exception-free error handling, string/encoding utilities, CNG cryptography, registry access, logging, cross-session process launching, and complete **shell-extension infrastructure**. | [README](TootegaWinLib/README.md) |
| **TootegaWinMFCLib** | MFC-based UI framework: MDI application shell, hardware-accelerated screen capture, H.264 recording, and frame-accurate video editing components on top of Media Foundation. | [README](TootegaWinMFCLib/README.md) |

### Shell Extensions

Each shell extension is an in-process COM DLL plus a standalone **self-elevating installer** (embeds the DLL,
registers it with `regsvr32`, and adds an entry to *Programs and Features*).

| Project | Handles | What it does | Docs |
|---------|---------|--------------|------|
| **7ZipShell** *(SevenZipView)* | `.7z` | Browse archives as virtual folders; preview text, view compression/encryption properties, extraction context menu, custom icons. | [README](7ZipShell/README.md) |
| **SQLiteShell** *(SQLiteView)* | `.db` `.sqlite` `.sqlite3` | Browse SQLite databases as folders: **tables/views as subfolders, rows as items** with dynamic columns; preview pane grid, schema/index viewer, CSV/JSON/SQL export. | [README](SQLiteShell/README.md) |
| **SQLLocalDB** *(SQLLocalDBView)* | `.mdf` | Browse **SQL Server LocalDB** databases read-only: double-click an `.mdf`, it attaches to `(localdb)\MSSQLLocalDB` via **ODBC** and exposes tables/views/rows, schema, foreign keys, and exports. | [README](SQLLocalDB/README.md) |

### Applications

| Project | Description | Docs |
|---------|-------------|------|
| **TootegaVideoTool** | Screen capture + video editing app. MDI interface, real-time preview (1–60 FPS), H.264/MP4 hardware encoding, frame-accurate mark in/out, export with cropping. | [README](TootegaVideoTool/README.md) |

---

## 🏗️ Architecture

```
Tools/
├── README.md                     # This file (repository overview)
├── TootegaWinTools.slnx          # Master solution — all projects
│
├── TootegaWinLib/                # Core library (static lib)
├── TootegaWinMFCLib/             # MFC UI framework (static lib)
│
├── 7ZipShell/                    # Shell extension — .7z archives
│   ├── SevenZipView/             #   COM DLL
│   └── Installer/                #   Self-elevating setup.exe
│
├── SQLiteShell/                  # Shell extension — SQLite databases
│   ├── SQLiteView/               #   COM DLL (statically links sqlite3)
│   └── Installer/                #   Self-elevating setup.exe
│
├── SQLLocalDB/                   # Shell extension — SQL Server LocalDB (.mdf)
│   ├── src/  include/            #   COM DLL (ODBC, read-only)
│   └── Installer/                #   Self-elevating setup.exe
│
├── TootegaVideoTool/             # Screen capture + video editor app
│
└── x64/                          # Build output (Release / Debug)
```

---

## 🔧 Requirements

### Development

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| **OS** | Windows 10 (1903) | Windows 11 |
| **Visual Studio** | 2022 (17.0) | 2022 (17.9+) |
| **Windows SDK** | 10.0.19041.0 | 10.0.22621.0 |
| **Toolset** | v143 | v143 |
| **C++ Standard** | C++17 | C++20 |

### Runtime

| Requirement | Notes |
|-------------|-------|
| **OS / Arch** | Windows 10 1903+ / Windows 11, **x64 only** |
| **VC++ Runtime** | Not required (static CRT) |
| **Admin rights** | Only to install a shell extension (per-machine); the DLLs also support per-user registration |
| **SQL Server LocalDB** | Required by **SQLLocalDB** (inherent — the `.mdf` belongs to a LocalDB instance) |
| **ODBC driver for SQL Server** | Used by **SQLLocalDB**; the in-box `SQL Server` driver already works — no install needed on most machines |

---

## 🚀 Building

```powershell
# Clone
git clone https://github.com/HermesSilva/TootegaWinTools.git
cd TootegaWinTools

# Build everything (Release x64)
MSBuild TootegaWinTools.slnx /p:Configuration=Release /p:Platform=x64
```

### Individual projects

```powershell
MSBuild TootegaWinLib\TootegaWinLib.vcxproj      /p:Configuration=Release /p:Platform=x64
MSBuild SQLLocalDB\SQLLocalDBView.vcxproj        /p:Configuration=Release /p:Platform=x64
MSBuild SQLiteShell\SQLiteView\SQLiteView.vcxproj /p:Configuration=Release /p:Platform=x64
```

Or open `TootegaWinTools.slnx` in **Visual Studio 2022**, pick **Release | x64**, and build (**Ctrl+Shift+B**).
All outputs land in `x64\Release\`.

### Installing a shell extension

```powershell
# Register the DLL directly (per-user, no admin)...
regsvr32 x64\Release\SQLLocalDBView.dll

# ...or run the bundled installer (self-elevates, adds Add/Remove entry)
x64\Release\SQLLocalDBViewSetup.exe
```

Shell extensions load into `explorer.exe`. After (re)registering, restart Explorer to pick up the new build:
`taskkill /f /im explorer.exe & start explorer.exe`.

---

## 📋 Project Details

### TootegaWinLib — foundation library

- **XResult** — exception-free error handling
- **XString** — string manipulation and encoding conversion
- **XCrypto** — cryptography via Windows CNG
- **XFile / XRegistry** — file system and type-safe registry access
- **XLogger** — thread-safe multi-target logging
- **XShell** — shell-extension scaffolding (`IShellFolder`, `IContextMenu`, `IPreviewHandler`, …)
- **XProcess / XGlobalEvent** — cross-session process launch and cross-process synchronization

### TootegaWinMFCLib — MFC UI framework

- **XApplication** — MDI framework with Media Foundation integration
- **XVideoRecorder** — hardware-accelerated capture + H.264 encoding
- **XVideoEditorDocument** — frame-accurate video handling
- **XPreviewPanel / XThumbnailStrip** — live preview and visual timeline
- **XWindowEnumerator** — window discovery

### 7ZipShell — `.7z` archive browser

Browse `.7z` archives as virtual folders, preview text files, inspect compression/encryption properties,
extract via context menu, and show custom icons. Bundles the **7-Zip LZMA SDK** (public domain).

### SQLiteShell — SQLite database browser

Open any `.db`/`.sqlite`/`.sqlite3` file as a folder. Tables and views appear as **navigable subfolders**;
rows appear as items with **one Explorer column per table column**. Includes a preview-pane grid, schema and
index viewers, and **Copy/Export as CSV, JSON, and INSERT statements**. SQLite is compiled in statically —
zero external dependencies.

### SQLLocalDB — SQL Server LocalDB browser

Double-click an `.mdf` and browse it like a folder. The extension attaches the file to the automatic instance
`(localdb)\MSSQLLocalDB` over **ODBC** and reads metadata from the `sys.*` catalog. Features:

- **Read-only** — only `SELECT` / catalog queries; never mutates the database.
- Tables & views as folders, rows as items with **dynamic per-column display**.
- Fast row counts via `sys.partitions` (no table scans).
- Schema view: columns, types, identity, defaults, indexes and **foreign keys**; reconstructed `CREATE TABLE`.
- Copy/Export **CSV / JSON / INSERT**, and `DBCC CHECKDB` validation.
- **No install on most machines** — prefers the in-box `SQL Server` ODBC driver and caches the working
  driver (per process and in the registry) so first-open probing happens at most once.

---

## 📄 License

| Project | License |
|---------|---------|
| **TootegaWinLib**, **TootegaWinMFCLib** | Proprietary — © Tootega Pesquisa e Inovação |
| **7ZipShell**, **SQLiteShell**, **SQLLocalDB** | MIT License |

### Third-party components

| Component | License | Used by |
|-----------|---------|---------|
| **7-Zip LZMA SDK** (Igor Pavlov) | Public Domain | 7ZipShell |
| **SQLite** (D. Richard Hipp) | Public Domain | SQLiteShell |
| **Microsoft ODBC Driver for SQL Server** | System component | SQLLocalDB |

---

## 🏢 About Tootega

**Tootega Pesquisa e Inovação** builds enterprise software focused on security, performance, and deep
Windows system integration.

- **Website:** [tootega.com.br](https://tootega.com.br)
- **Founded:** 1999

---

<p align="center">
  Made with ❤️ by <a href="https://tootega.com.br">Tootega</a>
</p>

<p align="center">
  <sub>Copyright © 1999-2026 Tootega Pesquisa e Inovação. All rights reserved.</sub>
</p>
