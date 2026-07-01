<#
.SYNOPSIS
    SQLiteView Setup - Downloads SQLite and prepares build environment

.DESCRIPTION
    This script downloads the SQLite amalgamation and sets up the build environment
    for the SQLiteView Windows Explorer Shell Extension.

.NOTES
    Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
#>

[CmdletBinding()]
param(
    [switch]$Force,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

# Configuration
$SQLiteVersion = "3510200"
$SQLiteUrl = "https://www.sqlite.org/2024/sqlite-amalgamation-$SQLiteVersion.zip"
$ProjectRoot = $PSScriptRoot
$SQLiteDir = Join-Path $ProjectRoot "sqlite"
$BuildDir = Join-Path $ProjectRoot "build"

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║             SQLiteView - Setup Script                          ║" -ForegroundColor Cyan
Write-Host "║     Windows Explorer Shell Extension for SQLite Databases      ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Check if SQLite amalgamation exists
$sqlite3c = Join-Path $SQLiteDir "sqlite3.c"
$sqlite3h = Join-Path $SQLiteDir "sqlite3.h"

if ((Test-Path $sqlite3c) -and -not $Force) {
    $content = Get-Content $sqlite3c -First 5 -Raw
    if ($content -notmatch "#error") {
        Write-Host "✓ SQLite amalgamation already present" -ForegroundColor Green
        $skipDownload = $true
    }
}

if (-not $skipDownload) {
    Write-Host "📥 Downloading SQLite amalgamation..." -ForegroundColor Yellow
    
    $tempZip = Join-Path $env:TEMP "sqlite-amalgamation.zip"
    $tempDir = Join-Path $env:TEMP "sqlite-amalgamation-$SQLiteVersion"
    
    try {
        # Download
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        
        $webClient = New-Object System.Net.WebClient
        $webClient.DownloadFile($SQLiteUrl, $tempZip)
        
        Write-Host "  Downloaded to $tempZip" -ForegroundColor Gray
        
        # Extract
        if (Test-Path $tempDir) {
            Remove-Item $tempDir -Recurse -Force
        }
        
        Expand-Archive -Path $tempZip -DestinationPath $env:TEMP -Force
        
        # Copy files
        if (-not (Test-Path $SQLiteDir)) {
            New-Item -Path $SQLiteDir -ItemType Directory | Out-Null
        }
        
        Copy-Item (Join-Path $tempDir "sqlite3.c") $SQLiteDir -Force
        Copy-Item (Join-Path $tempDir "sqlite3.h") $SQLiteDir -Force
        Copy-Item (Join-Path $tempDir "shell.c") $SQLiteDir -Force -ErrorAction SilentlyContinue
        Copy-Item (Join-Path $tempDir "sqlite3ext.h") $SQLiteDir -Force -ErrorAction SilentlyContinue
        
        Write-Host "✓ SQLite amalgamation downloaded and extracted" -ForegroundColor Green
        
        # Cleanup
        Remove-Item $tempZip -Force -ErrorAction SilentlyContinue
        Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    catch {
        Write-Host "✗ Failed to download SQLite: $_" -ForegroundColor Red
        Write-Host ""
        Write-Host "You can manually download from:" -ForegroundColor Yellow
        Write-Host "  $SQLiteUrl" -ForegroundColor Cyan
        Write-Host "Extract sqlite3.c and sqlite3.h to the 'sqlite' folder." -ForegroundColor Yellow
        exit 1
    }
}

# Check for Visual Studio / Build Tools
Write-Host ""
Write-Host "🔍 Checking build environment..." -ForegroundColor Yellow

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$hasVS = $false

if (Test-Path $vsWhere) {
    $vsInstalls = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsInstalls) {
        Write-Host "✓ Visual Studio found: $vsInstalls" -ForegroundColor Green
        $hasVS = $true
    }
}

if (-not $hasVS) {
    Write-Host "⚠ Visual Studio with C++ not found" -ForegroundColor Yellow
    Write-Host "  Install Visual Studio 2022 with 'Desktop development with C++' workload" -ForegroundColor Gray
}

# Check for CMake
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) {
    Write-Host "✓ CMake found: $($cmake.Version)" -ForegroundColor Green
} else {
    Write-Host "⚠ CMake not found. Install from https://cmake.org/download/" -ForegroundColor Yellow
}

# Build if requested
if (-not $SkipBuild -and $hasVS -and $cmake) {
    Write-Host ""
    Write-Host "🔨 Building project..." -ForegroundColor Yellow
    
    if (-not (Test-Path $BuildDir)) {
        New-Item -Path $BuildDir -ItemType Directory | Out-Null
    }
    
    Push-Location $BuildDir
    try {
        # Configure
        Write-Host "  Configuring with CMake..." -ForegroundColor Gray
        cmake .. -G "Visual Studio 17 2022" -A x64
        
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed"
        }
        
        # Build
        Write-Host "  Building Release configuration..." -ForegroundColor Gray
        cmake --build . --config Release
        
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed"
        }
        
        Write-Host "✓ Build successful!" -ForegroundColor Green
        
        $dllPath = Join-Path $BuildDir "Release\SQLiteView.dll"
        if (Test-Path $dllPath) {
            Write-Host ""
            Write-Host "  Output: $dllPath" -ForegroundColor Cyan
        }
    }
    catch {
        Write-Host "✗ Build failed: $_" -ForegroundColor Red
    }
    finally {
        Pop-Location
    }
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Gray
Write-Host ""
Write-Host "Next steps:" -ForegroundColor White
Write-Host "  1. Build the project (if not already built):" -ForegroundColor Gray
Write-Host "     cd build && cmake --build . --config Release" -ForegroundColor Cyan
Write-Host ""
Write-Host "  2. Install the extension (as Administrator):" -ForegroundColor Gray
Write-Host "     .\Install.ps1" -ForegroundColor Cyan
Write-Host ""
Write-Host "  3. Open any .db file in Windows Explorer!" -ForegroundColor Gray
Write-Host ""
