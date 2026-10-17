Remove-Item "$env:LOCALAPPDATA\HiResPlayer\crash.log" -ErrorAction SilentlyContinue
$exe = "d:\AudioPlayer\HiResPlayer\dist\HiResPlayer-win-x64\HiResPlayer.Desktop.exe"
$proc = Start-Process -FilePath $exe -PassThru
Start-Sleep -Seconds 3

Write-Host "Process ID: $($proc.Id)"
Write-Host "Has Exited: $($proc.HasExited)"
Write-Host "Main Window Title: $($proc.MainWindowTitle)"

$crashPath = "$env:LOCALAPPDATA\HiResPlayer\crash.log"
if (Test-Path $crashPath) {
    Write-Host "CRASH LOG FOUND:"
    Get-Content $crashPath
} else {
    Write-Host "No crash log found. App started successfully!"
}

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force
}
