<#
.SYNOPSIS
    SevenZipView Uninstaller

.DESCRIPTION
    Removes the SevenZipView Windows Explorer Shell Extension and all registry entries.

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
Write-Host "              SevenZipView Uninstaller" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# Possible installation locations
$InstallLocations = @(
    "C:\Program Files\Tootega\SevenZipView.dll",
    "$env:LOCALAPPDATA\Tootega\SevenZipView.dll",
    "$PSScriptRoot\x64\Release\SevenZipView.dll",
    "$PSScriptRoot\x64\Debug\SevenZipView.dll"
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
        "HKCU:\Software\Classes\SevenZipView.Archive",
        "HKCU:\Software\Classes\CLSID\{7A8B9C0D-1E2F-3A4B-5C6D-7E8F9A0B1C2D}",
        "HKCU:\Software\Classes\CLSID\{8B9C0D1E-2F3A-4B5C-6D7E-8F9A0B1C2D3E}",
        "HKCU:\Software\Classes\CLSID\{9C0D1E2F-3A4B-5C6D-7E8F-9A0B1C2D3E4F}",
        "HKCU:\Software\Classes\CLSID\{0D1E2F3A-4B5C-6D7E-8F9A-0B1C2D3E4F5A}",
        "HKCU:\Software\Classes\CLSID\{1E2F3A4B-5C6D-7E8F-9A0B-1C2D3E4F5A6B}",
        "HKCU:\Software\Classes\.7z\ShellEx",
        "HKCU:\Software\Classes\.7z\SevenZipView.Archive",
        "HKCU:\Software\Classes\.7z\OpenWithProgids"
    )
    
    foreach ($key in $keysToDelete) {
        if (Test-Path $key) {
            Remove-Item -Path $key -Recurse -Force -ErrorAction SilentlyContinue
            Write-Host "    Removed: $key" -ForegroundColor Gray
        }
    }
    
    # Reset .7z association to remove our ProgId
    $ext7zPath = "HKCU:\Software\Classes\.7z"
    if (Test-Path $ext7zPath) {
        $currentProgId = (Get-ItemProperty -Path $ext7zPath -ErrorAction SilentlyContinue).'(default)'
        if ($currentProgId -eq "SevenZipView.Archive") {
            Remove-ItemProperty -Path $ext7zPath -Name "(default)" -ErrorAction SilentlyContinue
            Write-Host "    Reset .7z default association" -ForegroundColor Gray
        }
    }
    
    # Remove approved extensions
    $approvedPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved"
    if (Test-Path $approvedPath) {
        $clsids = @(
            "{7A8B9C0D-1E2F-3A4B-5C6D-7E8F9A0B1C2D}",
            "{8B9C0D1E-2F3A-4B5C-6D7E-8F9A0B1C2D3E}",
            "{9C0D1E2F-3A4B-5C6D-7E8F-9A0B1C2D3E4F}",
            "{0D1E2F3A-4B5C-6D7E-8F9A-0B1C2D3E4F5A}",
            "{1E2F3A4B-5C6D-7E8F-9A0B-1C2D3E4F5A6B}"
        )
        foreach ($clsid in $clsids) {
            Remove-ItemProperty -Path $approvedPath -Name $clsid -ErrorAction SilentlyContinue
        }
    }
    
    Write-Host "    [OK] Registry cleaned" -ForegroundColor Green
    
    # Delete DLL files
    Write-Host "  > Removing files..." -ForegroundColor White
    foreach ($dllPath in $InstallLocations) {
        if (Test-Path $dllPath) {
            $dir = Split-Path $dllPath -Parent
            Remove-Item $dllPath -Force -ErrorAction SilentlyContinue
            
            $pdbPath = $dllPath -replace "\.dll$", ".pdb"
            if (Test-Path $pdbPath) {
                Remove-Item $pdbPath -Force -ErrorAction SilentlyContinue
            }
            
            # Remove directory if empty
            $remaining = Get-ChildItem $dir -ErrorAction SilentlyContinue
            if (-not $remaining -and $dir -match "Tootega$") {
                Remove-Item $dir -Force -ErrorAction SilentlyContinue
            }
            Write-Host "    Removed: $dllPath" -ForegroundColor Gray
        }
    }
    Write-Host "    [OK] Files removed" -ForegroundColor Green
    
    # Notify shell of changes
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Shell32Notify {
    [DllImport("shell32.dll")]
    public static extern void SHChangeNotify(int wEventId, int uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
"@
    [Shell32Notify]::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero)
    
    Write-Host ""
    Write-Host "  [SUCCESS] SevenZipView uninstalled successfully!" -ForegroundColor Green
    Write-Host ""
}
catch {
    Write-Host ""
    Write-Host "  [ERROR] $($_.Exception.Message)" -ForegroundColor Red
    Write-Host ""
}
finally {
    # Restart Explorer
    if ($explorerWasRunning) {
        Write-Host "  > Restarting Explorer..." -ForegroundColor White
        Start-Process "explorer.exe"
        Start-Sleep -Seconds 2
        Write-Host "    [OK] Explorer restarted" -ForegroundColor Green
    }
}

Write-Host ""
