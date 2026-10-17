Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

$proc = Start-Process "d:\AudioPlayer\HiResPlayer\dist\HiResPlayer-win-x64\HiResPlayer.Desktop.exe" -PassThru
Start-Sleep -Seconds 4

$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds

function CaptureScreen($destPath) {
    $bmp = New-Object System.Drawing.Bitmap ($bounds.Width), ($bounds.Height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    $bmp.Save($destPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
    Write-Host "Screenshot saved to $destPath"
}

# Capture default library view
CaptureScreen("C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\ui_verification_v4_default.png")

$root = [System.Windows.Automation.AutomationElement]::RootElement
$allWins = $root.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition)
$appWin = $null
foreach ($w in $allWins) {
    if ($w.Current.Name -like "HiRes Player*") {
        $appWin = $w
        break
    }
}

if ($appWin -ne $null) {
    Write-Host "Found Window: $($appWin.Current.Name)"

    # 1. Click Queue Dock button
    $btnQueueCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "BtnDockNowPlaying")
    $btnQueue = $appWin.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $btnQueueCond)
    if ($btnQueue -ne $null) {
        Write-Host "Invoking BtnDockNowPlaying..."
        $inv = $btnQueue.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
        $inv.Invoke()
        Start-Sleep -Seconds 1
        CaptureScreen("C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\ui_queue_dock_flyout.png")
        # Toggle close
        $inv.Invoke()
        Start-Sleep -Milliseconds 500
    }

    # 2. Click Settings Nav button
    $btnSettingsCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "NavSettings")
    $btnSettings = $appWin.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $btnSettingsCond)
    if ($btnSettings -ne $null) {
        Write-Host "Invoking NavSettings..."
        $inv2 = $btnSettings.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
        $inv2.Invoke()
        Start-Sleep -Seconds 1
        CaptureScreen("C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\ui_settings_view.png")
    }
} else {
    Write-Host "Window not found by Name pattern"
}

Stop-Process -Id $proc.Id -Force
Write-Host "Finished."
