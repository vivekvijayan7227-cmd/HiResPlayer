param (
    [string]$Configuration = "Release",
    [string]$InstallerOutputDir = "d:\AudioPlayer\Installer",
    [string]$SubFolderDir = "d:\AudioPlayer\HiResPlayer_Installer"
)

$ErrorActionPreference = "Stop"
$DotNetExe = "C:\Users\Vivek\AppData\Local\Programs\dotnet10\dotnet.exe"
if (-not (Test-Path $DotNetExe)) {
    $DotNetExe = "dotnet"
}

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " Building HiResPlayer Standalone Setup .EXE" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# 1. Update payload.zip from current dist
$DistDir = "d:\AudioPlayer\HiResPlayer\dist\HiResPlayer-win-x64"
$PayloadZip = "d:\AudioPlayer\HiResPlayer\assets\payload.zip"
Write-Host "Compressing fresh distribution payload..." -ForegroundColor Green
if (Test-Path $PayloadZip) { Remove-Item $PayloadZip -Force }
Compress-Archive -Path "$DistDir\*" -DestinationPath $PayloadZip -CompressionLevel Optimal
Write-Host "  -> Payload archive ready ($([math]::Round(((Get-Item $PayloadZip).Length / 1MB), 2)) MB)" -ForegroundColor Gray

# 2. Build and publish standalone single-file installer
Write-Host "Compiling HiResPlayer.Installer..." -ForegroundColor Green
& $DotNetExe publish "d:\AudioPlayer\HiResPlayer\src\HiResPlayer.Installer\HiResPlayer.Installer.csproj" `
    -c $Configuration `
    -r win-x64 `
    --self-contained true `
    -p:PublishSingleFile=true `
    -p:IncludeNativeLibrariesForSelfExtract=true `
    -p:EnableCompressionInSingleFile=true `
    -o $SubFolderDir
if ($LASTEXITCODE -ne 0) { throw "Installer build failed" }

# 3. Create standardized installer name
$TargetExe = Join-Path $SubFolderDir "HiResPlayer-Setup-v1.0.0.exe"
Copy-Item (Join-Path $SubFolderDir "HiResPlayer.Installer.exe") $TargetExe -Force

# 4. Copy to d:\AudioPlayer\Installer
New-Item -ItemType Directory -Path $InstallerOutputDir -Force | Out-Null
Copy-Item $TargetExe (Join-Path $InstallerOutputDir "HiResPlayer-Setup-v1.0.0.exe") -Force

# 5. Summary
$SizeMb = [math]::Round(((Get-Item $TargetExe).Length / 1MB), 2)
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " SUCCESS: Standalone installer created!" -ForegroundColor Green
Write-Host " Location 1: $TargetExe ($SizeMb MB)" -ForegroundColor Yellow
Write-Host " Location 2: $(Join-Path $InstallerOutputDir 'HiResPlayer-Setup-v1.0.0.exe') ($SizeMb MB)" -ForegroundColor Yellow
Write-Host "==================================================" -ForegroundColor Cyan
