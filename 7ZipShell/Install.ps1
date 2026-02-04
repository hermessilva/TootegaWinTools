<#
.SYNOPSIS
    SevenZipView Installer

.DESCRIPTION
    Installs the SevenZipView Windows Explorer Shell Extension.
    Run as Administrator for best results.

.PARAMETER SourceDll
    Path to the DLL to install. If not specified, uses x64\Release\SevenZipView.dll

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
Write-Host "              SevenZipView Installer" -ForegroundColor Cyan
Write-Host "    Windows Explorer Shell Extension for 7-Zip Archives" -ForegroundColor Cyan
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
    $SourceDll = Join-Path $PSScriptRoot "x64\Release\SevenZipView.dll"
}

if (-not (Test-Path $SourceDll)) {
    Write-Host "  [ERROR] DLL not found: $SourceDll" -ForegroundColor Red
    Write-Host ""
    Write-Host "  Please build the project first:" -ForegroundColor Yellow
    Write-Host "    MSBuild SevenZipView\SevenZipView.vcxproj /p:Configuration=Release /p:Platform=x64" -ForegroundColor Gray
    Write-Host ""
    exit 1
}

Write-Host "  Source DLL: $SourceDll" -ForegroundColor Gray

# Installation directory
$InstallDir = "C:\Program Files\Tootega"
$TargetDll = Join-Path $InstallDir "SevenZipView.dll"

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
        Copy-Item -Path $SourcePdb -Destination (Join-Path $InstallDir "SevenZipView.pdb") -Force
    }
    Write-Host "    [OK]" -ForegroundColor Green

    # Register DLL
    Write-Host "  > Registering DLL..." -ForegroundColor White
    $result = & regsvr32 /s "$TargetDll" 2>&1
    Start-Sleep -Seconds 1
    
    # Verify registration
    $clsidPath = "HKCU:\Software\Classes\CLSID\{7A8B9C0D-1E2F-3A4B-5C6D-7E8F9A0B1C2D}"
    if (Test-Path $clsidPath) {
        Write-Host "    [OK] CLSID registered" -ForegroundColor Green
    } else {
        Write-Host "    [WARN] CLSID may not be registered" -ForegroundColor Yellow
    }

    # Verify .7z association
    $ext7z = "HKCU:\Software\Classes\.7z"
    if (Test-Path $ext7z) {
        $progId = (Get-ItemProperty -Path $ext7z -ErrorAction SilentlyContinue).'(default)'
        Write-Host "    [OK] .7z -> $progId" -ForegroundColor Green
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
    Write-Host "  [SUCCESS] SevenZipView installed!" -ForegroundColor Green
    Write-Host ""
    Write-Host "  ============================================================" -ForegroundColor Yellow
    Write-Host "  IMPORTANT: To set SevenZipView as default for .7z files:" -ForegroundColor Yellow
    Write-Host "  ============================================================" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "    1. Right-click any .7z file" -ForegroundColor White
    Write-Host "    2. Select 'Open with' -> 'Choose another app'" -ForegroundColor White
    Write-Host "    3. Click 'More apps' and scroll down" -ForegroundColor White
    Write-Host "    4. Click 'Look for another app on this PC'" -ForegroundColor White
    Write-Host "    5. Navigate to: C:\Windows\explorer.exe" -ForegroundColor White
    Write-Host "    6. Check 'Always use this app'" -ForegroundColor White
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
