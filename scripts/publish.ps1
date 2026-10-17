# HiResPlayer Portable Packaging Script (Windows x64)
param (
    [string]$Configuration = "Release",
    [string]$OutputDir = "d:\AudioPlayer\HiResPlayer\dist\HiResPlayer-win-x64"
)

$ErrorActionPreference = "Stop"
$DotNetExe = "C:\Users\Vivek\AppData\Local\Programs\dotnet10\dotnet.exe"
if (-not (Test-Path $DotNetExe)) {
    $DotNetExe = "dotnet"
}

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " HiResPlayer Release Packaging (M6 & M9)" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# 1. Clean output dir
if (Test-Path $OutputDir) {
    Write-Host "Cleaning existing directory: $OutputDir" -ForegroundColor Yellow
    Remove-Item -Path $OutputDir -Recurse -Force
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

# 2. Publish Desktop App
Write-Host "Publishing HiResPlayer.Desktop (Self-Contained)..." -ForegroundColor Green
& $DotNetExe publish "d:\AudioPlayer\HiResPlayer\src\HiResPlayer.Desktop\HiResPlayer.Desktop.csproj" `
    -c $Configuration `
    -r win-x64 `
    --self-contained true `
    -o $OutputDir
if ($LASTEXITCODE -ne 0) { throw "Desktop publish failed" }

# 3. Staging native engine libraries
Write-Host "Staging native engine libraries..." -ForegroundColor Green
$NativeDlls = @(
    "d:\AudioPlayer\HiResPlayer\src\HiResAudioEngine\build\HiResAudioEngine.dll",
    "d:\AudioPlayer\HiResPlayer\tests\libc++.dll",
    "d:\AudioPlayer\HiResPlayer\tests\libunwind.dll"
)
foreach ($dll in $NativeDlls) {
    if (Test-Path $dll) {
        Copy-Item -Path $dll -Destination $OutputDir -Force
        Write-Host "  -> Staged $(Split-Path $dll -Leaf)" -ForegroundColor Gray
    } else {
        Write-Warning "Native dependency not found: $dll"
    }
}

# 4. Copy branding assets
Copy-Item "d:\AudioPlayer\HiResPlayer\assets\app_icon.ico" -Destination $OutputDir -Force
Copy-Item "d:\AudioPlayer\HiResPlayer\assets\app_icon_transparent.png" -Destination (Join-Path $OutputDir "app_icon.png") -Force
Write-Host "  -> Staged app_icon.ico and app_icon.png" -ForegroundColor Gray

# 5. Copy Sample Test Tracks
$SampleDir = Join-Path $OutputDir "Samples"
New-Item -ItemType Directory -Path $SampleDir -Force | Out-Null
Copy-Item "d:\AudioPlayer\HiResPlayer\tests\Fixtures\*.flac" -Destination $SampleDir -Force

# 6. Documentation
$DocContent = @"
HiResPlayer — Bit-Perfect FLAC & Enhanced Audio Studio for Windows
==================================================================

Version: 1.0.0-lts
Target: Windows 11 / Windows 10 (64-bit)

Audio Modes:
1. Original / Bit-Perfect:
   - Direct WASAPI Exclusive PCM stream with zero modifications.
   - All DSP, gain, and format conversions bypassed.
   - Confirmed by the 7-point hardware/stream eligibility gate.

2. Enhanced Audio:
   - Real-time 10-Band Graphic Equalizer (-12 dB to +12 dB)
   - Studio Parametric EQ with 12 band types
   - Master Preamp (-18 dB to +6 dB) with smooth parameter slewing
   - Dedicated Non-AI Focus Studio: Vocal Focus & Instrumental Focus (Phase-Coherent M/S)
   - Real-time Stereo Phase Correlation (-1.0 to +1.0) & 32-Band FFT Spectrum Monitoring
   - 100% Offline Non-AI Classical Audio Analysis: Zero ONNX runtimes, Zero cloud dependencies
   - Clearly labeled: "Processed Audio — Not Bit-perfect"

Usage:
  Launch 'HiResPlayer.Desktop.exe'.
  Click 'Open FLAC File' or drag and drop reference tracks from the 'Samples\' folder.
"@
Set-Content -Path (Join-Path $OutputDir "README.txt") -Value $DocContent

# 7. Mirror to root dist folder for convenient access
$RootDist = "d:\AudioPlayer\dist\HiResPlayer-win-x64"
if ($OutputDir -ne $RootDist) {
    Write-Host "Mirroring to root distribution folder: $RootDist..." -ForegroundColor Green
    New-Item -ItemType Directory -Path $RootDist -Force | Out-Null
    Copy-Item -Path "$OutputDir\*" -Destination $RootDist -Recurse -Force
}

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " Package successfully created at:" -ForegroundColor Cyan
Write-Host "   1. $OutputDir" -ForegroundColor Yellow
Write-Host "   2. $RootDist" -ForegroundColor Yellow
Write-Host "==================================================" -ForegroundColor Cyan
