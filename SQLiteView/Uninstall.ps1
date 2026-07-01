<#
.SYNOPSIS
    SQLiteView Uninstaller

.DESCRIPTION
    Uninstalls the SQLiteView Windows Explorer Shell Extension.
    This script must be run as Administrator.
    
    This is a standalone uninstaller with additional process management capabilities.

.PARAMETER KillOnly
    Only kills processes that may have the DLL locked, without uninstalling.
    Useful for debugging or before manual operations.

.NOTES
    Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
#>

[CmdletBinding()]
param(
    [switch]$KillOnly
)

$ErrorActionPreference = "Stop"

# Configuration
$InstallDir = "C:\Program Files\Tootega"
$DllName = "SQLiteView.dll"
$TargetDll = Join-Path $InstallDir $DllName

# Processes that may lock shell extension DLLs
$ShellProcesses = @(
    @{ Name = "prevhost"; Desc = "Preview Handler Host" },
    @{ Name = "dllhost"; Desc = "COM Surrogate" },
    @{ Name = "SearchProtocolHost"; Desc = "Windows Search Protocol" },
    @{ Name = "SearchFilterHost"; Desc = "Windows Search Filter" },
    @{ Name = "ApplicationFrameHost"; Desc = "UWP Frame Host" },
    @{ Name = "explorer"; Desc = "Windows Explorer" }
)

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Write-Step { param([string]$Message) Write-Host "  > $Message" -ForegroundColor White }
function Write-OK { param([string]$Message) Write-Host "    [OK] $Message" -ForegroundColor Green }
function Write-Warn { param([string]$Message) Write-Host "    [WARN] $Message" -ForegroundColor Yellow }
function Write-Fail { param([string]$Message) Write-Host "    [FAIL] $Message" -ForegroundColor Red }

function Stop-AllShellProcesses {
    Write-Step "Terminating shell processes..."
    
    $explorerWasRunning = $false
    $terminated = @()
    
    foreach ($proc in $ShellProcesses) {
        $running = Get-Process -Name $proc.Name -ErrorAction SilentlyContinue
        if ($running) {
            if ($proc.Name -eq "explorer") { $explorerWasRunning = $true }
            
            Write-Host "    Killing $($proc.Desc) ($($proc.Name).exe)..." -ForegroundColor Gray
            foreach ($p in $running) {
                try {
                    $pid = $p.Id
                    $p.Kill()
                    $p.WaitForExit(5000) | Out-Null
                    $terminated += "$($proc.Name) (PID: $pid)"
                } catch { }
            }
            Start-Sleep -Milliseconds 500
        }
    }
    
    if ($explorerWasRunning) { Start-Sleep -Seconds 2 }
    
    if ($terminated.Count -gt 0) {
        Write-OK "Terminated $($terminated.Count) process(es)"
    } else {
        Write-OK "No shell processes were running"
    }
    
    return $explorerWasRunning
}

function Start-ExplorerSafely {
    $explorer = Get-Process -Name "explorer" -ErrorAction SilentlyContinue
    if (-not $explorer) {
        Write-Step "Restarting Windows Explorer..."
        Start-Process "explorer.exe"
        Start-Sleep -Seconds 2
        Write-OK "Explorer restarted"
    }
}

function Remove-FileWithRetry {
    param([string]$Path, [int]$MaxAttempts = 5)
    
    if (-not (Test-Path $Path)) { return $true }
    
    for ($i = 1; $i -le $MaxAttempts; $i++) {
        try {
            Remove-Item $Path -Force -ErrorAction Stop
            return $true
        } catch {
            if ($i -lt $MaxAttempts) {
                Start-Sleep -Milliseconds 500
                Stop-Process -Name "prevhost" -Force -ErrorAction SilentlyContinue
                Stop-Process -Name "dllhost" -Force -ErrorAction SilentlyContinue
            }
        }
    }
    return $false
}

# Kill Only Mode
function Invoke-KillOnly {
    Write-Host ""
    Write-Host "================================================================" -ForegroundColor Yellow
    Write-Host "           SQLiteView Process Killer" -ForegroundColor Yellow
    Write-Host "================================================================" -ForegroundColor Yellow
    Write-Host ""
    
    if (-not (Test-Administrator)) {
        Write-Fail "This script requires Administrator privileges."
        exit 1
    }
    Write-OK "Running as Administrator"
    
    Write-Step "Current shell processes:"
    foreach ($proc in $ShellProcesses) {
        $running = Get-Process -Name $proc.Name -ErrorAction SilentlyContinue
        if ($running) {
            $pids = ($running | ForEach-Object { $_.Id }) -join ", "
            Write-Host "    $($proc.Name).exe - PIDs: $pids" -ForegroundColor Cyan
        }
    }
    
    Write-Host ""
    Write-Host "  This will kill all shell processes to release locked DLLs." -ForegroundColor Yellow
    Write-Host ""
    
    $confirm = Read-Host "  Continue? (Y/N)"
    if ($confirm -ne "Y" -and $confirm -ne "y") {
        Write-Host "  Cancelled." -ForegroundColor Gray
        return
    }
    
    $explorerWasRunning = Stop-AllShellProcesses
    
    Write-Host ""
    Write-OK "All shell processes terminated"
    Write-Host ""
    Write-Host "  You can now delete/replace DLLs or run installer." -ForegroundColor Gray
    Write-Host ""
    
    Start-ExplorerSafely
}

# Main Uninstall
function Invoke-Uninstall {
    Write-Host ""
    Write-Host "================================================================" -ForegroundColor Cyan
    Write-Host "           SQLiteView Uninstaller" -ForegroundColor Cyan
    Write-Host "    Windows Explorer Shell Extension for SQLite" -ForegroundColor Cyan
    Write-Host "================================================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Install Directory: $InstallDir" -ForegroundColor Gray
    Write-Host ""
    
    if (-not (Test-Administrator)) {
        Write-Fail "This script requires Administrator privileges."
        exit 1
    }
    Write-OK "Running as Administrator"
    
    Write-Step "Checking installation..."
    $dllExists = Test-Path $TargetDll
    $registryExists = Test-Path "HKCR:\CLSID\{E8A3C5B7-9D2F-4A1E-8B6C-3D5E7F9A2C4D}"
    
    if (-not $dllExists -and -not $registryExists) {
        Write-Warn "SQLiteView does not appear to be installed"
        return
    }
    
    if ($dllExists) { Write-OK "Found DLL: $TargetDll" }
    if ($registryExists) { Write-OK "Found registry entries" }
    
    $explorerWasRunning = Stop-AllShellProcesses
    
    try {
        if ($dllExists) {
            Write-Step "Unregistering shell extension..."
            $regsvr = Start-Process -FilePath "regsvr32.exe" -ArgumentList "/u /s `"$TargetDll`"" -Wait -PassThru -NoNewWindow
            if ($regsvr.ExitCode -eq 0) {
                Write-OK "DLL unregistered"
            } else {
                Write-Warn "Unregister returned code: $($regsvr.ExitCode)"
            }
        }
        
        Write-Step "Removing files..."
        $allRemoved = $true
        
        foreach ($fileName in @("SQLiteView.dll", "SQLiteView.pdb", "SQLiteView.exp", "SQLiteView.lib")) {
            $filePath = Join-Path $InstallDir $fileName
            if (Test-Path $filePath) {
                if (Remove-FileWithRetry -Path $filePath) {
                    Write-OK "Removed: $fileName"
                } else {
                    Write-Fail "Could not remove: $fileName"
                    $allRemoved = $false
                }
            }
        }
        
        if ($allRemoved -and (Test-Path $InstallDir)) {
            $remaining = Get-ChildItem $InstallDir -ErrorAction SilentlyContinue
            if (-not $remaining) {
                Remove-Item $InstallDir -Force -ErrorAction SilentlyContinue
                Write-OK "Removed installation directory"
            }
        }
        
        Write-Host ""
        if ($allRemoved) {
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
        Start-ExplorerSafely
    }
}

# Entry Point
if ($KillOnly) {
    Invoke-KillOnly
} else {
    Invoke-Uninstall
}
