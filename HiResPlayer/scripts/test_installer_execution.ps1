Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

$installerPath = "d:\AudioPlayer\Installer\HiResPlayer-Setup-v1.0.0.exe"
Write-Host "Launching installer: $installerPath"

$proc = Start-Process -FilePath $installerPath -PassThru
Start-Sleep -Seconds 3

$root = [System.Windows.Automation.AutomationElement]::RootElement
$procCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $proc.Id)
$wins = $root.FindAll([System.Windows.Automation.TreeScope]::Children, $procCond)

$appWin = $null
foreach ($w in $wins) {
    if ($w.Current.ClassName -eq "Window" -or $w.Current.Name -like "*Setup*") {
        $appWin = $w
        break
    }
}

if ($appWin -ne $null) {
    Write-Host "Found Installer Window: $($appWin.Current.Name)"

    # Uncheck launch after so it doesn't open the player automatically in test
    $chkLaunchCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "ChkLaunchAfter")
    $chkLaunch = $appWin.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $chkLaunchCond)
    if ($chkLaunch -ne $null) {
        $toggle = $chkLaunch.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
        if ($toggle.Current.ToggleState -eq [System.Windows.Automation.ToggleState]::On) {
            $toggle.Toggle()
            Write-Host "Toggled ChkLaunchAfter off for test."
        }
    }

    # Click Install Button
    $btnInstallCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "BtnInstall")
    $btnInstall = $appWin.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $btnInstallCond)
    if ($btnInstall -ne $null) {
        Write-Host "Invoking Install button..."
        $inv = $btnInstall.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
        $inv.Invoke()
        
        Write-Host "Waiting for installation to complete..."
        Start-Sleep -Seconds 6

        # Check install directory
        $installDir = "$env:LOCALAPPDATA\Programs\HiResPlayer"
        if (Test-Path "$installDir\HiResPlayer.Desktop.exe") {
            Write-Host "SUCCESS: HiResPlayer.Desktop.exe installed at $installDir!" -ForegroundColor Green
            Get-ChildItem -Path $installDir | Select-Object Name, Length | Format-Table
        } else {
            Write-Warning "Target file not found in $installDir"
        }
    }
} else {
    Write-Warning "Could not find installer window"
}

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Host "Installer test completed."
