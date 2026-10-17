$exePath = "d:\AudioPlayer\HiResPlayer\dist\HiResPlayer-win-x64\HiResPlayer.Desktop.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "Executable not found at $exePath"
    exit 1
}

Write-Host "Starting $exePath..."
$proc = Start-Process -FilePath $exePath -PassThru
Start-Sleep -Seconds 3

if ($proc.HasExited) {
    Write-Host "Process exited prematurely with exit code: $($proc.ExitCode)"
    $crashLog = "$env:LOCALAPPDATA\HiResPlayer\crash.log"
    if (Test-Path $crashLog) {
        Write-Host "Crash log contents:"
        Get-Content $crashLog -Tail 20
    }
    exit 1
} else {
    Write-Host "SUCCESS: HiResPlayer.Desktop.exe is running steadily with PID: $($proc.Id)!"
    $appLog = "$env:LOCALAPPDATA\HiResPlayer\app.log"
    if (Test-Path $appLog) {
        Write-Host "App log contents:"
        Get-Content $appLog -Tail 15
    }
    Stop-Process -Id $proc.Id -Force
    Write-Host "Test completed cleanly."
    exit 0
}
