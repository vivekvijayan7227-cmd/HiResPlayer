# HiResAudioEngine Portable Build & Staging Script
param (
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " Building HiResAudioEngine (C++20 Native Engine)" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# ── Dynamic Toolchain Discovery ──────────────────────────────────────────────
function Find-Executable {
    param([string]$Name, [string[]]$Fallbacks)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source) { return $cmd.Source }
    foreach ($fb in $Fallbacks) {
        if ($fb -and (Test-Path $fb)) {
            if ((Get-Item $fb).PSIsContainer) {
                $candidate = Join-Path $fb "$Name.exe"
                if (Test-Path $candidate) { return $candidate }
            } else {
                return $fb
            }
        }
    }
    return $null
}

# 1. CMake
$cmakeFallbacks = @(
    "C:\Program Files\CMake\bin\cmake.exe",
    "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Kitware.CMake_*\cmake-*\bin\cmake.exe"
)
$resolvedCmake = $null
foreach ($pattern in $cmakeFallbacks) {
    $matches = Resolve-Path $pattern -ErrorAction SilentlyContinue
    if ($matches) { $resolvedCmake = $matches[0].Path; break }
}
$CmakeExe = Find-Executable "cmake" @($resolvedCmake, "C:\Program Files\CMake\bin\cmake.exe")

# 2. Ninja
$ninjaFallbacks = @(
    "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Ninja-build.Ninja_*\ninja.exe",
    "C:\Program Files\Ninja\ninja.exe",
    "C:\ProgramData\chocolatey\bin\ninja.exe"
)
$resolvedNinja = $null
foreach ($pattern in $ninjaFallbacks) {
    $matches = Resolve-Path $pattern -ErrorAction SilentlyContinue
    if ($matches) { $resolvedNinja = $matches[0].Path; break }
}
$NinjaExe = Find-Executable "ninja" @($resolvedNinja, "C:\Program Files\Ninja\ninja.exe")

# 3. LLVM / Clang
$llvmFallbacks = @(
    "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_*\*\bin\clang++.exe",
    "C:\llvm-mingw\bin\clang++.exe",
    "C:\Program Files\LLVM\bin\clang++.exe"
)
$resolvedLlvm = $null
foreach ($pattern in $llvmFallbacks) {
    $matches = Resolve-Path $pattern -ErrorAction SilentlyContinue
    if ($matches) { $resolvedLlvm = $matches[0].Path; break }
}
$ClangExe = Find-Executable "clang++" @($resolvedLlvm, "C:\llvm-mingw\bin\clang++.exe")

if (-not $NinjaExe) { throw "Ninja not found. Please install Ninja or add it to PATH." }
if (-not $CmakeExe) { throw "CMake not found. Please install CMake or add it to PATH." }

# Prepend discovered tool dirs to PATH
$toolDirs = @(
    (Split-Path $NinjaExe -Parent),
    (Split-Path $CmakeExe -Parent)
)
if ($ClangExe) { $toolDirs += (Split-Path $ClangExe -Parent) }
$env:PATH = ($toolDirs -join ";") + ";" + $env:PATH

$engineSrc = Join-Path $RepoRoot "src\HiResAudioEngine"
$engineBuild = Join-Path $engineSrc "build"

# ── Clean if requested ────────────────────────────────────────────────────────
if ($Clean -and (Test-Path $engineBuild)) {
    Write-Host "Cleaning build directory: $engineBuild" -ForegroundColor Yellow
    Remove-Item $engineBuild -Recurse -Force
}

# ── CMake Configure if needed ─────────────────────────────────────────────────
if (-not (Test-Path (Join-Path $engineBuild "build.ninja"))) {
    Write-Host "Configuring CMake with Ninja generator..." -ForegroundColor Green
    New-Item -ItemType Directory -Path $engineBuild -Force | Out-Null
    & $CmakeExe -B $engineBuild -S $engineSrc -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed with exit code $LASTEXITCODE" }
}

# ── Compile with Ninja ────────────────────────────────────────────────────────
Write-Host "Compiling HiResAudioEngine with Ninja..." -ForegroundColor Green
& $NinjaExe -C $engineBuild
if ($LASTEXITCODE -ne 0) {
    throw "Ninja build failed with exit code $LASTEXITCODE"
}

# ── Copy Output to Staging Locations ──────────────────────────────────────────
$builtDll = Join-Path $engineBuild "HiResAudioEngine.dll"
if (-not (Test-Path $builtDll)) { throw "Built DLL not found at: $builtDll" }

Write-Host "Copying HiResAudioEngine.dll to target locations..." -ForegroundColor Green
$targets = @(
    (Join-Path $RepoRoot "tests\HiResAudioEngine.dll"),
    (Join-Path $RepoRoot "tests\ManagedTests\bin\Debug\net10.0-windows\HiResAudioEngine.dll"),
    (Join-Path $RepoRoot "src\HiResPlayer.Desktop\bin\Release\net10.0-windows\win-x64\HiResAudioEngine.dll"),
    (Join-Path $RepoRoot "dist\HiResPlayer-win-x64\HiResAudioEngine.dll")
)

foreach ($t in $targets) {
    $parent = Split-Path $t -Parent
    if (Test-Path $parent) {
        Copy-Item $builtDll $t -Force
        Write-Host "  -> Updated $t" -ForegroundColor Gray
    }
}

Write-Host "HiResAudioEngine build and staging complete." -ForegroundColor Green
