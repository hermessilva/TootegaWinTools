# GenerateIcons.ps1
# Creates simple .ico files for SevenZipView
# Run this script to generate the icons

Add-Type -AssemblyName System.Drawing

function Create-SimpleIcon {
    param(
        [string]$OutputPath,
        [string]$Text,
        [System.Drawing.Color]$BackColor,
        [System.Drawing.Color]$TextColor
    )
    
    $sizes = @(16, 32, 48, 256)
    $images = @()
    
    foreach ($size in $sizes) {
        $bmp = New-Object System.Drawing.Bitmap($size, $size)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
        
        # Fill background with rounded rectangle
        $brush = New-Object System.Drawing.SolidBrush($BackColor)
        $rect = New-Object System.Drawing.Rectangle(0, 0, $size, $size)
        $g.FillRectangle($brush, $rect)
        
        # Draw text
        $fontSize = [Math]::Max(6, $size / 3)
        $font = New-Object System.Drawing.Font("Segoe UI", $fontSize, [System.Drawing.FontStyle]::Bold)
        $textBrush = New-Object System.Drawing.SolidBrush($TextColor)
        $sf = New-Object System.Drawing.StringFormat
        $sf.Alignment = [System.Drawing.StringAlignment]::Center
        $sf.LineAlignment = [System.Drawing.StringAlignment]::Center
        $g.DrawString($Text, $font, $textBrush, ($size/2), ($size/2), $sf)
        
        $g.Dispose()
        $images += $bmp
    }
    
    # Save as ICO (simplified - just save 32x32 as icon)
    $icon = [System.Drawing.Icon]::FromHandle($images[1].GetHicon())
    $fs = [System.IO.File]::Create($OutputPath)
    $icon.Save($fs)
    $fs.Close()
    
    foreach ($img in $images) {
        $img.Dispose()
    }
    
    Write-Host "Created: $OutputPath"
}

# Create directories
$resourcesDir = Join-Path $PSScriptRoot "..\SevenZipView\resources"
$installerDir = Join-Path $PSScriptRoot "..\Installer\Resources"

if (!(Test-Path $resourcesDir)) { New-Item -ItemType Directory -Path $resourcesDir -Force }
if (!(Test-Path $installerDir)) { New-Item -ItemType Directory -Path $installerDir -Force }

# Create 7z file icon (blue background)
Create-SimpleIcon -OutputPath (Join-Path $resourcesDir "7z.ico") `
    -Text "7z" `
    -BackColor ([System.Drawing.Color]::FromArgb(255, 0, 120, 215)) `
    -TextColor ([System.Drawing.Color]::White)

# Create Setup icon (green background)
Create-SimpleIcon -OutputPath (Join-Path $installerDir "Setup.ico") `
    -Text "7z" `
    -BackColor ([System.Drawing.Color]::FromArgb(255, 16, 124, 16)) `
    -TextColor ([System.Drawing.Color]::White)

Write-Host "`nIcons generated successfully!"
Write-Host "- 7z.ico for archive files"
Write-Host "- Setup.ico for installer"
