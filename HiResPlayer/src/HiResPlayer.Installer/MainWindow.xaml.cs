using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows;
using Microsoft.Win32;

namespace HiResPlayer.Installer
{
    public partial class MainWindow : Window
    {
        private bool _isCompleted = false;
        private string _installedExePath = string.Empty;

        public MainWindow()
        {
            InitializeComponent();
            string defaultPath = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "Programs",
                "HiResPlayer"
            );
            TxtInstallPath.Text = defaultPath;

            // Automated UI verification hook
            Loaded += async (s, e) =>
            {
                var args = Environment.GetCommandLineArgs();
                for (int i = 0; i < args.Length; i++)
                {
                    if (args[i] == "--capture" && i + 1 < args.Length)
                    {
                        string outPath = args[i + 1];
                        await Task.Delay(400);
                        UpdateLayout();
                        int w = (int)Math.Max(ActualWidth, 660);
                        int h = (int)Math.Max(ActualHeight, 500);
                        var rtb = new System.Windows.Media.Imaging.RenderTargetBitmap(w, h, 96, 96, System.Windows.Media.PixelFormats.Pbgra32);
                        rtb.Render(this);
                        var enc = new System.Windows.Media.Imaging.PngBitmapEncoder();
                        enc.Frames.Add(System.Windows.Media.Imaging.BitmapFrame.Create(rtb));
                        using (var fs = File.Create(outPath))
                        {
                            enc.Save(fs);
                        }
                        System.Windows.Application.Current.Shutdown();
                        return;
                    }
                }
            };
        }

        private void BtnBrowse_Click(object sender, RoutedEventArgs e)
        {
            var dlg = new OpenFolderDialog
            {
                Title = "Select Installation Directory",
                InitialDirectory = TxtInstallPath.Text
            };
            if (dlg.ShowDialog() == true)
            {
                TxtInstallPath.Text = dlg.FolderName;
            }
        }

        private void BtnCancel_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private async void BtnInstall_Click(object sender, RoutedEventArgs e)
        {
            if (_isCompleted)
            {
                if (ChkLaunchAfter.IsChecked == true && !string.IsNullOrEmpty(_installedExePath) && File.Exists(_installedExePath))
                {
                    try
                    {
                        Process.Start(new ProcessStartInfo(_installedExePath) { UseShellExecute = true });
                    }
                    catch { }
                }
                Close();
                return;
            }

            string targetDir = TxtInstallPath.Text.Trim();
            if (string.IsNullOrEmpty(targetDir))
            {
                MessageBox.Show("Please specify a valid installation folder.", "Invalid Folder", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            // Disable configuration controls during install
            BtnInstall.IsEnabled = false;
            BtnBrowse.IsEnabled = false;
            TxtInstallPath.IsEnabled = false;
            ChkDesktopShortcut.IsEnabled = false;
            ChkStartMenuShortcut.IsEnabled = false;
            ChkAssociateFlac.IsEnabled = false;
            ChkLaunchAfter.IsEnabled = false;
            BtnCancel.IsEnabled = false;
            CardProgress.Visibility = Visibility.Visible;

            bool desktopShortcut = ChkDesktopShortcut.IsChecked == true;
            bool startMenuShortcut = ChkStartMenuShortcut.IsChecked == true;
            bool associateFlac = ChkAssociateFlac.IsChecked == true;

            try
            {
                await Task.Run(() => PerformInstallation(targetDir, desktopShortcut, startMenuShortcut, associateFlac));

                _isCompleted = true;
                _installedExePath = Path.Combine(targetDir, "HiResPlayer.Desktop.exe");

                TxtStatus.Text = "Installation completed successfully!";
                TxtPercent.Text = "100%";
                PrgInstall.Value = 100;
                BtnInstall.Content = "Finish & Launch";
                BtnInstall.IsEnabled = true;
                BtnCancel.Content = "Close";
                BtnCancel.IsEnabled = true;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Installation failed:\n\n{ex.Message}", "Installation Error", MessageBoxButton.OK, MessageBoxImage.Error);
                BtnInstall.IsEnabled = true;
                BtnCancel.IsEnabled = true;
            }
        }

        private void PerformInstallation(string targetDir, bool desktopShortcut, bool startMenuShortcut, bool associateFlac)
        {
            UpdateProgress("Preparing target directory...", 5);
            Directory.CreateDirectory(targetDir);

            // 1. Extract embedded payload.zip
            UpdateProgress("Locating embedded application payload...", 10);
            var assembly = Assembly.GetExecutingAssembly();
            using var stream = assembly.GetManifestResourceStream("payload.zip");
            if (stream == null)
            {
                throw new InvalidOperationException("Embedded payload.zip resource not found in installer binary.");
            }

            using var archive = new ZipArchive(stream, ZipArchiveMode.Read);
            int totalEntries = archive.Entries.Count;
            int extracted = 0;

            string canonicalTarget = Path.GetFullPath(targetDir);
            if (!canonicalTarget.EndsWith(Path.DirectorySeparatorChar.ToString()))
            {
                canonicalTarget += Path.DirectorySeparatorChar;
            }

            foreach (var entry in archive.Entries)
            {
                string destinationPath = Path.GetFullPath(Path.Combine(targetDir, entry.FullName));
                if (!destinationPath.StartsWith(canonicalTarget, StringComparison.OrdinalIgnoreCase))
                {
                    throw new System.Security.SecurityException($"Zip slip vulnerability detected: entry '{entry.FullName}' targets outside '{targetDir}'");
                }

                if (string.IsNullOrEmpty(entry.Name) && (entry.FullName.EndsWith("/") || entry.FullName.EndsWith("\\")))
                {
                    // Directory entry
                    Directory.CreateDirectory(destinationPath);
                    continue;
                }

                string? parentDir = Path.GetDirectoryName(destinationPath);
                if (parentDir != null) Directory.CreateDirectory(parentDir);

                entry.ExtractToFile(destinationPath, overwrite: true);
                extracted++;

                int pct = 10 + (int)((extracted / (double)totalEntries) * 75);
                UpdateProgress($"Extracting {entry.Name}...", pct);
            }

            string exePath = Path.Combine(targetDir, "HiResPlayer.Desktop.exe");
            if (!File.Exists(exePath))
            {
                throw new FileNotFoundException("HiResPlayer.Desktop.exe was not found in the payload.", exePath);
            }

            // 2. Create Shortcuts
            UpdateProgress("Creating desktop and application shortcuts...", 88);
            CreateShortcuts(exePath, targetDir, desktopShortcut, startMenuShortcut);

            // 3. Register .flac File Association
            if (associateFlac)
            {
                UpdateProgress("Registering .flac file association...", 93);
                RegisterFileAssociation(exePath);
            }

            // 4. Create Uninstaller & Registry entry
            UpdateProgress("Registering Windows uninstall entry...", 97);
            RegisterUninstaller(targetDir, exePath);

            UpdateProgress("Installation finalized.", 100);
        }

        private void UpdateProgress(string status, int percent)
        {
            Dispatcher.Invoke(() =>
            {
                TxtStatus.Text = status;
                TxtPercent.Text = $"{percent}%";
                PrgInstall.Value = percent;
            });
        }

        private void CreateShortcuts(string exePath, string targetDir, bool desktop, bool startMenu)
        {
            try
            {
                Type? shellType = Type.GetTypeFromProgID("WScript.Shell");
                if (shellType == null) return;
                dynamic shell = Activator.CreateInstance(shellType)!;

                if (desktop)
                {
                    string desktopDir = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
                    dynamic shortcut = shell.CreateShortcut(Path.Combine(desktopDir, "HiRes Player.lnk"));
                    shortcut.TargetPath = exePath;
                    shortcut.WorkingDirectory = targetDir;
                    shortcut.IconLocation = $"{exePath},0";
                    shortcut.Description = "HiResPlayer — Bit-Perfect Reference Audio Player";
                    shortcut.Save();
                }

                if (startMenu)
                {
                    string programsDir = Environment.GetFolderPath(Environment.SpecialFolder.Programs);
                    string appFolder = Path.Combine(programsDir, "HiResPlayer");
                    Directory.CreateDirectory(appFolder);
                    dynamic shortcut = shell.CreateShortcut(Path.Combine(appFolder, "HiRes Player.lnk"));
                    shortcut.TargetPath = exePath;
                    shortcut.WorkingDirectory = targetDir;
                    shortcut.IconLocation = $"{exePath},0";
                    shortcut.Description = "HiResPlayer — Bit-Perfect Reference Audio Player";
                    shortcut.Save();
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Shortcut creation error: {ex.Message}");
            }
        }

        private void RegisterFileAssociation(string exePath)
        {
            try
            {
                // Register for current user (HKCU)
                using var extKey = Registry.CurrentUser.CreateSubKey(@"Software\Classes\.flac");
                extKey.SetValue("", "HiResPlayer.flac");

                using var progKey = Registry.CurrentUser.CreateSubKey(@"Software\Classes\HiResPlayer.flac");
                progKey.SetValue("", "FLAC Audio Track");

                using var iconKey = progKey.CreateSubKey("DefaultIcon");
                iconKey.SetValue("", $"{exePath},0");

                using var cmdKey = progKey.CreateSubKey(@"shell\open\command");
                cmdKey.SetValue("", $"\"{exePath}\" \"%1\"");
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"File association error: {ex.Message}");
            }
        }

        private void RegisterUninstaller(string targetDir, string exePath)
        {
            try
            {
                // Create uninstall.cmd script
                string uninstScript = Path.Combine(targetDir, "uninstall.cmd");
                string scriptContent = $@"@echo off
echo ===================================================
echo  Uninstalling HiResPlayer...
echo ===================================================
taskkill /F /IM HiResPlayer.Desktop.exe 2>nul
del /Q ""%USERPROFILE%\Desktop\HiRes Player.lnk"" 2>nul
rmdir /S /Q ""%APPDATA%\Microsoft\Windows\Start Menu\Programs\HiResPlayer"" 2>nul
reg delete ""HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\HiResPlayer"" /f 2>nul
reg delete ""HKCU\Software\Classes\HiResPlayer.flac"" /f 2>nul
echo Cleaned shortcuts and registrations.
cd ..
rmdir /S /Q ""{targetDir}"" 2>nul
echo HiResPlayer uninstallation complete.
";
                File.WriteAllText(uninstScript, scriptContent);

                // Add/Remove Programs entry
                using var uninstKey = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\HiResPlayer");
                uninstKey.SetValue("DisplayName", "HiRes Player — Studio Master");
                uninstKey.SetValue("DisplayVersion", "1.0.0");
                uninstKey.SetValue("Publisher", "HiResPlayer Audio Technologies");
                uninstKey.SetValue("DisplayIcon", $"{exePath},0");
                uninstKey.SetValue("InstallLocation", targetDir);
                uninstKey.SetValue("UninstallString", $"cmd.exe /c \"{uninstScript}\"");
                uninstKey.SetValue("EstimatedSize", 185000, RegistryValueKind.DWord);
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Uninstaller registration error: {ex.Message}");
            }
        }
    }
}
