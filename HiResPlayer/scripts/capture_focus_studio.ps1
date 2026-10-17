Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

Add-Type -ReferencedAssemblies "System.Drawing" @"
using System;
using System.Drawing;
using System.Runtime.InteropServices;

public class WindowSnapshot {
    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBkgnd, uint nFlags);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left, Top, Right, Bottom;
    }

    public static Bitmap Capture(IntPtr hWnd) {
        RECT r;
        GetWindowRect(hWnd, out r);
        int w = r.Right - r.Left;
        int h = r.Bottom - r.Top;
        if (w <= 0) w = 1300;
        if (h <= 0) h = 780;
        Bitmap bmp = new Bitmap(w, h);
        using (Graphics g = Graphics.FromImage(bmp)) {
            IntPtr hdc = g.GetHdc();
            PrintWindow(hWnd, hdc, 2); // 2 = PW_RENDERFULLCONTENT
            g.ReleaseHdc(hdc);
        }
        return bmp;
    }
}
"@

$proc = Start-Process "d:\AudioPlayer\HiResPlayer\dist\HiResPlayer-win-x64\HiResPlayer.Desktop.exe" -PassThru
Start-Sleep -Seconds 4

$root = [System.Windows.Automation.AutomationElement]::RootElement
$procCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $proc.Id)
$wins = $root.FindAll([System.Windows.Automation.TreeScope]::Children, $procCond)

$appWin = $null
foreach ($w in $wins) {
    if ($w.Current.ClassName -eq "Window" -or $w.Current.Name -like "*HiRes Player*") {
        $appWin = $w
        break
    }
}

if ($appWin -ne $null) {
    Write-Host "Found Window: $($appWin.Current.Name)"
    $hwnd = [IntPtr]$appWin.Current.NativeWindowHandle

    # 1. Click Focus Studio Nav button
    $btnFocusCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "NavVocalStudio")
    $btnFocus = $appWin.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $btnFocusCond)
    if ($btnFocus -ne $null) {
        Write-Host "Navigating to Focus Studio..."
        $invFocus = $btnFocus.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
        $invFocus.Invoke()
        Start-Sleep -Seconds 1
        $bmp = [WindowSnapshot]::Capture($hwnd)
        $bmp.Save("C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\ui_focus_studio_idle.png", [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Host "Focus Studio Idle snapshot captured."
    }

    # 2. Trigger play
    $btnPlayCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "BtnPlayPause")
    $btnPlay = $appWin.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $btnPlayCond)
    if ($btnPlay -ne $null) {
        Write-Host "Triggering Play..."
        $invPlay = $btnPlay.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
        $invPlay.Invoke()
        Start-Sleep -Seconds 2
        $bmp = [WindowSnapshot]::Capture($hwnd)
        $bmp.Save("C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\ui_focus_studio_live.png", [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Host "Focus Studio Live snapshot captured."
    }
}

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Host "Done."
