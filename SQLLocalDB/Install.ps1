<#
.SYNOPSIS
    SQLLocalDBView Installer

.DESCRIPTION
    Installs the SQLLocalDBView Windows Explorer Shell Extension.
    This script must be run as Administrator.

.PARAMETER Uninstall
    Uninstalls the extension instead of installing.

.PARAMETER Force
    Forces installation even if processes cannot be stopped.

.PARAMETER KeepSource
    Does not delete the source DLL after installation.

.NOTES
    Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
#>

[CmdletBinding()]
param(
    [switch]$Uninstall,
    [switch]$Force,
    [switch]$KeepSource
)

$ErrorActionPreference = "Stop"

# Configuration
$ExtensionName = "SQLLocalDBView"
$ProjectRoot = $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build\Release"
$DllName = "SQLLocalDBView.dll"
$InstallDir = "C:\Program Files\Tootega"
$TargetDll = Join-Path $InstallDir $DllName
$TargetPdb = Join-Path $InstallDir "SQLLocalDBView.pdb"

# Processes that may lock the DLL
$ShellProcesses = @("explorer", "prevhost", "dllhost", "SearchProtocolHost", "SearchFilterHost", "ApplicationFrameHost")

# Helper Functions
function Show-Header {
    param([string]$Title)
    Write-Host ""
    Write-Host "================================================================" -ForegroundColor Cyan
    Write-Host "              SQLLocalDBView $Title" -ForegroundColor Cyan
    Write-Host "    Windows Explorer Shell Extension for SQL LocalDB" -ForegroundColor Cyan
    Write-Host "================================================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Install Directory: $InstallDir" -ForegroundColor Gray
    Write-Host ""
}

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Write-Step { param([string]$Message) Write-Host "  > $Message" -ForegroundColor White }
function Write-OK { param([string]$Message) Write-Host "    [OK] $Message" -ForegroundColor Green }
function Write-Warn { param([string]$Message) Write-Host "    [WARN] $Message" -ForegroundColor Yellow }
function Write-Fail { param([string]$Message) Write-Host "    [FAIL] $Message" -ForegroundColor Red }

function Stop-ProcessesSafely {
    Write-Step "Stopping processes that may lock the DLL..."
    
    $explorerWasRunning = $false
    
    # Kill non-critical processes first
    foreach ($procName in @("prevhost", "dllhost", "SearchProtocolHost", "SearchFilterHost", "ApplicationFrameHost")) {
        $procs = Get-Process -Name $procName -ErrorAction SilentlyContinue
        if ($procs) {
            Write-Host "    Stopping $procName.exe..." -ForegroundColor Gray
            Stop-Process -Name $procName -Force -ErrorAction SilentlyContinue
            Start-Sleep -Milliseconds 500
        }
    }
    
    # Kill Explorer last
    $explorer = Get-Process -Name "explorer" -ErrorAction SilentlyContinue
    if ($explorer) {
        $explorerWasRunning = $true
        Write-Host "    Stopping explorer.exe..." -ForegroundColor Gray
        Stop-Process -Name "explorer" -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 2
    }
    
    Write-OK "Processes stopped"
    return $explorerWasRunning
}

function Start-ExplorerIfNeeded {
    param([bool]$WasRunning = $true)
    
    if ($WasRunning) {
        $explorer = Get-Process -Name "explorer" -ErrorAction SilentlyContinue
        if (-not $explorer) {
            Write-Step "Restarting Windows Explorer..."
            Start-Process "explorer.exe"
            Start-Sleep -Seconds 2
            Write-OK "Explorer restarted"
        }
    }
}

function Remove-FileWithRetry {
    param([string]$Path, [int]$MaxAttempts = 5, [int]$DelayMs = 1000)
    
    if (-not (Test-Path $Path)) { return $true }
    
    for ($i = 1; $i -le $MaxAttempts; $i++) {
        try {
            $file = Get-Item $Path -Force
            if ($file.Attributes -band [System.IO.FileAttributes]::ReadOnly) {
                $file.Attributes = $file.Attributes -bxor [System.IO.FileAttributes]::ReadOnly
            }
            Remove-Item $Path -Force -ErrorAction Stop
            return $true
        }
        catch {
            if ($i -lt $MaxAttempts) {
                Write-Host "      Attempt $i/$MaxAttempts failed, retrying..." -ForegroundColor Gray
                Start-Sleep -Milliseconds $DelayMs
                Stop-Process -Name "prevhost" -Force -ErrorAction SilentlyContinue
                Stop-Process -Name "dllhost" -Force -ErrorAction SilentlyContinue
            }
        }
    }
    return $false
}

function Unregister-Extension {
    param([string]$DllPath)
    
    if (-not (Test-Path $DllPath)) { return $true }
    
    Write-Host "    Unregistering DLL..." -ForegroundColor Gray
    
    & regsvr32 /u /s "$DllPath"
    Start-Sleep -Milliseconds 500
    
    # Verify unregistration - check both old (HKCR) and new (HKCU) locations
    $hkcuExists = reg query "HKCU\Software\Classes\CLSID\{E8A3C5B7-9D2F-4A1E-8B6C-3D5E7F9A2C4D}" 2>&1
    $hkcrExists = reg query "HKCR\CLSID\{E8A3C5B7-9D2F-4A1E-8B6C-3D5E7F9A2C4D}" 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-OK "DLL unregistered"
        return $true
    } else {
        Write-Warn "Registry keys may still be present after unregister"
        return $false
    }
}

function Register-Extension {
    param([string]$DllPath)
    
    if (-not (Test-Path $DllPath)) { throw "DLL not found: $DllPath" }
    
    Write-Host "    Registering DLL..." -ForegroundColor Gray
    
    # Run regsvr32 and wait
    & regsvr32 /s "$DllPath"
    Start-Sleep -Milliseconds 1000
    
    # Verify by checking if key was created in HKCU\Software\Classes (per-user registration)
    # The new DllMain.cpp uses HKCU\Software\Classes instead of HKCR
    $null = reg query "HKCU\Software\Classes\CLSID\{E8A3C5B7-9D2F-4A1E-8B6C-3D5E7F9A2C4D}" 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-OK "DLL registered successfully (per-user)"
        return $true
    } else {
        # Try to show error by running without /s
        Write-Warn "Silent registration may have failed. Trying with dialog..."
        & regsvr32 "$DllPath"
        Start-Sleep -Milliseconds 500
        
        $null = reg query "HKCU\Software\Classes\CLSID\{E8A3C5B7-9D2F-4A1E-8B6C-3D5E7F9A2C4D}" 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-OK "DLL registered successfully (per-user)"
            return $true
        }
        
        # Show debug log location
        Write-Host ""
        Write-Host "    Check debug log for details:" -ForegroundColor Yellow
        Write-Host "      $env:TEMP\SQLLocalDBView_Debug.log" -ForegroundColor Gray
        Write-Host ""
        throw "Failed to register DLL."
    }
}

function Show-RegistrationStatus {
    Write-Step "Verifying registration..."
    
    # CLSIDs now registered under HKCU\Software\Classes (per-user, no admin required)
    # The new DllMain.cpp uses HKEY_CURRENT_USER instead of HKEY_CLASSES_ROOT
    $checks = @(
        @{ Name = "ShellFolder CLSID"; Key = "HKCU\Software\Classes\CLSID\{E8A3C5B7-9D2F-4A1E-8B6C-3D5E7F9A2C4D}" },
        @{ Name = "PreviewHandler CLSID"; Key = "HKCU\Software\Classes\CLSID\{F9B4D6C8-AE3F-5B2F-9C7D-4E6F8A1B3D5E}" },
        @{ Name = "ContextMenu CLSID"; Key = "HKCU\Software\Classes\CLSID\{B2D6F8A1-CF5B-7D4E-BF9A-6C8E1B3D5F7A}" },
        @{ Name = "Property CLSID"; Key = "HKCU\Software\Classes\CLSID\{A1C5E7F9-BF4A-6C3D-AE8F-5B7D9A2C4E6F}" },
        @{ Name = "ProgId"; Key = "HKCU\Software\Classes\SQLLocalDBView.Database" },
        @{ Name = ".mdf Extension"; Key = "HKCU\Software\Classes\.mdf" }
    )
    
    foreach ($check in $checks) {
        $null = reg query $check.Key 2>$null
        if ($LASTEXITCODE -eq 0) {
            Write-OK $check.Name
        } else {
            Write-Fail "$($check.Name) - NOT FOUND"
        }
    }
}

# Main Installation
function Install-SQLLocalDBView {
    Show-Header "Installer"
    
    if (-not (Test-Administrator)) {
        Write-Fail "This script requires Administrator privileges."
        Write-Host "    Please run PowerShell as Administrator and try again." -ForegroundColor Yellow
        exit 1
    }
    Write-OK "Running as Administrator"
    
    # Find source DLL
    Write-Step "Locating source DLL..."
    
    $sourceDll = Join-Path $BuildDir $DllName
    $sourcePdb = Join-Path $BuildDir "SQLLocalDBView.pdb"
    
    if (-not (Test-Path $sourceDll)) {
        $altLocations = @(
            (Join-Path $ProjectRoot $DllName),
            (Join-Path $ProjectRoot "bin\Release\$DllName"),
            (Join-Path $ProjectRoot "x64\Release\$DllName")
        )
        foreach ($alt in $altLocations) {
            if (Test-Path $alt) {
                $sourceDll = $alt
                $sourcePdb = [IO.Path]::ChangeExtension($alt, ".pdb")
                break
            }
        }
    }
    
    if (-not (Test-Path $sourceDll)) {
        Write-Fail "Cannot find $DllName"
        Write-Host ""
        Write-Host "    Please build the project first:" -ForegroundColor Yellow
        Write-Host "      cd build" -ForegroundColor Cyan
        Write-Host "      cmake --build . --config Release" -ForegroundColor Cyan
        Write-Host ""
        exit 1
    }
    
    Write-OK "Found: $sourceDll"
    $dllInfo = Get-Item $sourceDll
    Write-Host "    Size: $([math]::Round($dllInfo.Length / 1KB, 2)) KB | Modified: $($dllInfo.LastWriteTime)" -ForegroundColor Gray
    
    $explorerWasRunning = Stop-ProcessesSafely
    
    try {
        # Remove previous installation
        Write-Step "Removing previous installation..."
        
        if (Test-Path $TargetDll) {
            Unregister-Extension -DllPath $TargetDll
            
            if (-not (Remove-FileWithRetry -Path $TargetDll)) {
                if ($Force) {
                    Write-Warn "Could not remove old DLL, continuing anyway (-Force)"
                } else {
                    throw "Cannot remove existing DLL. Try with -Force or reboot."
                }
            } else {
                Write-OK "Previous DLL removed"
            }
        } else {
            Write-OK "No previous installation found"
        }
        
        if (Test-Path $TargetPdb) {
            Remove-FileWithRetry -Path $TargetPdb | Out-Null
        }
        
        # Create directory
        Write-Step "Creating installation directory..."
        if (-not (Test-Path $InstallDir)) {
            New-Item -Path $InstallDir -ItemType Directory -Force | Out-Null
            Write-OK "Created: $InstallDir"
        } else {
            Write-OK "Directory exists: $InstallDir"
        }
        
        # Copy files
        Write-Step "Copying files..."
        Copy-Item $sourceDll $TargetDll -Force
        Write-OK "Copied $DllName"
        
        if (Test-Path $sourcePdb) {
            Copy-Item $sourcePdb $InstallDir -Force -ErrorAction SilentlyContinue
            Write-OK "Copied SQLLocalDBView.pdb"
        }
        
        # Register
        Write-Step "Registering shell extension..."
        Register-Extension -DllPath $TargetDll
        
        # Verify
        Show-RegistrationStatus
        
        # Delete source DLL
        if (-not $KeepSource) {
            Write-Step "Cleaning up source files..."
            if (Remove-FileWithRetry -Path $sourceDll -MaxAttempts 3 -DelayMs 500) {
                Write-OK "Source DLL deleted (rebuild required for next install)"
            } else {
                Write-Warn "Could not delete source DLL"
            }
            if (Test-Path $sourcePdb) {
                Remove-Item $sourcePdb -Force -ErrorAction SilentlyContinue
            }
        }
        
        # Success
        Write-Host ""
        Write-Host "================================================================" -ForegroundColor Green
        Write-Host "         Installation Completed Successfully!" -ForegroundColor Green
        Write-Host "================================================================" -ForegroundColor Green
        Write-Host ""
        Write-Host "  Installed to: $TargetDll" -ForegroundColor White
        Write-Host ""
    }
    catch {
        Write-Host ""
        Write-Fail "Installation failed: $_"
        Write-Host ""
        Write-Host "  Troubleshooting:" -ForegroundColor Yellow
        Write-Host "    1. Close all Explorer windows" -ForegroundColor Gray
        Write-Host "    2. Try: .\Install.ps1 -Force" -ForegroundColor Gray
        Write-Host "    3. Reboot and try again" -ForegroundColor Gray
        Write-Host ""
        exit 1
    }
    finally {
        Start-ExplorerIfNeeded -WasRunning $explorerWasRunning
    }
}

# Main Uninstallation
function Uninstall-SQLLocalDBView {
    Show-Header "Uninstaller"
    
    if (-not (Test-Administrator)) {
        Write-Fail "This script requires Administrator privileges."
        exit 1
    }
    Write-OK "Running as Administrator"
    
    Write-Step "Checking installation..."
    if (-not (Test-Path $TargetDll)) {
        Write-Warn "SQLLocalDBView is not installed at $InstallDir"
        return
    }
    Write-OK "Found installation at $InstallDir"
    
    $explorerWasRunning = Stop-ProcessesSafely
    
    try {
        Write-Step "Unregistering shell extension..."
        Unregister-Extension -DllPath $TargetDll
        
        Write-Step "Removing files..."
        $filesToRemove = @($TargetDll, $TargetPdb, (Join-Path $InstallDir "SQLLocalDBView.exp"), (Join-Path $InstallDir "SQLLocalDBView.lib"))
        $failedFiles = @()
        
        foreach ($file in $filesToRemove) {
            if (Test-Path $file) {
                if (Remove-FileWithRetry -Path $file) {
                    Write-OK "Removed: $(Split-Path $file -Leaf)"
                } else {
                    Write-Fail "Could not remove: $(Split-Path $file -Leaf)"
                    $failedFiles += $file
                }
            }
        }
        
        # Remove directory if empty
        if ($failedFiles.Count -eq 0 -and (Test-Path $InstallDir)) {
            $remaining = Get-ChildItem $InstallDir -ErrorAction SilentlyContinue
            if (-not $remaining) {
                Remove-Item $InstallDir -Force -ErrorAction SilentlyContinue
                Write-OK "Removed installation directory"
            }
        }
        
        Write-Host ""
        if ($failedFiles.Count -eq 0) {
            Write-Host "================================================================" -ForegroundColor Green
            Write-Host "        Uninstallation Completed Successfully!" -ForegroundColor Green
            Write-Host "================================================================" -ForegroundColor Green
        } else {
            Write-Host "================================================================" -ForegroundColor Yellow
            Write-Host "      Uninstallation Completed with Warnings" -ForegroundColor Yellow
            Write-Host "================================================================" -ForegroundColor Yellow
            Write-Host "  Some files could not be removed. Try rebooting." -ForegroundColor Yellow
        }
        Write-Host ""
    }
    catch {
        Write-Fail "Uninstallation failed: $_"
        exit 1
    }
    finally {
        Start-ExplorerIfNeeded -WasRunning $explorerWasRunning
    }
}

# Entry Point
if ($Uninstall) {
    Uninstall-SQLLocalDBView
} else {
    Install-SQLLocalDBView
}

