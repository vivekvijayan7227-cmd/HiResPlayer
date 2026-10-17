Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$proc = Start-Process "d:\AudioPlayer\HiResPlayer\dist\HiResPlayer-win-x64\HiResPlayer.Desktop.exe" -PassThru
Start-Sleep -Seconds 3

$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap ($bounds.Width), ($bounds.Height)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
$shotPath = "C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\ui_verification_shot_v3.png"
$bmp.Save($shotPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()

Write-Host "Screenshot saved to $shotPath. Process ID: $($proc.Id)"
Start-Sleep -Seconds 1
Stop-Process -Id $proc.Id -Force
