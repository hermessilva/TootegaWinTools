# SQLiteView

**SQLiteView** is a Windows Explorer shell extension that enables browsing SQLite database files (.db, .sqlite, .sqlite3, .db3) directly in Windows Explorer. Tables appear as folders, and records appear as files with dynamic columns showing field values.

## Features

### Shell Integration
- **Browse databases as folders**: Double-click a SQLite database file to explore its contents
- **Tables as folders**: Each table and view appears as a navigable folder
- **Records as files**: Each database record is displayed as a virtual file
- **Dynamic columns**: The Explorer details pane shows column data from the current table
- **Preview Handler**: Preview panel shows database statistics and schema information
- **Property Store**: File properties include table count, record count, page size, encoding

### Context Menu
- Export table/database to CSV
- Export table/database to JSON
- Export schema to SQL
- Copy selected records as CSV, JSON, or INSERT statements
- View table/database schema
- Database integrity check

### Supported File Extensions
- `.db` - SQLite Database
- `.sqlite` - SQLite Database
- `.sqlite3` - SQLite Database
- `.db3` - SQLite Database

## Building

### Prerequisites
- Visual Studio 2022 (v143 toolset)
- Windows SDK 10.0
- SQLite3 amalgamation (sqlite3.c, sqlite3.h)

### Build Steps

1. **Download SQLite Amalgamation**
   
   Download the SQLite amalgamation source from [sqlite.org](https://www.sqlite.org/download.html):
   - Get `sqlite-amalgamation-*.zip`
   - Extract `sqlite3.c` and `sqlite3.h` to the `sqlite3/` folder

2. **Build TootegaWinLib** (dependency)
   ```powershell
   cd ..\..\TootegaWinLib
   msbuild TootegaWinLib.vcxproj /p:Configuration=Release /p:Platform=x64
   ```

3. **Build SQLiteView**
   ```powershell
   msbuild SQLiteView.vcxproj /p:Configuration=Release /p:Platform=x64
   ```

## Installation

### Using PowerShell Script
```powershell
# Run as Administrator
.\Install.ps1
```

### Manual Registration
```powershell
# Run as Administrator
regsvr32 "x64\Release\SQLiteView.dll"
```

## Uninstallation

### Using PowerShell Script
```powershell
# Run as Administrator
.\Uninstall.ps1
```

### Manual Unregistration
```powershell
# Run as Administrator
regsvr32 /u "x64\Release\SQLiteView.dll"
```

## Architecture

### Components

| Component | Interface | Description |
|-----------|-----------|-------------|
| **ShellFolder** | `IShellFolder2`, `IPersistFolder3` | Core shell namespace extension |
| **PreviewHandler** | `IPreviewHandler` | Preview pane content provider |
| **PropertyHandler** | `IPropertyStore` | Property pane values |
| **ContextMenu** | `IContextMenu3` | Right-click menu items |
| **IconHandler** | `IExtractIconW` | Custom file/folder icons |

### Data Flow

```
┌─────────────────┐     ┌──────────────┐     ┌─────────────────┐
│  Windows Shell  │────▶│  ShellFolder │────▶│    Database     │
│    Explorer     │     │   (COM DLL)  │     │ (SQLite Wrapper)│
└─────────────────┘     └──────────────┘     └─────────────────┘
                               │                      │
                               ▼                      ▼
                        ┌──────────────┐      ┌──────────────┐
                        │  PIDL Items  │      │   sqlite3.c  │
                        │  (Virtual)   │      │ (Amalgamation│
                        └──────────────┘      └──────────────┘
```

### PIDL Structure

Items are identified using PIDLs (Pointer to Item IDList) with:
- Magic signature (`0x5351` = 'SQ')
- Item type (Database, Table, View, Record)
- Unique ID (rowid for records, hash for tables)
- Display name

## Static Linking

SQLiteView uses static linking for all dependencies:
- **SQLite3**: Amalgamation compiled directly into the DLL
- **CRT**: Static runtime (`/MT` for Release, `/MTd` for Debug)
- **TootegaWinLib**: Static library dependency

This ensures no external DLL dependencies at runtime.

## Configuration

### Registry Keys (Auto-registered)

```
HKCR\CLSID\{...SQLiteShellFolder...}\InProcServer32
HKCR\CLSID\{...SQLitePreviewHandler...}\InProcServer32
HKCR\CLSID\{...SQLitePropertyHandler...}\InProcServer32
HKCR\CLSID\{...SQLiteContextMenu...}\InProcServer32
HKCR\CLSID\{...SQLiteIconHandler...}\InProcServer32

HKCR\.db\ShellEx\...
HKCR\.sqlite\ShellEx\...
HKCR\.sqlite3\ShellEx\...
HKCR\.db3\ShellEx\...
```

## Limitations

- **Read-only access**: Database modifications require external tools
- **Large databases**: Performance may degrade with millions of records
- **BLOB columns**: Binary data is displayed as size information
- **Encrypted databases**: Not supported (SQLCipher encryption)

## License

MIT License - Copyright (c) 1999-2026 Tootega Pesquisa e Inovação

## See Also

- [SQLite Documentation](https://www.sqlite.org/docs.html)
- [Windows Shell Extension Programming](https://docs.microsoft.com/en-us/windows/win32/shell/shell-exts)
- [TootegaWinLib](../TootegaWinLib/README.md)
