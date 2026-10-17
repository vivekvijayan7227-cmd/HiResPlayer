$ninjaDir = "C:\Users\Vivek\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe"
$cmakeDir = "C:\Program Files\CMake\bin"
$llvmDir = "C:\Users\Vivek\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260908-ucrt-x86_64\bin"
$dotnetDir = "C:\Users\Vivek\AppData\Local\Programs\dotnet10"

$env:PATH = "$ninjaDir;$cmakeDir;$llvmDir;$dotnetDir;" + $env:PATH
$engineBuild = "d:\AudioPlayer\HiResPlayer\src\HiResAudioEngine\build"

Write-Host "Compiling HiResAudioEngine with Ninja..."
& "$ninjaDir\ninja.exe" -C $engineBuild
if ($LASTEXITCODE -ne 0) {
    Write-Error "Ninja build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "Copying HiResAudioEngine.dll to targets..."
$targets = @(
    "d:\AudioPlayer\HiResPlayer\tests\HiResAudioEngine.dll",
    "d:\AudioPlayer\HiResPlayer\tests\ManagedTests\bin\Debug\net10.0-windows\HiResAudioEngine.dll",
    "d:\AudioPlayer\HiResPlayer\src\HiResPlayer.Desktop\bin\Release\net10.0-windows\win-x64\HiResAudioEngine.dll",
    "d:\AudioPlayer\HiResPlayer\dist\HiResPlayer-win-x64\HiResAudioEngine.dll"
)

foreach ($t in $targets) {
    $parent = Split-Path $t -Parent
    if (Test-Path $parent) {
        Copy-Item "$engineBuild\HiResAudioEngine.dll" $t -Force
        Write-Host "  -> Updated $t"
    }
}

Write-Host "HiResAudioEngine build and staging complete."
