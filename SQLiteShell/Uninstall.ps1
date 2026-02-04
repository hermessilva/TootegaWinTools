<#
.SYNOPSIS
    SQLiteView Uninstaller

.DESCRIPTION
    Removes the SQLiteView Windows Explorer Shell Extension and all registry entries.

.PARAMETER Force
    Forces uninstallation even if some operations fail.

.NOTES
    Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
#>

[CmdletBinding()]
param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "              SQLiteView Uninstaller" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# Possible installation locations
$InstallLocations = @(
    "C:\Program Files\Tootega\SQLiteView.dll",
    "$env:LOCALAPPDATA\Tootega\SQLiteView.dll",
    "$PSScriptRoot\x64\Release\SQLiteView.dll",
    "$PSScriptRoot\x64\Debug\SQLiteView.dll"
)

# Stop Explorer and related processes
Write-Host "  > Stopping Explorer and related processes..." -ForegroundColor White
$explorerWasRunning = $false
$explorer = Get-Process -Name "explorer" -ErrorAction SilentlyContinue
if ($explorer) {
    $explorerWasRunning = $true
}

foreach ($proc in @("prevhost", "dllhost", "SearchProtocolHost", "SearchFilterHost")) {
    Stop-Process -Name $proc -Force -ErrorAction SilentlyContinue
}
Stop-Process -Name "explorer" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
Write-Host "    [OK] Processes stopped" -ForegroundColor Green

try {
    # Unregister from all locations
    Write-Host "  > Unregistering DLL..." -ForegroundColor White
    foreach ($dllPath in $InstallLocations) {
        if (Test-Path $dllPath) {
            Write-Host "    Found: $dllPath" -ForegroundColor Gray
            & regsvr32 /u /s "$dllPath"
            Start-Sleep -Milliseconds 500
        }
    }
    Write-Host "    [OK] DLL unregistered" -ForegroundColor Green

    # Clean registry - HKCU
    Write-Host "  > Cleaning registry (HKCU)..." -ForegroundColor White
    
    $keysToDelete = @(
        "HKCU:\Software\Classes\SQLiteView.Database",
        "HKCU:\Software\Classes\CLSID\{A1B2C3D4-E5F6-7A8B-9C0D-1E2F3A4B5C6D}",
        "HKCU:\Software\Classes\CLSID\{B2C3D4E5-F6A7-8B9C-0D1E-2F3A4B5C6D7E}",
        "HKCU:\Software\Classes\CLSID\{C3D4E5F6-A7B8-9C0D-1E2F-3A4B5C6D7E8F}",
        "HKCU:\Software\Classes\CLSID\{D4E5F6A7-B8C9-0D1E-2F3A-4B5C6D7E8F9A}",
        "HKCU:\Software\Classes\CLSID\{E5F6A7B8-C9D0-1E2F-3A4B-5C6D7E8F9A0B}"
    )
    
    $extensions = @(".db", ".sqlite", ".sqlite3", ".db3")
    foreach ($ext in $extensions) {
        $keysToDelete += "HKCU:\Software\Classes\$ext\ShellEx"
        $keysToDelete += "HKCU:\Software\Classes\$ext\SQLiteView.Database"
        $keysToDelete += "HKCU:\Software\Classes\$ext\OpenWithProgids"
        $keysToDelete += "HKCU:\Software\Classes\SystemFileAssociations\$ext"
    }
    
    foreach ($key in $keysToDelete) {
        if (Test-Path $key) {
            Remove-Item -Path $key -Recurse -Force -ErrorAction SilentlyContinue
            Write-Host "    Removed: $key" -ForegroundColor Gray
        }
    }
    
    # Reset file associations to remove our ProgId
    foreach ($ext in $extensions) {
        $extPath = "HKCU:\Software\Classes\$ext"
        if (Test-Path $extPath) {
            $currentProgId = (Get-ItemProperty -Path $extPath -ErrorAction SilentlyContinue).'(default)'
            if ($currentProgId -eq "SQLiteView.Database") {
                Remove-ItemProperty -Path $extPath -Name "(default)" -ErrorAction SilentlyContinue
                Remove-ItemProperty -Path $extPath -Name "PerceivedType" -ErrorAction SilentlyContinue
                Remove-ItemProperty -Path $extPath -Name "Content Type" -ErrorAction SilentlyContinue
                Write-Host "    Reset $ext default association" -ForegroundColor Gray
            }
        }
    }
    
    # Remove approved extensions
    $approvedPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved"
    if (Test-Path $approvedPath) {
        $clsids = @(
            "{A1B2C3D4-E5F6-7A8B-9C0D-1E2F3A4B5C6D}",
            "{B2C3D4E5-F6A7-8B9C-0D1E-2F3A4B5C6D7E}",
            "{C3D4E5F6-A7B8-9C0D-1E2F-3A4B5C6D7E8F}",
            "{D4E5F6A7-B8C9-0D1E-2F3A-4B5C6D7E8F9A}",
            "{E5F6A7B8-C9D0-1E2F-3A4B-5C6D7E8F9A0B}"
        )
        foreach ($clsid in $clsids) {
            Remove-ItemProperty -Path $approvedPath -Name $clsid -ErrorAction SilentlyContinue
        }
    }
    
    Write-Host "    [OK] Registry cleaned" -ForegroundColor Green

    # Delete files
    Write-Host "  > Removing files..." -ForegroundColor White
    foreach ($dllPath in $InstallLocations) {
        if (Test-Path $dllPath) {
            Remove-Item $dllPath -Force -ErrorAction SilentlyContinue
            $pdbPath = $dllPath -replace "\.dll$", ".pdb"
            if (Test-Path $pdbPath) {
                Remove-Item $pdbPath -Force -ErrorAction SilentlyContinue
            }
            Write-Host "    Removed: $dllPath" -ForegroundColor Gray
        }
    }
    Write-Host "    [OK] Files removed" -ForegroundColor Green

    # Notify shell
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Shell32Uninstall {
    [DllImport("shell32.dll")]
    public static extern void SHChangeNotify(int wEventId, int uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
"@
    [Shell32Uninstall]::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero)

    Write-Host ""
    Write-Host "  [SUCCESS] SQLiteView uninstalled!" -ForegroundColor Green
    Write-Host ""
}
catch {
    Write-Host ""
    Write-Host "  [ERROR] $($_.Exception.Message)" -ForegroundColor Red
    Write-Host ""
    if (-not $Force) {
        throw
    }
}
finally {
    # Restart Explorer
    Write-Host "  > Restarting Explorer..." -ForegroundColor White
    Start-Process "explorer.exe"
    Start-Sleep -Seconds 2
    Write-Host "    [OK]" -ForegroundColor Green
    Write-Host ""
}
