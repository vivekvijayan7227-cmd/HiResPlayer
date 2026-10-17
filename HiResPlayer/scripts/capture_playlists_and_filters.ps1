Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

Add-Type -ReferencedAssemblies "System.Drawing" @"
using System;
using System.Drawing;
using System.Runtime.InteropServices;

public class WindowSnapshot2 {
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

    # 1. Capture Library with Filter Bar & Playlists Rail
    $bmp1 = [WindowSnapshot2]::Capture($hwnd)
    $bmp1.Save("C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\ui_playlists_and_filters_library.png", [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp1.Dispose()
    Write-Host "Captured Library with Playlists and Filters."

    # 2. Find and click Favorites playlist button
    $btnCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Button)
    $buttons = $appWin.FindAll([System.Windows.Automation.TreeScope]::Descendants, $btnCond)
    foreach ($btn in $buttons) {
        if ($btn.Current.Name -like "*Favorites*") {
            Write-Host "Clicking Favorites playlist..."
            $inv = $btn.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
            $inv.Invoke()
            Start-Sleep -Seconds 1
            $bmp2 = [WindowSnapshot2]::Capture($hwnd)
            $bmp2.Save("C:\Users\Vivek\.gemini\antigravity\brain\7c6dc9dd-cee4-4547-8ae1-67ab2913b0a5\ui_favorites_playlist_view.png", [System.Drawing.Imaging.ImageFormat]::Png)
            $bmp2.Dispose()
            Write-Host "Captured Favorites playlist view."
            break
        }
    }
}

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Host "Verification capture complete."
