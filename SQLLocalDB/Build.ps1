<#
.SYNOPSIS
    Quick Build Script for SQLLocalDBView

.DESCRIPTION
    Builds the SQLLocalDBView project using CMake.

.PARAMETER Configuration
    Build configuration (Debug or Release). Default: Release

.PARAMETER Clean
    Clean build directory before building.

.NOTES
    Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
#>

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"

Write-Host ""
Write-Host "🔨 Building SQLLocalDBView ($Configuration)..." -ForegroundColor Cyan
Write-Host ""

# Clean if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "  Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item $BuildDir -Recurse -Force
}

# Create build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -Path $BuildDir -ItemType Directory | Out-Null
}

Push-Location $BuildDir

try {
    # Check if we need to configure
    if (-not (Test-Path "CMakeCache.txt")) {
        Write-Host "  Configuring CMake..." -ForegroundColor Yellow
        cmake .. -G "Visual Studio 17 2022" -A x64
        
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed"
        }
    }
    
    # Build
    Write-Host "  Building..." -ForegroundColor Yellow
    cmake --build . --config $Configuration --parallel
    
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }
    
    $dllPath = Join-Path $BuildDir "$Configuration\SQLLocalDBView.dll"
    
    Write-Host ""
    Write-Host "✓ Build successful!" -ForegroundColor Green
    
    if (Test-Path $dllPath) {
        $fileInfo = Get-Item $dllPath
        Write-Host ""
        Write-Host "  Output: $dllPath" -ForegroundColor Cyan
        Write-Host "  Size:   $([math]::Round($fileInfo.Length / 1KB, 1)) KB" -ForegroundColor Gray
    }
    
    Write-Host ""
    Write-Host "To install, run as Administrator:" -ForegroundColor White
    Write-Host "  .\Install.ps1" -ForegroundColor Cyan
    Write-Host ""
}
catch {
    Write-Host ""
    Write-Host "✗ Build failed: $_" -ForegroundColor Red
    Write-Host ""
    exit 1
}
finally {
    Pop-Location
}
