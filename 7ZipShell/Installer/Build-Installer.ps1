<#
.SYNOPSIS
    Build SevenZipView MSI Installer

.DESCRIPTION
    Builds the SevenZipView MSI installer using WiX Toolset.
    
    Prerequisites:
    - WiX Toolset 3.11 or later (install via: winget install WixToolset.WiX)
    - Or .NET SDK with WiX extension (for WiX 4+)
    - SevenZipView.dll must be built first (x64\Release)

.PARAMETER Configuration
    Build configuration (Release or Debug). Default is Release.

.EXAMPLE
    .\Build-Installer.ps1
    .\Build-Installer.ps1 -Configuration Debug

.NOTES
    Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
#>

[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
$RootDir = Split-Path $ScriptDir -Parent
$OutputDir = Join-Path $ScriptDir "bin\$Configuration"

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "        SevenZipView MSI Installer Build" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# Check if DLL exists
$DllPath = Join-Path $RootDir "x64\$Configuration\SevenZipView.dll"
if (-not (Test-Path $DllPath)) {
    Write-Host "  [ERROR] DLL not found: $DllPath" -ForegroundColor Red
    Write-Host ""
    Write-Host "  Please build the main project first:" -ForegroundColor Yellow
    Write-Host "    MSBuild SevenZipView\SevenZipView.vcxproj /p:Configuration=$Configuration /p:Platform=x64" -ForegroundColor Gray
    Write-Host ""
    exit 1
}
Write-Host "  [OK] Source DLL found: $DllPath" -ForegroundColor Green

# Create output directory
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Find WiX Toolset
$WixPaths = @(
    "${env:ProgramFiles(x86)}\WiX Toolset v3.11\bin",
    "${env:ProgramFiles(x86)}\WiX Toolset v3.14\bin",
    "${env:ProgramFiles}\WiX Toolset v3.11\bin",
    "${env:ProgramFiles}\WiX Toolset v3.14\bin",
    "$env:WIX\bin",
    "$env:USERPROFILE\.dotnet\tools"
)

$WixPath = $null
foreach ($path in $WixPaths) {
    $candlePath = Join-Path $path "candle.exe"
    if (Test-Path $candlePath) {
        $WixPath = $path
        break
    }
}

if (-not $WixPath) {
    Write-Host "  [ERROR] WiX Toolset not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "  Please install WiX Toolset:" -ForegroundColor Yellow
    Write-Host "    Option 1: winget install WixToolset.WiX" -ForegroundColor Gray
    Write-Host "    Option 2: Download from https://wixtoolset.org/releases/" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  Or use the dotnet-based build (requires .NET SDK):" -ForegroundColor Yellow
    Write-Host "    dotnet tool install --global wix" -ForegroundColor Gray
    Write-Host "    dotnet build Installer\SevenZipViewInstaller.wixproj" -ForegroundColor Gray
    Write-Host ""
    exit 1
}

Write-Host "  [OK] WiX Toolset found: $WixPath" -ForegroundColor Green

$Candle = Join-Path $WixPath "candle.exe"
$Light = Join-Path $WixPath "light.exe"

# Create temporary icon if it doesn't exist
$IconPath = Join-Path $ScriptDir "Resources\SevenZipView.ico"
if (-not (Test-Path $IconPath)) {
    Write-Host "  [WARN] Icon not found, creating placeholder..." -ForegroundColor Yellow
    # Copy from resources or create a placeholder
    $ResourcesDir = Join-Path $ScriptDir "Resources"
    if (-not (Test-Path $ResourcesDir)) {
        New-Item -ItemType Directory -Path $ResourcesDir -Force | Out-Null
    }
    # Try to extract icon from DLL
    Add-Type -AssemblyName System.Drawing
    try {
        $icon = [System.Drawing.Icon]::ExtractAssociatedIcon($DllPath)
        $fs = [System.IO.File]::Create($IconPath)
        $icon.Save($fs)
        $fs.Close()
        Write-Host "  [OK] Extracted icon from DLL" -ForegroundColor Green
    } catch {
        Write-Host "  [WARN] Could not extract icon, using shell32 default" -ForegroundColor Yellow
        # Use a system icon as fallback
        $shell32 = "$env:SystemRoot\System32\shell32.dll"
        Copy-Item "$env:SystemRoot\System32\imageres.dll" -ErrorAction SilentlyContinue
    }
}

# Build WiX
Write-Host ""
Write-Host "  > Compiling WiX source (candle.exe)..." -ForegroundColor White

$WxsFile = Join-Path $ScriptDir "Product.wxs"
$WixObjFile = Join-Path $OutputDir "Product.wixobj"
$MsiFile = Join-Path $OutputDir "SevenZipViewInstaller.msi"

# Run candle (compiler)
$candleArgs = @(
    "-nologo",
    "-arch", "x64",
    "-ext", "WixUIExtension",
    "-ext", "WixUtilExtension",
    "-dConfiguration=$Configuration",
    "-out", $WixObjFile,
    $WxsFile
)

Write-Host "    candle.exe $($candleArgs -join ' ')" -ForegroundColor Gray
& $Candle @candleArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "  [ERROR] Candle failed with exit code $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "    [OK] Compiled" -ForegroundColor Green

# Run light (linker)
Write-Host "  > Linking MSI (light.exe)..." -ForegroundColor White

$lightArgs = @(
    "-nologo",
    "-ext", "WixUIExtension",
    "-ext", "WixUtilExtension",
    "-cultures:en-us",
    "-out", $MsiFile,
    $WixObjFile
)

Write-Host "    light.exe $($lightArgs -join ' ')" -ForegroundColor Gray
& $Light @lightArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "  [ERROR] Light failed with exit code $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "    [OK] Linked" -ForegroundColor Green

# Verify output
if (Test-Path $MsiFile) {
    $fileInfo = Get-Item $MsiFile
    Write-Host ""
    Write-Host "  ================================================================" -ForegroundColor Green
    Write-Host "  [SUCCESS] MSI installer created!" -ForegroundColor Green
    Write-Host "  ================================================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Output: $MsiFile" -ForegroundColor White
    Write-Host "  Size:   $([math]::Round($fileInfo.Length / 1KB, 2)) KB" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  To install:" -ForegroundColor Yellow
    Write-Host "    msiexec /i `"$MsiFile`"" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  To uninstall:" -ForegroundColor Yellow
    Write-Host "    msiexec /x `"$MsiFile`"" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  Silent install:" -ForegroundColor Yellow
    Write-Host "    msiexec /i `"$MsiFile`" /qn" -ForegroundColor Gray
    Write-Host ""
} else {
    Write-Host "  [ERROR] MSI file was not created!" -ForegroundColor Red
    exit 1
}
