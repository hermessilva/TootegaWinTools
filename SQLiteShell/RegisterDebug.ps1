<#
.SYNOPSIS
    SQLiteView Debug Registration

.DESCRIPTION
    Registers the SQLiteView Debug DLL without copying.
    The DLL runs directly from the build folder for debugging.
    Run as Administrator.

.NOTES
    Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
#>

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "           SQLiteView Debug Registration" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# Check admin
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "  [ERROR] Must run as Administrator!" -ForegroundColor Red
    Write-Host ""
    exit 1
}

# DLL path - directly from build folder
$DllPath = "D:\Tootega\Source\Tools\x64\Debug\SQLiteView.dll"

if (-not (Test-Path $DllPath)) {
    Write-Host "  [ERROR] DLL not found: $DllPath" -ForegroundColor Red
    Write-Host ""
    Write-Host "  Please build the project in Debug configuration first." -ForegroundColor Yellow
    Write-Host ""
    exit 1
}

Write-Host "  DLL Path: $DllPath" -ForegroundColor Gray
Write-Host ""

# Stop Explorer and hosts
Write-Host "  [1/4] Stopping Explorer..." -ForegroundColor Yellow
Stop-Process -Name "prevhost" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "dllhost" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "explorer" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
Write-Host "    [OK]" -ForegroundColor Green

# Unregister any existing installation
Write-Host "  [2/4] Unregistering existing DLL..." -ForegroundColor Yellow
$existingDll = "C:\Program Files\Tootega\SQLiteView.dll"
if (Test-Path $existingDll) {
    & regsvr32 /u /s "$existingDll" 2>&1 | Out-Null
    Start-Sleep -Milliseconds 500
}
# Also try to unregister the debug DLL in case it was registered before
& regsvr32 /u /s "$DllPath" 2>&1 | Out-Null
Start-Sleep -Milliseconds 500
Write-Host "    [OK]" -ForegroundColor Green

# Register Debug DLL using regsvr32
Write-Host "  [3/4] Registering Debug DLL..." -ForegroundColor Yellow
$result = & regsvr32 /s "$DllPath" 2>&1
Start-Sleep -Seconds 1

# Verify registration
$clsidPath = "HKCU:\Software\Classes\CLSID\{A1B2C3D4-E5F6-7A8B-9C0D-1E2F3A4B5C6D}"
if (Test-Path $clsidPath) {
    Write-Host "    [OK] CLSID registered" -ForegroundColor Green
} else {
    Write-Host "    [WARN] CLSID may not be registered" -ForegroundColor Yellow
}

# Notify shell of changes
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Shell32Debug {
    [DllImport("shell32.dll")]
    public static extern void SHChangeNotify(int wEventId, int uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
"@
[Shell32Debug]::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero)

# Restart Explorer
Write-Host "  [4/4] Starting Explorer..." -ForegroundColor Yellow
Start-Process explorer
Start-Sleep -Seconds 2
Write-Host "    [OK]" -ForegroundColor Green

Write-Host ""
Write-Host "================================================================" -ForegroundColor Green
Write-Host "  Debug registration complete!" -ForegroundColor Green
Write-Host "================================================================" -ForegroundColor Green
Write-Host ""
Write-Host "  DLL registered from: $DllPath" -ForegroundColor Cyan
Write-Host ""
Write-Host "  To debug in VS 2022:" -ForegroundColor Yellow
Write-Host "    1. Open SQLiteView.vcxproj" -ForegroundColor Gray
Write-Host "    2. Set Configuration to Debug|x64" -ForegroundColor Gray
Write-Host "    3. Set breakpoints in ShellFolder.cpp" -ForegroundColor Gray
Write-Host "    4. Press F5 to start debugging" -ForegroundColor Gray
Write-Host ""
