using System;
using System.IO;
using System.Windows;
using System.Windows.Threading;

namespace HiResPlayer.Desktop
{
    public partial class App : System.Windows.Application
    {
        public static void Log(string msg)
        {
            try
            {
                string logPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "HiResPlayer", "app.log");
                Directory.CreateDirectory(Path.GetDirectoryName(logPath)!);
                File.AppendAllText(logPath, $"[{DateTime.UtcNow:HH:mm:ss.fff}] {msg}\n");
            }
            catch { }
        }

        protected override void OnStartup(StartupEventArgs e)
        {
            Log("App.OnStartup enter");
            DispatcherUnhandledException += App_DispatcherUnhandledException;
            AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException;
            base.OnStartup(e);
            Log("App.OnStartup finished");
        }

        protected override void OnExit(ExitEventArgs e)
        {
            Log($"App.OnExit called with exit code: {e.ApplicationExitCode}");
            base.OnExit(e);
        }

        private void App_DispatcherUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
        {
            string err = $"HiResPlayer encountered an unhandled error:\n\n{e.Exception}\n";
            try
            {
                string logPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "HiResPlayer", "crash.log");
                Directory.CreateDirectory(Path.GetDirectoryName(logPath)!);
                File.WriteAllText(logPath, err);
            }
            catch { }
            MessageBox.Show(err, "HiResPlayer Error", MessageBoxButton.OK, MessageBoxImage.Error);
            e.Handled = true;
        }

        private void CurrentDomain_UnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            if (e.ExceptionObject is Exception ex)
            {
                try
                {
                    string logPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "HiResPlayer", "crash.log");
                    Directory.CreateDirectory(Path.GetDirectoryName(logPath)!);
                    File.WriteAllText(logPath, $"Fatal Crash:\n{ex.Message}\n{ex.StackTrace}");
                }
                catch { }
            }
        }
    }
}
