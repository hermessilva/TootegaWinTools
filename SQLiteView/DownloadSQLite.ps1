# DownloadSQLite.ps1 - Pre-build script to download SQLite amalgamation
# Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
# This script is automatically executed by Visual Studio before build.

$ErrorActionPreference = "SilentlyContinue"
$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SqliteDir = Join-Path $ProjectDir "sqlite"
$SqliteC = Join-Path $SqliteDir "sqlite3.c"

# Check if sqlite3.c already exists and is valid (not the placeholder)
if (Test-Path $SqliteC) {
    $content = Get-Content $SqliteC -First 5 -ErrorAction SilentlyContinue
    $contentStr = $content -join "`n"
    
    if ($contentStr -notmatch "#error") {
        # Valid sqlite3.c exists
        exit 0
    }
}

Write-Host "Downloading SQLite amalgamation..."

# Create sqlite directory if needed
if (-not (Test-Path $SqliteDir)) {
    New-Item -Path $SqliteDir -ItemType Directory -Force | Out-Null
}

# SQLite version and URL - Version 3.51.2 (January 2026)
$SqliteVersion = "3510200"
$SqliteUrl = "https://www.sqlite.org/2026/sqlite-amalgamation-$SqliteVersion.zip"
$TempZip = Join-Path $env:TEMP "sqlite-amalgamation.zip"
$TempDir = Join-Path $env:TEMP "sqlite-temp"

try {
    # Download
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    
    $webClient = New-Object System.Net.WebClient
    $webClient.DownloadFile($SqliteUrl, $TempZip)
    
    # Extract
    if (Test-Path $TempDir) {
        Remove-Item $TempDir -Recurse -Force
    }
    
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory($TempZip, $TempDir)
    
    # Find and copy files
    $extractedDir = Get-ChildItem $TempDir -Directory | Select-Object -First 1
    
    if ($extractedDir) {
        $srcPath = $extractedDir.FullName
        
        # Copy sqlite3.c
        $srcC = Join-Path $srcPath "sqlite3.c"
        if (Test-Path $srcC) {
            Copy-Item $srcC $SqliteC -Force
            Write-Host "  sqlite3.c copied successfully"
        }
        
        # Copy sqlite3.h (overwrite placeholder with full version)
        $srcH = Join-Path $srcPath "sqlite3.h"
        $dstH = Join-Path $SqliteDir "sqlite3.h"
        if (Test-Path $srcH) {
            Copy-Item $srcH $dstH -Force
            Write-Host "  sqlite3.h copied successfully"
        }
    }
    
    # Cleanup
    Remove-Item $TempZip -Force -ErrorAction SilentlyContinue
    Remove-Item $TempDir -Recurse -Force -ErrorAction SilentlyContinue
    
    Write-Host "SQLite amalgamation downloaded successfully."
    exit 0
}
catch {
    Write-Host "WARNING: Failed to download SQLite amalgamation: $_"
    Write-Host "Please download manually from https://www.sqlite.org/download.html"
    Write-Host "Extract sqlite3.c and sqlite3.h to: $SqliteDir"
    
    # Don't fail the build - let it fail on compilation with a clearer error
    exit 0
}
