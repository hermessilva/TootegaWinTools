<#
.SYNOPSIS
    SQLLocalDBView Debug Test Script

.DESCRIPTION
    Tests the SQLLocalDBView extension and shows debug logs.
    Run this after building and registering the DLL.

.NOTES
    Copyright (c) 1999-2026 Tootega Pesquisa e Inovacao. MIT License.
#>

$ErrorActionPreference = "Stop"
$LogFile = Join-Path $env:TEMP "SQLLocalDBView_Debug.log"

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║             SQLLocalDBView Debug Test Script                       ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Clear old log
if (Test-Path $LogFile) {
    Remove-Item $LogFile -Force
    Write-Host "  ✓ Cleared old log file" -ForegroundColor Green
} else {
    Write-Host "  ✓ No previous log file" -ForegroundColor Green
}

# Check if DLL is registered
Write-Host ""
Write-Host "Checking registration..." -ForegroundColor White
$clsid = reg query "HKCR\CLSID\{E8A3C5B7-9D2F-4A1E-8B6C-3D5E7F9A2C4D}" 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "  [OK] ShellFolder CLSID registered" -ForegroundColor Green
} else {
    Write-Host "  [ERROR] ShellFolder CLSID NOT registered!" -ForegroundColor Red
    exit 1
}

$progid = reg query "HKCR\SQLLocalDBView.Database" 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "  [OK] ProgId registered" -ForegroundColor Green
} else {
    Write-Host "  [ERROR] ProgId NOT registered!" -ForegroundColor Red
}

$ext = reg query "HKCR\.mdf" 2>&1
if ($ext -match "SQLLocalDBView.Database") {
    Write-Host "  [OK] .mdf extension associated" -ForegroundColor Green
} else {
    Write-Host "  [WARN] .mdf extension not associated with SQLLocalDBView.Database" -ForegroundColor Yellow
}

# Show shell command
Write-Host ""
Write-Host "Shell Open Command:" -ForegroundColor White
reg query "HKCR\SQLLocalDBView.Database\shell\open\command" 2>&1 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }

# Show ShellFolder attributes
Write-Host ""
Write-Host "ShellFolder Attributes:" -ForegroundColor White
reg query "HKCR\CLSID\{E8A3C5B7-9D2F-4A1E-8B6C-3D5E7F9A2C4D}\ShellFolder" 2>&1 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }

# Create a test database
$testDb = Join-Path $env:TEMP "SQLLocalDBView_Test.mdf"
Write-Host ""
Write-Host "Creating test database: $testDb" -ForegroundColor White

# Remove old test db
if (Test-Path $testDb) {
    Remove-Item $testDb -Force
}

# We can't easily create SQL LocalDB db from PowerShell without SQL LocalDB3.exe
# So just create an empty file and try to open it
# The important thing is to trigger the shell extension
[System.IO.File]::WriteAllBytes($testDb, @())
Write-Host "  [OK] Test file created" -ForegroundColor Green

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Gray
Write-Host ""
Write-Host "To test the extension:" -ForegroundColor White
Write-Host ""
Write-Host "  1. Open Windows Explorer" -ForegroundColor Gray
Write-Host "  2. Navigate to: $env:TEMP" -ForegroundColor Cyan
Write-Host "  3. Find: SQLLocalDBView_Test.mdf" -ForegroundColor Cyan
Write-Host "  4. Single-click to see Preview Pane" -ForegroundColor Gray
Write-Host "  5. Double-click to try to open as folder" -ForegroundColor Gray
Write-Host ""
Write-Host "After testing, check the log file:" -ForegroundColor White
Write-Host "  $LogFile" -ForegroundColor Cyan
Write-Host ""
Write-Host "To view log in real-time:" -ForegroundColor White
Write-Host "  Get-Content '$LogFile' -Wait" -ForegroundColor Cyan
Write-Host ""

# Wait for log file and show
Write-Host "Press any key to open Explorer in $env:TEMP..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# Open Explorer in temp folder
Start-Process explorer.exe -ArgumentList $env:TEMP

Write-Host ""
Write-Host "Watching log file for 30 seconds..." -ForegroundColor Yellow
Write-Host "(Do actions in Explorer now)" -ForegroundColor Yellow
Write-Host ""

$timeout = 30
$startTime = Get-Date
$lastSize = 0

while (((Get-Date) - $startTime).TotalSeconds -lt $timeout) {
    if (Test-Path $LogFile) {
        $currentSize = (Get-Item $LogFile).Length
        if ($currentSize -gt $lastSize) {
            $newContent = Get-Content $LogFile -Tail 50 2>$null
            foreach ($line in $newContent) {
                Write-Host $line -ForegroundColor Cyan
            }
            $lastSize = $currentSize
        }
    }
    Start-Sleep -Milliseconds 500
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Gray
Write-Host ""

# Show final log
if (Test-Path $LogFile) {
    Write-Host "Full log contents:" -ForegroundColor White
    Write-Host ""
    Get-Content $LogFile | ForEach-Object { Write-Host "  $_" -ForegroundColor Cyan }
} else {
    Write-Host "[WARN] No log file created - DLL may not be loading" -ForegroundColor Yellow
}

Write-Host ""
