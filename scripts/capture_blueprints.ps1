Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

$proc = Start-Process "d:\AudioPlayer\HiResPlayer\dist\HiResPlayer-win-x64\HiResPlayer.Desktop.exe" -PassThru
Start-Sleep -Seconds 3

# Capture Library View
$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap ($bounds.Width), ($bounds.Height)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
$shot1 = "C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\ui_blueprints_library.png"
$bmp.Save($shot1, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()
Write-Host "Library screenshot saved to $shot1"

# Click "Now Playing" using UI Automation
try {
    $root = [System.Windows.Automation.AutomationElement]::RootElement
    $cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $proc.Id)
    $appWin = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $cond)
    if ($appWin) {
        $btnCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "NavNowPlaying")
        $navNowPlaying = $appWin.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $btnCond)
        if ($navNowPlaying) {
            $invokePattern = $navNowPlaying.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
            $invokePattern.Invoke()
            Start-Sleep -Seconds 1
            
            $bmp2 = New-Object System.Drawing.Bitmap ($bounds.Width), ($bounds.Height)
            $g2 = [System.Drawing.Graphics]::FromImage($bmp2)
            $g2.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
            $shot2 = "C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\ui_blueprints_nowplaying.png"
            $bmp2.Save($shot2, [System.Drawing.Imaging.ImageFormat]::Png)
            $g2.Dispose()
            $bmp2.Dispose()
            Write-Host "Now Playing screenshot saved to $shot2"
        } else {
            Write-Host "NavNowPlaying button not found via UIAutomation"
        }
    } else {
        Write-Host "App window not found"
    }
} catch {
    Write-Host "UI Automation error: $_"
}

Stop-Process -Id $proc.Id -Force
Write-Host "Done."
