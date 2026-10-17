# HiResPlayer Standalone Installer Builder Script
param (
    [string]$Configuration = "Release",
    [string]$Version = "1.0.1",
    [string]$InstallerOutputDir = "",
    [string]$SubFolderDir = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if (-not $InstallerOutputDir) {
    $InstallerOutputDir = Join-Path $RepoRoot "Installer"
}
if (-not $SubFolderDir) {
    $SubFolderDir = Join-Path $RepoRoot "HiResPlayer_Installer"
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
            $sdks = & $c --list-sdks 2>$null
            if ($sdks) { return $c }
        }
    }
    return "dotnet"
}

$DotNetExe = Find-DotNet

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " Building HiResPlayer Standalone Setup .EXE" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# 1. Update payload.zip from current dist
$DistDir = Join-Path $RepoRoot "dist\HiResPlayer-win-x64"
$PayloadZip = Join-Path $RepoRoot "assets\payload.zip"
Write-Host "Compressing fresh distribution payload..." -ForegroundColor Green
if (Test-Path $PayloadZip) { Remove-Item $PayloadZip -Force }
Compress-Archive -Path "$DistDir\*" -DestinationPath $PayloadZip -CompressionLevel Optimal
Write-Host "  -> Payload archive ready ($([math]::Round(((Get-Item $PayloadZip).Length / 1MB), 2)) MB)" -ForegroundColor Gray

# 2. Build and publish standalone single-file installer
$installerProj = Join-Path $RepoRoot "src\HiResPlayer.Installer\HiResPlayer.Installer.csproj"
Write-Host "Compiling HiResPlayer.Installer..." -ForegroundColor Green
& $DotNetExe publish $installerProj `
    -c $Configuration `
    -r win-x64 `
    --self-contained true `
    -p:PublishSingleFile=true `
    -p:IncludeNativeLibrariesForSelfExtract=true `
    -p:EnableCompressionInSingleFile=true `
    -o $SubFolderDir
if ($LASTEXITCODE -ne 0) { throw "Installer build failed with exit code $LASTEXITCODE" }

# 3. Create standardized installer name
$InstallerFileName = "HiResPlayer-Setup-v$Version.exe"
$TargetExe = Join-Path $SubFolderDir $InstallerFileName
Copy-Item (Join-Path $SubFolderDir "HiResPlayer.Installer.exe") $TargetExe -Force

# 4. Copy to Installer directory
New-Item -ItemType Directory -Path $InstallerOutputDir -Force | Out-Null
Copy-Item $TargetExe (Join-Path $InstallerOutputDir $InstallerFileName) -Force

$GitRootInstaller = Join-Path $RepoRoot "..\Installer"
if ((Test-Path $GitRootInstaller) -and (Resolve-Path $GitRootInstaller).Path -ne (Resolve-Path $InstallerOutputDir).Path) {
    Copy-Item $TargetExe (Join-Path $GitRootInstaller $InstallerFileName) -Force
}

# 5. Summary
$SizeMb = [math]::Round(((Get-Item $TargetExe).Length / 1MB), 2)
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " SUCCESS: Standalone installer created!" -ForegroundColor Green
Write-Host " Output: $(Join-Path $InstallerOutputDir $InstallerFileName) ($SizeMb MB)" -ForegroundColor Yellow
Write-Host "==================================================" -ForegroundColor Cyan
