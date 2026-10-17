# HiResPlayer Portable Packaging Script (Windows x64)
param (
    [string]$Configuration = "Release",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if (-not $OutputDir) {
    $OutputDir = Join-Path $RepoRoot "dist\HiResPlayer-win-x64"
}

# ── Dynamic DotNet Discovery ─────────────────────────────────────────────────
function Find-DotNet {
    $candidates = @(
        (Get-Command dotnet -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue),
        (Join-Path $env:LOCALAPPDATA "Programs\dotnet10\dotnet.exe"),
        "C:\Program Files\dotnet\dotnet.exe"
    )
    foreach ($c in $candidates) {
        if ($c -and (Test-Path $c)) {
            # Check if this dotnet has SDK installed
            $sdks = & $c --list-sdks 2>$null
            if ($sdks) { return $c }
        }
    }
    return "dotnet"
}

$DotNetExe = Find-DotNet

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " HiResPlayer Release Packaging" -ForegroundColor Cyan
Write-Host " Output: $OutputDir" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# 1. Clean output dir
if (Test-Path $OutputDir) {
    Write-Host "Cleaning existing directory: $OutputDir" -ForegroundColor Yellow
    Remove-Item -Path $OutputDir -Recurse -Force
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

# 2. Publish Desktop App
$desktopProj = Join-Path $RepoRoot "src\HiResPlayer.Desktop\HiResPlayer.Desktop.csproj"
Write-Host "Publishing HiResPlayer.Desktop (Self-Contained)..." -ForegroundColor Green
& $DotNetExe publish $desktopProj `
    -c $Configuration `
    -r win-x64 `
    --self-contained true `
    -o $OutputDir
if ($LASTEXITCODE -ne 0) { throw "Desktop publish failed with exit code $LASTEXITCODE" }

# 3. Staging native engine libraries
Write-Host "Staging native engine libraries..." -ForegroundColor Green
$NativeDlls = @(
    (Join-Path $RepoRoot "src\HiResAudioEngine\build\HiResAudioEngine.dll"),
    (Join-Path $RepoRoot "tests\libc++.dll"),
    (Join-Path $RepoRoot "tests\libunwind.dll")
)
if ($env:FLAC_ROOT -and (Test-Path (Join-Path $env:FLAC_ROOT "bin"))) {
    $flacDlls = Get-ChildItem (Join-Path $env:FLAC_ROOT "bin\*.dll") -ErrorAction SilentlyContinue
    foreach ($fd in $flacDlls) {
        $NativeDlls += $fd.FullName
    }
}
foreach ($dll in $NativeDlls) {
    if (Test-Path $dll) {
        Copy-Item -Path $dll -Destination $OutputDir -Force
        Write-Host "  -> Staged $(Split-Path $dll -Leaf)" -ForegroundColor Gray
    } else {
        Write-Warning "Native dependency not found: $dll"
    }
}

# 4. Copy branding assets
$icoSrc = Join-Path $RepoRoot "assets\app_icon.ico"
$pngSrc = Join-Path $RepoRoot "assets\app_icon_transparent.png"
if (Test-Path $icoSrc) { Copy-Item $icoSrc -Destination $OutputDir -Force }
if (Test-Path $pngSrc) { Copy-Item $pngSrc -Destination (Join-Path $OutputDir "app_icon.png") -Force }
Write-Host "  -> Staged app_icon.ico and app_icon.png" -ForegroundColor Gray

# 5. Copy Sample Test Tracks
$SampleDir = Join-Path $OutputDir "Samples"
New-Item -ItemType Directory -Path $SampleDir -Force | Out-Null
$fixtureGlob = Join-Path $RepoRoot "tests\Fixtures\*.flac"
Copy-Item $fixtureGlob -Destination $SampleDir -Force -ErrorAction SilentlyContinue

# 6. Documentation
$DocContent = @"
================================================================================
 HiResPlayer v1.0.1 — Audiophile Bit-Perfect FLAC Player & Acoustic Studio
================================================================================

Quick Start:
  1. Launch HiResPlayer.Desktop.exe
  2. Select your Audio Output Device from Settings / Devices
  3. Choose Playback Mode:
       - WASAPI Exclusive Mode: Bit-perfect hardware rendering (locks DAC)
       - WASAPI Shared Mode: High-resolution rendering with sinc resampler
  4. Play FLAC files or drag and drop audio folders into the player

Included Sample Files:
  - Samples\sample_44k_16b.flac (16-bit / 44.1 kHz Red Book CD standard)
  - Samples\sample_96k_24b.flac (24-bit / 96 kHz Studio Master)

Acoustic Mastering Studio:
  - 10-Band Precision Graphic Equalizer with Studio Pro Presets
  - 5-Band Parametric Equalizer with custom Q-factor control
  - Studio Center Channel (Vocal Focus / Center Cut)
  - Live 60 FPS Real-time FFT Audio Spectrum Analyzer

Architecture:
  - C++20 Native Audio Engine (libFLAC, WASAPI event-driven, MMCSS Audio priority)
  - Polyphase Kaiser-windowed sinc resampler (>105 dB alias rejection)
  - 100% Offline, Zero Telemetry, Deterministic DSP

================================================================================
"@
Set-Content -Path (Join-Path $OutputDir "README.txt") -Value $DocContent -Encoding UTF8

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " Packaging completed successfully!" -ForegroundColor Green
Write-Host " Destination: $OutputDir" -ForegroundColor Green
Write-Host "==================================================" -ForegroundColor Cyan
