Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName UIAutomationClient

Get-Process HiResPlayer.Desktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

$proc = Start-Process "d:\AudioPlayer\dist\HiResPlayer-win-x64\HiResPlayer.Desktop.exe" -PassThru
Start-Sleep -Seconds 3

$root = [System.Windows.Automation.AutomationElement]::RootElement
$procCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $proc.Id)
$appWin = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $procCond)

if ($appWin) {
    Write-Host "Found Window via PID $($proc.Id)!"
    
    # 1. Click Now Playing button
    $btnNow = $appWin.FindFirst([System.Windows.Automation.TreeScope]::Descendants, 
        (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "NavNowPlaying")))
    if ($btnNow) {
        Write-Host "Navigating to Now Playing..."
        $inv = $btnNow.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
        $inv.Invoke()
        Start-Sleep -Seconds 1
    }

    # 2. Click Play button
    $btnPlay = $appWin.FindFirst([System.Windows.Automation.TreeScope]::Descendants, 
        (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "BtnDockPlay")))
    if ($btnPlay) {
        Write-Host "Triggering Playback..."
        $invPlay = $btnPlay.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
        $invPlay.Invoke()
        Start-Sleep -Seconds 2
    }

    # Capture visualizer in Now Playing
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap ($bounds.Width), ($bounds.Height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    $shotPath = "C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\ui_spectrum_obsidian_verified.png"
    $bmp.Save($shotPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
    Write-Host "Captured spectrum to $shotPath"
} else {
    Write-Host "Could not find window by PID"
}

Stop-Process -Id $proc.Id -Force
