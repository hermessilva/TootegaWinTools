<h1 align="center">🗄️ SQLiteView</h1>

<p align="center">
  <strong>A Native Windows Explorer Shell Extension for SQLite Databases</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#download">Download</a> •
  <a href="#installation">Installation</a> •
  <a href="#building">Building</a> •
  <a href="#architecture">Architecture</a> •
  <a href="#license">License</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%2010%2F11-blue?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/Architecture-x64-green?style=flat-square" alt="Architecture">
  <img src="https://img.shields.io/badge/License-MIT-yellow?style=flat-square" alt="License">
  <img src="https://img.shields.io/badge/Version-1.0.0-orange?style=flat-square" alt="Version">
</p>

---

## Overview

**SQLiteView** is a lightweight, high-performance Windows Shell Extension that seamlessly integrates SQLite database browsing directly into Windows Explorer. Unlike traditional database tools that require opening a separate application, SQLiteView allows you to navigate `.db`, `.sqlite`, `.sqlite3`, and `.db3` files as if they were regular folders — with tables appearing as subfolders and records as virtual files.

Built entirely in modern C++20 with the official SQLite3 amalgamation, SQLiteView provides native performance with zero external dependencies. The DLL is statically linked, meaning it works out of the box without requiring any runtime libraries or additional software installation.

---

## Features

### 🗂️ Virtual Shell Folder Navigation
Browse inside SQLite databases directly in Windows Explorer:
- **Database Level** — See all tables and views as navigable folders
- **Table Level** — View records as virtual files with dynamic columns
- **Record Details** — Full column data displayed in the Details pane

### 📊 Dynamic Column Display
When viewing records inside a table, the Explorer Details pane dynamically shows all columns from that table:
- Column headers match the table schema
- Data types are automatically formatted
- NULL values displayed appropriately
- BLOB sizes shown instead of raw data

### 👁️ Preview Handler
Preview database information directly in the Explorer preview pane:
- Database statistics (tables, views, indexes)
- Page size and encoding information
- SQLite version
- Schema overview for tables

### 📋 Property Handler
View detailed database and table properties:
- **Table Count** — Number of tables in the database
- **View Count** — Number of views
- **Index Count** — Number of indexes
- **Trigger Count** — Number of triggers
- **Page Size** — Database page size
- **Encoding** — Text encoding (UTF-8, UTF-16)
- **Record Count** — For individual tables

### 🎨 Custom Icon Handler
Distinctive icons for SQLite files that integrate naturally with the Windows shell:
- Database icon for `.db` files
- Table/folder icons for tables
- Record/document icons for rows

### 📋 Context Menu Integration
Right-click context menu with powerful options:

| Command | Scope | Description |
|---------|-------|-------------|
| **Export to CSV** | Database/Table | Export all data to CSV format |
| **Export to JSON** | Table/Record | Export as JSON |
| **Export Schema** | Database/Table | Export CREATE statements |
| **Copy as CSV** | Record | Copy selected records to clipboard |
| **Copy as JSON** | Record | Copy as JSON to clipboard |
| **Copy as INSERT** | Record | Copy as SQL INSERT statements |
| **View Schema** | Database/Table | Display table schema |
| **Integrity Check** | Database | Verify database integrity |

### ⚡ Performance Optimized
- **Connection Pooling** — Open databases are cached for instant navigation
- **Lazy Loading** — Records are loaded on-demand with pagination
- **Prepared Statements** — All queries use prepared statements
- **Thread-Safe** — All operations support concurrent access

### 🔒 Security
- **Read-Only Access** — Databases opened in read-only mode
- **SQL Injection Safe** — All queries use parameterized statements
- **UAC Compliant** — Installer requests elevation only when necessary

---

## Supported File Extensions

| Extension | Description |
|-----------|-------------|
| `.db` | SQLite Database |
| `.sqlite` | SQLite Database |
| `.sqlite3` | SQLite Database |
| `.db3` | SQLite Database |

---

## Installation

### Quick Install (PowerShell)

```powershell
# Download and run as Administrator
.\Install.ps1
```

### Manual Install

1. Build the project (see Building section)
2. Run as Administrator:
   ```powershell
   regsvr32 "x64\Release\SQLiteView.dll"
   ```

### Post-Installation

To make Explorer automatically open `.db` files with SQLiteView:

1. Right-click any `.db` file
2. Select **Open with** → **Choose another app**
3. Click **More apps** and scroll down
4. Click **Look for another app on this PC**
5. Navigate to: `C:\Windows\explorer.exe`
6. Check **Always use this app**

---

## Uninstallation

```powershell
# Run as Administrator
.\Uninstall.ps1
```

Or manually:
```powershell
regsvr32 /u "C:\Program Files\Tootega\SQLiteView.dll"
```

---

## Building

### Prerequisites

- **Visual Studio 2022** (v143 toolset)
- **Windows SDK 10.0**
- **SQLite3 Amalgamation** (downloaded automatically or manually)

### Build Steps

1. **Download SQLite Amalgamation**
   
   Get the amalgamation from [sqlite.org/download.html](https://www.sqlite.org/download.html):
   - Download `sqlite-amalgamation-*.zip`
   - Extract `sqlite3.c` and `sqlite3.h` to `SQLiteView/sqlite3/`

2. **Build TootegaWinLib** (dependency)
   ```powershell
   cd ..\..\TootegaWinLib
   msbuild TootegaWinLib.vcxproj /p:Configuration=Release /p:Platform=x64
   ```

3. **Build SQLiteView**
   ```powershell
   msbuild SQLiteView\SQLiteView.vcxproj /p:Configuration=Release /p:Platform=x64
   ```

4. **Build Installer** (optional)
   ```powershell
   msbuild Installer\SQLiteViewSetup.vcxproj /p:Configuration=Release /p:Platform=x64
   ```

---

## Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Windows Explorer                         │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    SQLiteView.dll                           │
│  ┌───────────────┐  ┌───────────────┐  ┌───────────────┐   │
│  │  ShellFolder  │  │ PreviewHandler│  │PropertyHandler│   │
│  │  IShellFolder2│  │IPreviewHandler│  │IPropertyStore │   │
│  └───────┬───────┘  └───────┬───────┘  └───────┬───────┘   │
│          │                  │                  │           │
│          └──────────────────┼──────────────────┘           │
│                             ▼                              │
│  ┌───────────────────────────────────────────────────────┐ │
│  │                   Database Pool                        │ │
│  │              (Connection Caching)                      │ │
│  └───────────────────────────────────────────────────────┘ │
│                             │                              │
│                             ▼                              │
│  ┌───────────────────────────────────────────────────────┐ │
│  │               SQLite3 Amalgamation                     │ │
│  │                 (Static Linked)                        │ │
│  └───────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### COM Interfaces Implemented

| Interface | Purpose |
|-----------|---------|
| `IShellFolder2` | Virtual folder for database contents |
| `IPersistFolder3` | Initialize with database path |
| `IEnumIDList` | Enumerate tables/records |
| `IPreviewHandler` | Preview pane content |
| `IPropertyStore` | Properties pane values |
| `IContextMenu3` | Right-click menu commands |
| `IExtractIconW` | Custom icons |

### PIDL Structure

Items are identified using custom PIDLs:
- **Signature**: `0x5351` ('SQ')
- **Item Type**: Database, Table, View, Record
- **Item ID**: rowid for records, hash for tables
- **Display Name**: Table name or record identifier

### Static Linking

All dependencies are statically linked:
- SQLite3 amalgamation compiled directly into DLL
- Static CRT (`/MT` for Release)
- TootegaWinLib static library

Result: Single DLL with zero external dependencies.

---

## Technical Details

### Database Access

- **Read-Only Mode**: All databases opened with `SQLITE_OPEN_READONLY`
- **URI Filenames**: Enabled for advanced path handling
- **Shared Cache**: Disabled for isolation
- **Thread Safety**: `SQLITE_THREADSAFE=1` (serialized mode)

### SQLite Features Enabled

- `SQLITE_ENABLE_FTS5` — Full-text search
- `SQLITE_ENABLE_JSON1` — JSON functions

### Limitations

- **No Write Access**: Database modifications require external tools
- **Large Databases**: Performance may degrade with millions of records
- **BLOB Columns**: Binary data displayed as size information
- **Encrypted Databases**: SQLCipher/SEE encryption not supported
- **Memory Databases**: Only file-based databases supported

---

## License

MIT License

Copyright (c) 1999-2026 Tootega Pesquisa e Inovação

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---

## See Also

- [SQLite Documentation](https://www.sqlite.org/docs.html)
- [Windows Shell Extension Development](https://docs.microsoft.com/en-us/windows/win32/shell/shell-exts)
- [TootegaWinLib](../../TootegaWinLib/README.md)
