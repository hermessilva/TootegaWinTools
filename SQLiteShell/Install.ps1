<#
.SYNOPSIS
    SQLiteView Installer

.DESCRIPTION
    Installs the SQLiteView Windows Explorer Shell Extension.
    Run as Administrator for best results.

.PARAMETER SourceDll
    Path to the DLL to install. If not specified, uses x64\Release\SQLiteView.dll

.NOTES
    Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
#>

[CmdletBinding()]
param(
    [string]$SourceDll
)

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "              SQLiteView Installer" -ForegroundColor Cyan
Write-Host "    Windows Explorer Shell Extension for SQLite Databases" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# Check admin
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "  [WARN] Not running as Administrator. Some features may not work." -ForegroundColor Yellow
    Write-Host ""
}

# Find source DLL
if (-not $SourceDll) {
    $SourceDll = Join-Path $PSScriptRoot "x64\Release\SQLiteView.dll"
}

if (-not (Test-Path $SourceDll)) {
    Write-Host "  [ERROR] DLL not found: $SourceDll" -ForegroundColor Red
    Write-Host ""
    Write-Host "  Please build the project first:" -ForegroundColor Yellow
    Write-Host "    MSBuild SQLiteView\SQLiteView.vcxproj /p:Configuration=Release /p:Platform=x64" -ForegroundColor Gray
    Write-Host ""
    exit 1
}

Write-Host "  Source DLL: $SourceDll" -ForegroundColor Gray

# Installation directory
$InstallDir = "C:\Program Files\Tootega"
$TargetDll = Join-Path $InstallDir "SQLiteView.dll"

Write-Host "  Install to: $TargetDll" -ForegroundColor Gray
Write-Host ""

# Stop Explorer
Write-Host "  > Stopping Explorer..." -ForegroundColor White
Stop-Process -Name "prevhost" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "dllhost" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "explorer" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
Write-Host "    [OK]" -ForegroundColor Green

try {
    # Unregister old if exists
    if (Test-Path $TargetDll) {
        Write-Host "  > Unregistering existing DLL..." -ForegroundColor White
        & regsvr32 /u /s "$TargetDll"
        Start-Sleep -Milliseconds 500
        Write-Host "    [OK]" -ForegroundColor Green
    }

    # Create install directory
    Write-Host "  > Creating install directory..." -ForegroundColor White
    if (-not (Test-Path $InstallDir)) {
        New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    }
    Write-Host "    [OK]" -ForegroundColor Green

    # Copy files
    Write-Host "  > Copying files..." -ForegroundColor White
    Copy-Item -Path $SourceDll -Destination $TargetDll -Force
    
    $SourcePdb = $SourceDll -replace "\.dll$", ".pdb"
    if (Test-Path $SourcePdb) {
        Copy-Item -Path $SourcePdb -Destination (Join-Path $InstallDir "SQLiteView.pdb") -Force
    }
    Write-Host "    [OK]" -ForegroundColor Green

    # Register DLL
    Write-Host "  > Registering DLL..." -ForegroundColor White
    $result = & regsvr32 /s "$TargetDll" 2>&1
    Start-Sleep -Seconds 1
    
    # Verify registration
    $clsidPath = "HKCU:\Software\Classes\CLSID\{8B9C0D1E-2F3A-4B5C-6D7E-8F9A0B1C2D3E}"
    if (Test-Path $clsidPath) {
        Write-Host "    [OK] CLSID registered" -ForegroundColor Green
    } else {
        Write-Host "    [WARN] CLSID may not be registered" -ForegroundColor Yellow
    }

    # Verify file associations
    $extensions = @(".db", ".sqlite", ".sqlite3", ".db3")
    foreach ($ext in $extensions) {
        $extPath = "HKCU:\Software\Classes\$ext"
        if (Test-Path $extPath) {
            Write-Host "    [OK] $ext registered" -ForegroundColor Green
        }
    }

    # Notify shell
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Shell32Install {
    [DllImport("shell32.dll")]
    public static extern void SHChangeNotify(int wEventId, int uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
"@
    [Shell32Install]::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero)

    Write-Host ""
    Write-Host "  [SUCCESS] SQLiteView installed!" -ForegroundColor Green
    Write-Host ""
    Write-Host "  ============================================================" -ForegroundColor Yellow
    Write-Host "  IMPORTANT: To set SQLiteView as default for .db files:" -ForegroundColor Yellow
    Write-Host "  ============================================================" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "    1. Right-click any .db file" -ForegroundColor White
    Write-Host "    2. Select 'Open with' -> 'Choose another app'" -ForegroundColor White
    Write-Host "    3. Click 'More apps' and scroll down" -ForegroundColor White
    Write-Host "    4. Click 'Look for another app on this PC'" -ForegroundColor White
    Write-Host "    5. Navigate to: C:\Windows\explorer.exe" -ForegroundColor White
    Write-Host "    6. Check 'Always use this app'" -ForegroundColor White
    Write-Host ""
    Write-Host "  Supported extensions: .db, .sqlite, .sqlite3, .db3" -ForegroundColor Gray
    Write-Host ""
}
catch {
    Write-Host ""
    Write-Host "  [ERROR] $($_.Exception.Message)" -ForegroundColor Red
    Write-Host ""
}
finally {
    # Restart Explorer
    Write-Host "  > Restarting Explorer..." -ForegroundColor White
    Start-Process "explorer.exe"
    Start-Sleep -Seconds 2
    Write-Host "    [OK]" -ForegroundColor Green
    Write-Host ""
}
