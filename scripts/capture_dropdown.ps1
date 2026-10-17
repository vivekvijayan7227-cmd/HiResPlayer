Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$proc = Start-Process "d:\AudioPlayer\HiResPlayer\dist\HiResPlayer-win-x64\HiResPlayer.Desktop.exe" -PassThru
Start-Sleep -Seconds 3

try {
    $root = [System.Windows.Automation.AutomationElement]::RootElement
    $cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $proc.Id)
    $win = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $cond)

    if ($win -ne $null) {
        Write-Host "Found window: $($win.Current.Name)"
        
        # Find NavAudioQuality button
        $navCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "NavAudioQuality")
        $navBtn = $win.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $navCond)
        if ($navBtn -ne $null) {
            Write-Host "Clicking NavAudioQuality..."
            $invokePattern = $navBtn.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
            $invokePattern.Invoke()
            Start-Sleep -Milliseconds 800
        }

        # Find CboDevices
        $cboCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "CboDevices")
        $cbo = $win.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $cboCond)
        if ($cbo -ne $null) {
            Write-Host "Expanding CboDevices..."
            $expandPattern = $cbo.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
            $expandPattern.Expand()
            Start-Sleep -Milliseconds 800
        }
    }
} catch {
    Write-Host "Automation error: $_"
}

$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap ($bounds.Width), ($bounds.Height)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
$shotPath = "C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\dropdown_verification_shot.png"
$bmp.Save($shotPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()

Write-Host "Screenshot saved to $shotPath. Process ID: $($proc.Id)"
Start-Sleep -Seconds 2
Stop-Process -Id $proc.Id -Force
