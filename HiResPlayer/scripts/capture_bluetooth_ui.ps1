Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

Add-Type -ReferencedAssemblies "System.Drawing" @"
using System;
using System.Drawing;
using System.Runtime.InteropServices;

public class BtWindowCapture {
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

    # 1. Click Settings Nav button
    $btnSettingsCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "NavSettings")
    $btnSettings = $appWin.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $btnSettingsCond)
    if ($btnSettings -ne $null) {
        Write-Host "Navigating to Settings view..."
        $inv = $btnSettings.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
        $inv.Invoke()
        Start-Sleep -Seconds 2
        $bmp1 = [BtWindowCapture]::Capture($hwnd)
        $destPath1 = "C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\ui_bluetooth_fidelity_settings.png"
        $bmp1.Save($destPath1, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp1.Dispose()
        Write-Host "Settings screenshot saved to $destPath1"
    }

    # 2. Click Now Playing Nav button
    $btnNowPlayingCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "NavNowPlaying")
    $btnNowPlaying = $appWin.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $btnNowPlayingCond)
    if ($btnNowPlaying -ne $null) {
        Write-Host "Navigating to Now Playing view..."
        $inv2 = $btnNowPlaying.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
        $inv2.Invoke()
        Start-Sleep -Seconds 1
        $bmp2 = [BtWindowCapture]::Capture($hwnd)
        $destPath2 = "C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\ui_bluetooth_fidelity_nowplaying.png"
        $bmp2.Save($destPath2, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp2.Dispose()
        Write-Host "Now Playing screenshot saved to $destPath2"
    }
} else {
    Write-Host "Window not found by Name/PID pattern"
}

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Host "Capture completed successfully."
