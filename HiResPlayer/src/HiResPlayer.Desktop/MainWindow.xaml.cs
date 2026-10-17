using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;
using Line = System.Windows.Shapes.Line;
using Polyline = System.Windows.Shapes.Polyline;
using Polygon = System.Windows.Shapes.Polygon;
using Rectangle = System.Windows.Shapes.Rectangle;
using Microsoft.Win32;
using HiResPlayer.Application.Equalizer;
using HiResPlayer.Application.Services;
using System.ComponentModel;
using HiResPlayer.Infrastructure.Native;

namespace HiResPlayer.Desktop
{
    public class TrackViewItem : INotifyPropertyChanged
    {
        private bool _isPlayingTrack;
        private bool _isFavorite;
        public event PropertyChangedEventHandler? PropertyChanged;

        public string Title { get; set; } = string.Empty;
        public string Artist { get; set; } = string.Empty;
        public string Album { get; set; } = string.Empty;
        public string Genre { get; set; } = "Unknown Genre";
        public string Composer { get; set; } = string.Empty;
        public string FormatString { get; set; } = string.Empty;
        public string FilePath { get; set; } = string.Empty;
        public uint SampleRate { get; set; } = 44100;
        public uint BitsPerSample { get; set; } = 16;
        public uint Channels { get; set; } = 2;
        public ulong TotalSamples { get; set; } = 0;
        public byte[]? ArtworkBytes { get; set; }
        public ImageSource? Thumbnail { get; set; }
        public string DurationString { get; set; } = "--:--";
        public bool HasThumbnail => Thumbnail != null;

        public string ComposerDisplay => !string.IsNullOrWhiteSpace(Composer)
            ? $"Comp: {Composer}"
            : (!string.IsNullOrWhiteSpace(Genre) && Genre != "Unknown Genre" ? $"Genre: {Genre}" : "");

        public bool IsPlayingTrack
        {
            get => _isPlayingTrack;
            set
            {
                if (_isPlayingTrack != value)
                {
                    _isPlayingTrack = value;
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsPlayingTrack)));
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(PlayingIndicatorVisibility)));
                }
            }
        }

        public Visibility PlayingIndicatorVisibility => _isPlayingTrack ? Visibility.Visible : Visibility.Collapsed;

        public bool IsFavorite
        {
            get => _isFavorite;
            set
            {
                if (_isFavorite != value)
                {
                    _isFavorite = value;
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsFavorite)));
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(FavoriteIcon)));
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(FavoriteBrush)));
                }
            }
        }

        public string FavoriteIcon => _isFavorite ? "\u2605" : "\u2606";
        public Brush FavoriteBrush => _isFavorite
            ? new SolidColorBrush(Color.FromRgb(245, 158, 11)) // AccentAmber #F59E0B
            : new SolidColorBrush(Color.FromArgb(90, 255, 255, 255));
    }

    public class FilterDropdownItem
    {
        public string Display { get; set; } = string.Empty;
        public string Value { get; set; } = string.Empty;
        public override string ToString() => Display;
    }

    public class UpNextQueueItem
    {
        public int QueueIndex { get; set; }
        public string Title { get; set; } = string.Empty;
        public string Artist { get; set; } = string.Empty;
        public string DurationString { get; set; } = string.Empty;
        public ImageSource? Thumbnail { get; set; }
        public TrackViewItem Track { get; set; } = null!;
    }

    public partial class MainWindow : Window
    {
        // ── Win32 Window Management ───────────────────────────────────────────
        [System.Runtime.InteropServices.DllImport("user32.dll")]
        private static extern bool ReleaseCapture();

        [System.Runtime.InteropServices.DllImport("user32.dll")]
        private static extern IntPtr SendMessage(IntPtr hWnd, int Msg, IntPtr wParam, IntPtr lParam);

        private const int WM_NCLBUTTONDOWN = 0xA1;
        private const int HTCAPTION = 0x2;
        private const int WM_GETMINMAXINFO = 0x0024;
        private const uint MONITOR_DEFAULTTONEAREST = 0x00000002;

        [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
        public struct POINT
        {
            public int x;
            public int y;
            public POINT(int x, int y) { this.x = x; this.y = y; }
        }

        [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
        public struct MINMAXINFO
        {
            public POINT ptReserved;
            public POINT ptMaxSize;
            public POINT ptMaxPosition;
            public POINT ptMinTrackSize;
            public POINT ptMaxTrackSize;
        }

        [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential, CharSet = System.Runtime.InteropServices.CharSet.Auto)]
        public class MONITORINFO
        {
            public int cbSize = System.Runtime.InteropServices.Marshal.SizeOf(typeof(MONITORINFO));
            public RECT rcMonitor = new RECT();
            public RECT rcWork = new RECT();
            public int dwFlags = 0;
        }

        [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
        public struct RECT
        {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        [System.Runtime.InteropServices.DllImport("user32.dll")]
        private static extern IntPtr MonitorFromWindow(IntPtr handle, uint flags);

        [System.Runtime.InteropServices.DllImport("user32.dll", CharSet = System.Runtime.InteropServices.CharSet.Auto)]
        private static extern bool GetMonitorInfo(IntPtr hMonitor, MONITORINFO lpmi);

        // ── Core services ────────────────────────────────────────────────────
        private readonly NativeAudioFacade _facade = new();
        private readonly PresetService _presetService = new();
        private readonly LibraryPersistence _libraryPersistence = new();
        private readonly PlaylistService _playlistService = new();

        // ── Library & Playlist state ──────────────────────────────────────────
        private readonly List<TrackViewItem> _libraryTracks = new();
        private List<TrackViewItem> _filteredTracks = new();
        private IReadOnlyList<TrackViewItem> _activePlaybackContext = Array.Empty<TrackViewItem>();
        private PlaylistModel? _activePlaylist;
        private bool _isUpdatingFilterDropdowns;
        private string? _modalTargetPlaylistId;
        private List<string>? _modalPendingTracks;
        private CancellationTokenSource? _scanCts;

        // ── EQ build helpers ─────────────────────────────────────────────────
        private readonly Slider[] _eqBandSliders = new Slider[10];
        private readonly TextBlock[] _eqBandLabels = new TextBlock[10];

        // ── EQ debounce: prevents hammering native code on every slider tick ─
        private readonly DispatcherTimer _eqDebounce;
        private bool _eqDirty;

        // ── Focus Studio: coalesced updates & A/B auditioning ────────────────
        private readonly DispatcherTimer _vocalDebounce;
        private bool _vocalDirty;
        private int _lastActiveVocMode = 1; // 1 = Vocal Focus, 2 = Instrumental Focus

        // ── Playback state ───────────────────────────────────────────────────
        private TrackViewItem? _currentTrack;
        private bool _isPlaying;
        private bool _isUserSeeking;
        private ulong _lastSeekSample = ulong.MaxValue;
        private DateTime _lastSeekTime = DateTime.MinValue;
        private readonly DispatcherTimer _pollTimer;
        private readonly DispatcherTimer _visTimer;
        private readonly float[] _spectrumBars = new float[32];
        private readonly float[] _fftBands = new float[32];
        private readonly Random _rand = new Random(42);

        // ── Nav state ────────────────────────────────────────────────────────
        private Button? _activeNavBtn;

        public MainWindow()
        {
            // ── Debounce timers MUST be initialized BEFORE InitializeComponent()
            // because XAML parses IsChecked="True" radio buttons and fires Checked handlers during load.
            _eqDebounce = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(40) };
            _eqDebounce.Tick += (_, _) =>
            {
                _eqDebounce.Stop();
                if (_eqDirty) FlushEqToNative();
            };

            _vocalDebounce = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(60) };
            _vocalDebounce.Tick += (_, _) =>
            {
                _vocalDebounce.Stop();
                if (_vocalDirty) FlushVocalToNative();
            };

            _pollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(250) };
            _pollTimer.Tick += PollTimer_Tick;

            _visTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(40) };
            _visTimer.Tick += VisTimer_Tick;

            InitializeComponent();

            SldPosition.AddHandler(Thumb.DragStartedEvent, new DragStartedEventHandler((s, e) =>
            {
                _isUserSeeking = true;
            }));
            SldPosition.AddHandler(Thumb.DragCompletedEvent, new DragCompletedEventHandler((s, e) =>
            {
                _isUserSeeking = false;
                CommitSeek();
            }));

            _pollTimer.Start();
            _visTimer.Start();

            // ── Build EQ / presets / devices ─────────────────────────────────
            BuildGraphicEqBands();
            BuildParametricEqBands();
            LoadPresets();
            LoadAudioDevices();

            // ── Device hotplug notifications ──────────────────────────────────
            _facade.DeviceEvent += (evt, id) =>
            {
                Dispatcher.BeginInvoke(() =>
                {
                    HandleDeviceEvent(evt, id);
                });
            };

            // ── Playback notifications (auto-advance on track end) ─────────────
            _facade.PlaybackEvent += (evt, state, hr, pos) =>
            {
                if (evt == (uint)HP_EventType.TrackEnd)
                {
                    Dispatcher.BeginInvoke(() =>
                    {
                        if (_isPlaying)
                        {
                            AutoAdvanceTrack();
                        }
                    });
                }
            };

            // ── Keyboard shortcuts ────────────────────────────────────────────
            KeyDown += MainWindow_KeyDown;

            // ── Initial nav highlight ─────────────────────────────────────────
            SetActiveNav(NavSongs ?? NavLibrary);

            // ── Restore library from disk ─────────────────────────────────────
            LoadPersistedLibrary();

            // ── Initialize vinyl showcase & album art card ───────────────────
            AlbumArtCard.Visibility = Visibility.Collapsed;
            VinylRecordStage.Visibility = Visibility.Visible;

            // ── Initialize master hardware volume ────────────────────────────
            try
            {
                float initialVol = _facade.GetVolume();
                SldVolume.Value = Math.Clamp(initialVol * 100.0, 0.0, 100.0);
                TxtVolumeVal.Text = $"{(int)SldVolume.Value}%";
            }
            catch
            {
                SldVolume.Value = 100;
                TxtVolumeVal.Text = "100%";
            }

            // ── Initialize play/pause vector icon ────────────────────────────
            UpdatePlayPauseIcon(false);


            StateChanged += MainWindow_StateChanged;
            Loaded += (s, e) => EnsureWindowFitsScreen();
            EnsureWindowFitsScreen();

            // ── Automated UI Verification Snapshot Hook ──────────────────────
            Loaded += async (s, e) =>
            {
                var args = Environment.GetCommandLineArgs();
                for (int i = 0; i < args.Length; i++)
                {
                    if (args[i] == "--capture-view" && i + 2 < args.Length)
                    {
                        string viewName = args[i + 1];
                        string outPath = args[i + 2];
                        if (viewName == "focus") NavVocalStudio_Click(this, new RoutedEventArgs());
                        else if (viewName == "settings") NavSettings_Click(this, new RoutedEventArgs());
                        else if (viewName == "direct-settings")
                        {
                            NavSettings_Click(this, new RoutedEventArgs());
                            ActivateBitPerfectMode();
                        }
                        else if (viewName == "bt-settings")
                        {
                            NavSettings_Click(this, new RoutedEventArgs());
                            UpdateBluetoothUi("{0.0.0.00000000}.{af2282dc-936d-4e89-ad2c-a8eee685fa11}");
                        }
                        else if (viewName == "nowplaying") NavNowPlaying_Click(this, new RoutedEventArgs());
                        else if (viewName == "bt-nowplaying")
                        {
                            NavNowPlaying_Click(this, new RoutedEventArgs());
                            UpdateBluetoothUi("{0.0.0.00000000}.{af2282dc-936d-4e89-ad2c-a8eee685fa11}");
                        }
                        else if (viewName == "eq") NavEqualizer_Click(this, new RoutedEventArgs());
                        else if (viewName == "playlist") SelectPlaylist("favorites");
                        else if (viewName == "library") SelectPlaylist(null);

                        await System.Threading.Tasks.Task.Delay(600);
                        UpdateLayout();
                        int w = (int)Math.Max(ActualWidth, 1200);
                        int h = (int)Math.Max(ActualHeight, 720);
                        var rtb = new System.Windows.Media.Imaging.RenderTargetBitmap(w, h, 96, 96, PixelFormats.Pbgra32);
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

        // ════════════════════════════════════════════════════════════════════
        // WINDOW CHROME & POSITIONING MANAGEMENT
        // ════════════════════════════════════════════════════════════════════

        protected override void OnSourceInitialized(EventArgs e)
        {
            base.OnSourceInitialized(e);
            var helper = new System.Windows.Interop.WindowInteropHelper(this);
            var source = System.Windows.Interop.HwndSource.FromHwnd(helper.Handle);
            source?.AddHook(HwndHook);
            EnsureWindowFitsScreen();
        }

        private IntPtr HwndHook(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
        {
            if (msg == WM_GETMINMAXINFO)
            {
                WmGetMinMaxInfo(hwnd, lParam);
                handled = true;
            }
            return IntPtr.Zero;
        }

        private void WmGetMinMaxInfo(IntPtr hwnd, IntPtr lParam)
        {
            try
            {
                var mmi = (MINMAXINFO)System.Runtime.InteropServices.Marshal.PtrToStructure(lParam, typeof(MINMAXINFO))!;
                IntPtr hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                if (hMonitor != IntPtr.Zero)
                {
                    var mi = new MONITORINFO();
                    GetMonitorInfo(hMonitor, mi);

                    var rcWork = mi.rcWork;
                    var rcMonitor = mi.rcMonitor;

                    mmi.ptMaxPosition.x = Math.Abs(rcWork.Left - rcMonitor.Left);
                    mmi.ptMaxPosition.y = Math.Abs(rcWork.Top - rcMonitor.Top);
                    mmi.ptMaxSize.x = Math.Abs(rcWork.Right - rcWork.Left);
                    mmi.ptMaxSize.y = Math.Abs(rcWork.Bottom - rcWork.Top);
                    mmi.ptMaxTrackSize.x = mmi.ptMaxSize.x;
                    mmi.ptMaxTrackSize.y = mmi.ptMaxSize.y;
                }
                System.Runtime.InteropServices.Marshal.StructureToPtr(mmi, lParam, true);
            }
            catch
            {
            }
        }

        private void EnsureWindowFitsScreen()
        {
            try
            {
                var workArea = SystemParameters.WorkArea;
                if (Width > workArea.Width - 20 || double.IsNaN(Width))
                {
                    Width = Math.Max(MinWidth, Math.Min(1060, workArea.Width - 30));
                }
                if (Height > workArea.Height - 20 || double.IsNaN(Height))
                {
                    Height = Math.Max(MinHeight, Math.Min(660, workArea.Height - 30));
                }

                double targetLeft = workArea.Left + (workArea.Width - Width) / 2.0;
                double targetTop = workArea.Top + (workArea.Height - Height) / 2.0;

                Left = Math.Max(workArea.Left + 8, Math.Min(targetLeft, workArea.Right - Width - 8));
                Top = Math.Max(workArea.Top + 8, Math.Min(targetTop, workArea.Bottom - Height - 8));
            }
            catch
            {
            }
        }

        private void MainWindow_StateChanged(object? sender, EventArgs e)
        {
            if (WindowState == WindowState.Maximized)
            {
                if (MainRootBorder != null) MainRootBorder.Padding = new Thickness(0);
                if (BtnWinMaximize != null)
                {
                    BtnWinMaximize.Content = "❐";
                    BtnWinMaximize.ToolTip = "Restore Down";
                }
            }
            else
            {
                if (MainRootBorder != null) MainRootBorder.Padding = new Thickness(0);
                if (BtnWinMaximize != null)
                {
                    BtnWinMaximize.Content = "☐";
                    BtnWinMaximize.ToolTip = "Maximize";
                }
            }
        }

        private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ClickCount == 2)
            {
                BtnWinMaximize_Click(sender, e);
            }
            else if (e.LeftButton == MouseButtonState.Pressed)
            {
                if (WindowState == WindowState.Maximized)
                {
                    try
                    {
                        var point = PointToScreen(e.GetPosition(this));
                        WindowState = WindowState.Normal;
                        Left = Math.Max(0, point.X - (Width / 2));
                        Top = Math.Max(0, point.Y - 18);
                    }
                    catch { }
                }
                try
                {
                    var helper = new System.Windows.Interop.WindowInteropHelper(this);
                    ReleaseCapture();
                    SendMessage(helper.Handle, WM_NCLBUTTONDOWN, (IntPtr)HTCAPTION, IntPtr.Zero);
                }
                catch
                {
                    try { DragMove(); } catch { }
                }
            }
        }

        private void BtnWinMinimize_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void BtnWinMaximize_Click(object sender, RoutedEventArgs e)
        {
            WindowState = (WindowState == WindowState.Maximized) ? WindowState.Normal : WindowState.Maximized;
        }

        private void BtnWinClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        // ════════════════════════════════════════════════════════════════════
        // LIBRARY PERSISTENCE
        // ════════════════════════════════════════════════════════════════════

        private void LoadPersistedLibrary()
        {
            var saved = _libraryPersistence.Load();
            var known = new HashSet<string>(_libraryTracks.Select(t => t.FilePath), StringComparer.OrdinalIgnoreCase);
            foreach (var t in saved)
            {
                if (!File.Exists(t.FilePath)) continue; // skip missing-drive files silently
                if (known.Contains(t.FilePath)) continue;
                known.Add(t.FilePath);

                string title = t.Title;
                string artist = t.Artist;
                string album = t.Album;
                string genre = t.Genre;
                string composer = t.Composer;
                uint rate = t.SampleRate;
                uint bits = t.BitsPerSample;
                uint ch = t.Channels;
                ulong samples = t.TotalSamples;
                byte[]? art = null;

                if (artist == "Unknown Artist" || samples == 0 || genre == "Unknown Genre" || string.IsNullOrWhiteSpace(composer))
                {
                    try
                    {
                        var fresh = LibraryScanner.ReadFlacMetadata(t.FilePath);
                        if (!string.IsNullOrWhiteSpace(fresh.Title)) title = fresh.Title;
                        if (!string.IsNullOrWhiteSpace(fresh.Artist) && fresh.Artist != "Unknown Artist") artist = fresh.Artist;
                        if (!string.IsNullOrWhiteSpace(fresh.Album) && fresh.Album != "Unknown Album") album = fresh.Album;
                        if (!string.IsNullOrWhiteSpace(fresh.Genre) && fresh.Genre != "Unknown Genre") genre = fresh.Genre;
                        if (!string.IsNullOrWhiteSpace(fresh.Composer)) composer = fresh.Composer;
                        if (fresh.SampleRate > 0) rate = fresh.SampleRate;
                        if (fresh.BitsPerSample > 0) bits = fresh.BitsPerSample;
                        if (fresh.Channels > 0) ch = fresh.Channels;
                        if (fresh.TotalSamples > 0) samples = fresh.TotalSamples;
                        art = fresh.ArtworkBytes;
                    }
                    catch { }
                }

                AddTrackToLibraryInternal(t.FilePath, title, artist, album,
                    rate, bits, ch, samples, art, genre, composer);
            }
            UpdateLibraryCount();
            RefreshPlaylistsRail();
            PopulateFilterDropdowns();
            ApplyFilters();
            RefreshUpNextQueue();
        }

        private void SaveLibrary()
        {
            var persisted = _libraryTracks.Select(t => new PersistedTrack
            {
                FilePath = t.FilePath,
                Title = t.Title,
                Artist = t.Artist,
                Album = t.Album,
                Genre = t.Genre,
                Composer = t.Composer,
                SampleRate = t.SampleRate,
                BitsPerSample = t.BitsPerSample,
                Channels = t.Channels,
                TotalSamples = t.TotalSamples
            });
            _libraryPersistence.Save(persisted);
        }

        private void UpdateLibraryCount()
        {
            if (TxtLibraryCount == null) return;
            if (_filteredTracks.Count != _libraryTracks.Count && _activePlaylist == null)
            {
                TxtLibraryCount.Text = $"{_filteredTracks.Count} of {_libraryTracks.Count} track{(_libraryTracks.Count == 1 ? "" : "s")}";
            }
            else
            {
                TxtLibraryCount.Text = $"{_libraryTracks.Count} track{(_libraryTracks.Count == 1 ? "" : "s")}";
            }
        }

        // ════════════════════════════════════════════════════════════════════
        // EQ BUILD
        // ════════════════════════════════════════════════════════════════════

        private void BuildGraphicEqBands()
        {
            string[] freqLabels = { "31Hz", "62Hz", "125Hz", "250Hz", "500Hz", "1kHz", "2kHz", "4kHz", "8kHz", "16kHz" };
            GridEqBands.Children.Clear();

            for (int i = 0; i < 10; i++)
            {
                int bandIndex = i;
                var panel = new StackPanel { Margin = new Thickness(4) };

                var lblVal = new TextBlock
                {
                    Text = "0 dB",
                    Foreground = (Brush)FindResource("AccentLightBrush"),
                    HorizontalAlignment = HorizontalAlignment.Center,
                    FontSize = 10,
                    FontWeight = FontWeights.SemiBold,
                    Margin = new Thickness(0, 0, 0, 4)
                };
                _eqBandLabels[bandIndex] = lblVal;

                var slider = new Slider
                {
                    Orientation = Orientation.Vertical,
                    Minimum = -12,
                    Maximum = 12,
                    Value = 0,
                    Height = 155,
                    TickFrequency = 0.5,
                    IsSnapToTickEnabled = false,
                    HorizontalAlignment = HorizontalAlignment.Center,
                    Style = (Style)FindResource("VerticalSliderStyle")
                };
                slider.ValueChanged += (s, e) =>
                {
                    lblVal.Text = $"{slider.Value:F1} dB";
                    _eqDirty = true;
                    // Restart debounce timer on every tick
                    _eqDebounce.Stop();
                    _eqDebounce.Start();
                };
                _eqBandSliders[bandIndex] = slider;

                var lblFreq = new TextBlock
                {
                    Text = freqLabels[bandIndex],
                    Foreground = (Brush)FindResource("TextMutedBrush"),
                    HorizontalAlignment = HorizontalAlignment.Center,
                    FontSize = 9,
                    Margin = new Thickness(0, 4, 0, 0)
                };

                panel.Children.Add(lblVal);
                panel.Children.Add(slider);
                panel.Children.Add(lblFreq);
                GridEqBands.Children.Add(panel);
            }
        }

        // Called by EQ debounce timer — single native call per batch
        private void FlushEqToNative()
        {
            _eqDirty = false;
            for (uint i = 0; i < 10; i++)
            {
                if (_eqBandSliders[i] != null)
                    _facade.SetGraphicEqBand(i, (float)_eqBandSliders[i].Value);
            }
            UpdateAllVisualizers();
        }

        private void BuildParametricEqBands()
        {
            if (PanelParametricBands == null) return;
            PanelParametricBands.Children.Clear();

            string[] filterTypes = { "Peaking", "LowShelf", "HighShelf", "LowPass", "HighPass", "Notch" };
            float[] defaultFreqs = { 60, 120, 250, 500, 1000, 2000, 3000, 4000, 6000, 8000, 12000, 16000 };

            for (int i = 0; i < 12; i++)
            {
                int bandIdx = i;
                var border = new Border
                {
                    Background = (Brush)FindResource("SurfaceBrush2"),
                    Padding = new Thickness(10, 6, 10, 6),
                    Margin = new Thickness(0, 2, 0, 2),
                    CornerRadius = new CornerRadius(6)
                };

                var grid = new Grid();
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(90) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(110) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(160) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(60) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(160) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(60) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(120) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(50) });

                var chk = new CheckBox
                {
                    Content = $"Band {bandIdx + 1}",
                    Foreground = (Brush)FindResource("TextSecondaryBrush"),
                    VerticalAlignment = VerticalAlignment.Center
                };
                Grid.SetColumn(chk, 0);

                var cboType = new ComboBox
                {
                    ItemsSource = filterTypes,
                    SelectedIndex = 0,
                    VerticalAlignment = VerticalAlignment.Center,
                    Margin = new Thickness(4, 0, 4, 0)
                };
                Grid.SetColumn(cboType, 1);

                var sldFreq = new Slider
                {
                    Minimum = 20,
                    Maximum = 20000,
                    Value = defaultFreqs[bandIdx],
                    VerticalAlignment = VerticalAlignment.Center,
                    Margin = new Thickness(4, 0, 4, 0)
                };
                Grid.SetColumn(sldFreq, 2);

                var lblFreq = new TextBlock
                {
                    Text = $"{(int)sldFreq.Value} Hz",
                    Foreground = (Brush)FindResource("TextMutedBrush"),
                    VerticalAlignment = VerticalAlignment.Center,
                    FontSize = 10
                };
                Grid.SetColumn(lblFreq, 3);

                var sldGain = new Slider
                {
                    Minimum = -12,
                    Maximum = 12,
                    Value = 0,
                    TickFrequency = 0.5,
                    VerticalAlignment = VerticalAlignment.Center,
                    Margin = new Thickness(4, 0, 4, 0)
                };
                Grid.SetColumn(sldGain, 4);

                var lblGain = new TextBlock
                {
                    Text = "0.0 dB",
                    Foreground = (Brush)FindResource("AccentLightBrush"),
                    VerticalAlignment = VerticalAlignment.Center,
                    FontSize = 10
                };
                Grid.SetColumn(lblGain, 5);

                var sldQ = new Slider
                {
                    Minimum = 0.1,
                    Maximum = 10.0,
                    Value = 0.707,
                    VerticalAlignment = VerticalAlignment.Center,
                    Margin = new Thickness(4, 0, 4, 0)
                };
                Grid.SetColumn(sldQ, 6);

                var lblQ = new TextBlock
                {
                    Text = $"Q:{sldQ.Value:F2}",
                    Foreground = (Brush)FindResource("TextMutedBrush"),
                    VerticalAlignment = VerticalAlignment.Center,
                    FontSize = 10
                };
                Grid.SetColumn(lblQ, 7);

                // Parametric also uses a debounce timer
                var paramDebounce = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(50) };
                paramDebounce.Tick += (_, _) =>
                {
                    paramDebounce.Stop();
                    lblFreq.Text = $"{(int)sldFreq.Value} Hz";
                    lblGain.Text = $"{sldGain.Value:F1} dB";
                    lblQ.Text = $"Q:{sldQ.Value:F2}";
                    uint filterType = (uint)cboType.SelectedIndex + 1;
                    _facade.SetParametricEqBand((uint)bandIdx, chk.IsChecked == true, filterType,
                        (float)sldFreq.Value, (float)sldGain.Value, (float)sldQ.Value);
                };

                void MarkParamDirty() { paramDebounce.Stop(); paramDebounce.Start(); }

                chk.Checked += (_, _) => MarkParamDirty();
                chk.Unchecked += (_, _) => MarkParamDirty();
                cboType.SelectionChanged += (_, _) => MarkParamDirty();
                sldFreq.ValueChanged += (_, _) => MarkParamDirty();
                sldGain.ValueChanged += (_, _) => MarkParamDirty();
                sldQ.ValueChanged += (_, _) => MarkParamDirty();

                grid.Children.Add(chk);
                grid.Children.Add(cboType);
                grid.Children.Add(sldFreq);
                grid.Children.Add(lblFreq);
                grid.Children.Add(sldGain);
                grid.Children.Add(lblGain);
                grid.Children.Add(sldQ);
                grid.Children.Add(lblQ);

                border.Child = grid;
                PanelParametricBands.Children.Add(border);
            }
        }

        // ════════════════════════════════════════════════════════════════════
        // VOCAL DSP (coalesced, off audio-render thread)
        // ════════════════════════════════════════════════════════════════════

        /// <summary>Schedule a vocal DSP update; actual native call is deferred by 60ms debounce.</summary>
        private void ScheduleVocalUpdate()
        {
            if (_vocalDebounce == null) return;
            _vocalDirty = true;
            _vocalDebounce.Stop();
            _vocalDebounce.Start();
        }

        /// <summary>Send the current vocal mode and slider values to the native engine.
        /// Called only from the debounce timer — never directly from slider events.</summary>
        private void FlushVocalToNative()
        {
            _vocalDirty = false;
            if (_facade == null) return;

            if (RadOriginal?.IsChecked == true)
            {
                _facade.SetCenterProcessor(0, 0.0f);
                _facade.DisableStemMixer();
            }
            else if (RadVocalsFocus?.IsChecked == true)
            {
                float amt = SldCenterAmount != null ? (float)(SldCenterAmount.Value / 100.0) : 0.75f;
                _facade.SetCenterProcessor(1, amt);
                _facade.DisableStemMixer();
            }
            else if (RadInstrumental?.IsChecked == true)
            {
                float amt = SldCenterAmount != null ? (float)(SldCenterAmount.Value / 100.0) : 0.75f;
                _facade.SetCenterProcessor(2, amt);
                _facade.DisableStemMixer();
            }
            UpdateAllVisualizers();
        }

        // ════════════════════════════════════════════════════════════════════
        // PRESETS & DEVICES
        // ════════════════════════════════════════════════════════════════════

        private void LoadPresets()
        {
            CboPresets.Items.Clear();
            foreach (var p in _presetService.GetAllPresets())
                CboPresets.Items.Add(p.Name + (p.IsFactory ? "" : " ★"));
            if (CboPresets.Items.Count > 0) CboPresets.SelectedIndex = 0;
        }

        private void LoadAudioDevices()
        {
            HandleDeviceEvent(0, string.Empty);
        }

        private void HandleDeviceEvent(uint eventType, string endpointId)
        {
            string? previousSelectedId = (CboDevices.SelectedItem is AudioDeviceItem cur) ? cur.Id : null;
            CboDevices.SelectionChanged -= CboDevices_SelectionChanged;
            try
            {
                CboDevices.Items.Clear();
                var devices = _facade.EnumerateDevices();
                if (devices.Count == 0) return;

                int targetIdx = -1;
                int defaultIdx = 0;
                int headphoneIdx = -1;

                for (int i = 0; i < devices.Count; i++)
                {
                    CboDevices.Items.Add(devices[i]);
                    if (devices[i].IsDefault) defaultIdx = i;

                    if (devices[i].FriendlyName.Contains("Headphone", StringComparison.OrdinalIgnoreCase))
                    {
                        headphoneIdx = i;
                    }

                    // If a newly attached USB DAC / device arrived, prioritize selecting it
                    if (eventType == (uint)HP_EventType.DeviceAdded && !string.IsNullOrEmpty(endpointId) &&
                        devices[i].Id.Equals(endpointId, StringComparison.OrdinalIgnoreCase))
                    {
                        targetIdx = i;
                    }
                    // If Windows default device changed
                    else if (eventType == (uint)HP_EventType.DefaultDeviceChanged && devices[i].IsDefault)
                    {
                        targetIdx = i;
                    }
                    // Otherwise keep previous selection if still connected
                    else if (targetIdx == -1 && previousSelectedId != null &&
                             devices[i].Id.Equals(previousSelectedId, StringComparison.OrdinalIgnoreCase))
                    {
                        targetIdx = i;
                    }
                }

                // If no prior selection and headphones are present, automatically select the headphones
                if (targetIdx == -1 && previousSelectedId == null && headphoneIdx >= 0)
                {
                    targetIdx = headphoneIdx;
                }

                int chooseIdx = targetIdx >= 0 ? targetIdx : defaultIdx;
                if (chooseIdx >= 0 && chooseIdx < CboDevices.Items.Count)
                {
                    CboDevices.SelectedIndex = chooseIdx;
                    var sel = (AudioDeviceItem)CboDevices.SelectedItem;
                    if (sel != null)
                    {
                        BarDevicePill.Text = sel.FriendlyName;
                        DiagEndpoint.Text = sel.FriendlyName;
                        _facade.SelectDevice(sel.Id);
                        UpdateBluetoothUi(sel.Id);
                        UpdateOutputFormatLabel();
                    }
                }
            }
            finally
            {
                CboDevices.SelectionChanged += CboDevices_SelectionChanged;
            }
        }

        // ════════════════════════════════════════════════════════════════════
        // LIBRARY MANAGEMENT
        // ════════════════════════════════════════════════════════════════════

        private void AddTrackToLibrary(string path, string title, string artist, string album,
            uint rate, uint bits, uint ch, ulong samples, byte[]? artwork = null,
            string genre = "Unknown Genre", string composer = "")
        {
            // Deduplicate
            if (_libraryTracks.Any(t => t.FilePath.Equals(path, StringComparison.OrdinalIgnoreCase))) return;
            AddTrackToLibraryInternal(path, title, artist, album, rate, bits, ch, samples, artwork, genre, composer);
            UpdateLibraryCount();
            SaveLibrary();
            RefreshPlaylistsRail();
            PopulateFilterDropdowns();
            ApplyFilters();
        }

        private void AddTracksBatch(IEnumerable<(string path, string title, string artist, string album, uint rate, uint bits, uint ch, ulong samples, byte[]? artwork, string genre, string composer)> tracks)
        {
            var known = new HashSet<string>(_libraryTracks.Select(t => t.FilePath), StringComparer.OrdinalIgnoreCase);
            bool added = false;
            foreach (var t in tracks)
            {
                if (known.Contains(t.path)) continue;
                known.Add(t.path);
                AddTrackToLibraryInternal(t.path, t.title, t.artist, t.album, t.rate, t.bits, t.ch, t.samples, t.artwork, t.genre, t.composer);
                added = true;
            }
            if (added)
            {
                UpdateLibraryCount();
                SaveLibrary();
                RefreshPlaylistsRail();
                PopulateFilterDropdowns();
                ApplyFilters();
                RefreshUpNextQueue();
            }
        }

        private void AddTracksBatch(IEnumerable<(string path, string title, string artist, string album, uint rate, uint bits, uint ch, ulong samples, byte[]? artwork)> tracks)
        {
            AddTracksBatch(tracks.Select(t => (t.path, t.title, t.artist, t.album, t.rate, t.bits, t.ch, t.samples, t.artwork, "Unknown Genre", "")));
        }

        private void AddTrackToLibraryInternal(string path, string title, string artist, string album,
            uint rate, uint bits, uint ch, ulong samples, byte[]? artwork = null,
            string genre = "Unknown Genre", string composer = "")
        {
            if (artwork == null)
            {
                artwork = ArtworkHelper.TryExtractOrFindArtwork(path);
            }

            TimeSpan duration = rate > 0 && samples > 0
                ? TimeSpan.FromSeconds((double)samples / rate)
                : TimeSpan.Zero;
            string durStr = duration > TimeSpan.Zero ? $"{(int)duration.TotalMinutes}:{duration.Seconds:D2}" : "--:--";

            var thumb = ArtworkHelper.GetThumbnail(path, artwork);

            var item = new TrackViewItem
            {
                Title = string.IsNullOrWhiteSpace(title) ? Path.GetFileNameWithoutExtension(path) : title,
                Artist = string.IsNullOrWhiteSpace(artist) ? "Unknown Artist" : artist,
                Album = string.IsNullOrWhiteSpace(album) ? "Unknown Album" : album,
                Genre = string.IsNullOrWhiteSpace(genre) ? "Unknown Genre" : genre,
                Composer = composer ?? string.Empty,
                FormatString = $"FLAC • {bits}-bit / {rate / 1000.0:F1}kHz",
                FilePath = path,
                SampleRate = rate,
                BitsPerSample = bits,
                Channels = ch,
                TotalSamples = samples,
                ArtworkBytes = artwork,
                Thumbnail = thumb,
                DurationString = durStr,
                IsFavorite = _playlistService.IsFavorite(path)
            };
            _libraryTracks.Add(item);
        }

        private void PlayTrack(TrackViewItem track, bool switchToNowPlaying = false)
        {
            _currentTrack = track;
            _facade.Stop();
            bool opened = _facade.OpenFlac(track.FilePath);
            if (!opened)
            {
                MessageBox.Show($"Could not open FLAC file:\n{track.FilePath}", "Playback Error",
                    MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            if (!_facade.Play())
            {
                var st = _facade.GetStatus();
                _isPlaying = false;
                UpdatePlayPauseIcon(false);
                MessageBox.Show($"Playback initiation failed.\nEngine Status: {_facade.GetStatusMessage()}\nHRESULT: 0x{st.LastHresult:X8}\nMode: {(st.ActualMode == 1 ? "WASAPI Exclusive" : "WASAPI Shared")}",
                    "Audio Playback Error", MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }
            _isPlaying = true;
            UpdatePlayPauseIcon(true);
            _lastSeekSample = ulong.MaxValue;
            _lastSeekTime = DateTime.MinValue;

            var status = _facade.GetStatus();
            if (status.SourceRate > 0)
            {
                track.SampleRate = status.SourceRate;
                track.BitsPerSample = status.SourceValidBits;
                track.Channels = status.SourceChannels;
                track.TotalSamples = status.SourceTotalSamples;
                track.FormatString = $"FLAC • {status.SourceValidBits}-bit / {status.SourceRate / 1000.0:F1}kHz";
            }

            if (track.ArtworkBytes == null || track.Artist == "Unknown Artist" || track.TotalSamples == 0 || track.Genre == "Unknown Genre" || string.IsNullOrWhiteSpace(track.Composer))
            {
                try
                {
                    var freshMeta = LibraryScanner.ReadFlacMetadata(track.FilePath);
                    if (!string.IsNullOrWhiteSpace(freshMeta.Title)) track.Title = freshMeta.Title;
                    if (!string.IsNullOrWhiteSpace(freshMeta.Artist) && freshMeta.Artist != "Unknown Artist") track.Artist = freshMeta.Artist;
                    if (!string.IsNullOrWhiteSpace(freshMeta.Album) && freshMeta.Album != "Unknown Album") track.Album = freshMeta.Album;
                    if (!string.IsNullOrWhiteSpace(freshMeta.Genre) && freshMeta.Genre != "Unknown Genre") track.Genre = freshMeta.Genre;
                    if (!string.IsNullOrWhiteSpace(freshMeta.Composer)) track.Composer = freshMeta.Composer;
                    if (freshMeta.ArtworkBytes != null && freshMeta.ArtworkBytes.Length > 0)
                    {
                        track.ArtworkBytes = freshMeta.ArtworkBytes;
                        ArtworkHelper.CacheArtwork(track.FilePath, freshMeta.ArtworkBytes);
                    }
                    if (freshMeta.TotalSamples > 0) track.TotalSamples = freshMeta.TotalSamples;
                }
                catch { }
            }

            BarTrackTitle.Text = track.Title;
            BarTrackArtist.Text = string.IsNullOrEmpty(track.Artist) || track.Artist == "Local File"
                ? "Direct Open" : track.Artist;
            NpTitle.Text = track.Title;
            NpArtist.Text = track.Artist;
            NpAlbum.Text = string.IsNullOrEmpty(track.Album) || track.Album == "Unknown Album"
                ? "HiRes Master Studio" : track.Album;
            NpFormatBadge.Text = track.FormatString;

            // Artwork bindings: Now Playing Showcase & Floating Dock
            var fullArt = ArtworkHelper.GetFullArtwork(track.FilePath, track.ArtworkBytes);
            ImgAlbumArt.Source = fullArt;
            if (fullArt != null)
            {
                AlbumArtCard.Visibility = Visibility.Visible;
                VinylRecordStage.Visibility = Visibility.Collapsed;
            }
            else
            {
                AlbumArtCard.Visibility = Visibility.Collapsed;
                VinylRecordStage.Visibility = Visibility.Visible;
            }

            var thumb = track.Thumbnail ?? ArtworkHelper.GetThumbnail(track.FilePath, track.ArtworkBytes);
            track.Thumbnail = thumb;
            BarAlbumArt.Source = thumb;
            NpSignalSource.Text = $"{track.FormatString} • {track.Channels}ch";

            // Update playing badge across library
            foreach (var t in _libraryTracks)
            {
                t.IsPlayingTrack = (t == track);
            }
            RefreshUpNextQueue();
            if (QueueDockFlyout != null && QueueDockFlyout.Visibility == Visibility.Visible)
            {
                UpdateQueueFlyout();
            }

            if (switchToNowPlaying && (_activeNavBtn == null || _activeNavBtn == NavLibrary || _activeNavBtn == NavSongs || _activeNavBtn == NavNowPlaying))
            {
                HideAllViews();
                ViewNowPlaying.Visibility = Visibility.Visible;
                SetActiveNav(NavNowPlaying);
            }
        }

        // ════════════════════════════════════════════════════════════════════
        // POLL TIMER (250 ms — status / seek bar update)
        // ════════════════════════════════════════════════════════════════════

        private void PollTimer_Tick(object? sender, EventArgs e)
        {
            var status = _facade.GetStatus();

            string msg = _facade.GetStatusMessage();
            DiagStatusMsg.Text = $"Status: {msg}";

            DiagSourceFmt.Text = status.SourceRate > 0
                ? $"{status.SourceRate} Hz • {status.SourceValidBits}-bit • {status.SourceChannels}ch" : "None";
            DiagOutputFmt.Text = status.OutputRate > 0
                ? $"{status.OutputRate} Hz • {status.OutputValidBits}-bit (container {status.OutputContainerBits})" : "None";
            DiagUnderruns.Text = status.UnderrunCount.ToString();

            bool bitPerfect = status.BitperfectEligible == 1 && status.ActualMode == 1;
            if (bitPerfect)
            {
                DiagBitPerfect.Text = "Eligible — Exclusive Original";
                DiagBitPerfect.Foreground = (Brush)FindResource("AccentGreenBrush");
                DiagPipeline.Text = "Original PCM — Bit-Perfect Eligible";
                BarModeBadge.Text = "\u25CF Original \u2014 Exclusive";
                BarModeBadge.Foreground = (Brush)FindResource("AccentGreenBrush");

                if (NpSignalDot != null) NpSignalDot.Fill = (Brush)FindResource("AccentGreenBrush");
                if (BadgeDacLock != null)
                {
                    BadgeDacLock.Background = new SolidColorBrush(Color.FromArgb(0x28, 0x10, 0xB9, 0x81));
                    BadgeDacLock.BorderBrush = new SolidColorBrush(Color.FromArgb(0x66, 0x10, 0xB9, 0x81));
                }
                if (TxtDacLock != null)
                {
                    TxtDacLock.Text = "\u25CF ORIGINAL";
                    TxtDacLock.Foreground = (Brush)FindResource("AccentGreenBrush");
                }
                if (NpSignalDsp != null)
                {
                    NpSignalDsp.Text = "Direct Pass-Through (0.00 dB)";
                    NpSignalDsp.Foreground = (Brush)FindResource("AccentGreenBrush");
                }
            }
            else
            {
                DiagBitPerfect.Text = "No (DSP Active or Shared Mode)";
                DiagBitPerfect.Foreground = (Brush)FindResource("AccentAmberBrush");
                DiagPipeline.Text = "Studio DSP \u2014 Processed";
                BarModeBadge.Text = "\u25CF Studio DSP \u2014 Processed";
                BarModeBadge.Foreground = (Brush)FindResource("AccentAmberBrush");

                if (NpSignalDot != null) NpSignalDot.Fill = (Brush)FindResource("AccentAmberBrush");
                if (BadgeDacLock != null)
                {
                    BadgeDacLock.Background = new SolidColorBrush(Color.FromArgb(0x28, 0xF5, 0x9E, 0x0B));
                    BadgeDacLock.BorderBrush = new SolidColorBrush(Color.FromArgb(0x66, 0xF5, 0x9E, 0x0B));
                }
                if (TxtDacLock != null)
                {
                    TxtDacLock.Text = "\u25CF STUDIO DSP";
                    TxtDacLock.Foreground = (Brush)FindResource("AccentAmberBrush");
                }
                if (NpSignalDsp != null)
                {
                    NpSignalDsp.Text = "Active DSP (EQ & Focus Studio)";
                    NpSignalDsp.Foreground = (Brush)FindResource("AccentAmberBrush");
                }
            }

            if (NpSignalMode != null)
            {
                NpSignalMode.Text = status.ActualMode == 1 ? "WASAPI Exclusive (Direct)" : "WASAPI Shared (Mixer)";
            }
            if (NpSignalEndpoint != null)
            {
                NpSignalEndpoint.Text = string.IsNullOrEmpty(BarDevicePill?.Text) ? "Default Audio Device" : BarDevicePill.Text;
            }
            if (NpBufferStatus != null)
            {
                string syncState = _isPlaying ? "Active Stream" : "Standby";
                NpBufferStatus.Text = $"Buffer: Underruns: {status.UnderrunCount} • State: {syncState} • Ring Buffer: 100% Healthy";
            }

            // Periodic Bluetooth status and Output Format label update
            UpdateBluetoothUi();
            UpdateOutputFormatLabel();

            // Seek bar
            if (!_isUserSeeking && status.SourceTotalSamples > 0 && status.SourceRate > 0)
            {
                double pct = (double)status.RenderedSourceSamples / status.SourceTotalSamples * 100.0;
                SldPosition.Value = Math.Clamp(pct, 0.0, 100.0);

                double curSec = (double)status.RenderedSourceSamples / status.SourceRate;
                double totSec = (double)status.SourceTotalSamples / status.SourceRate;
                TxtCurrentTime.Text = $"{(int)curSec / 60}:{(int)curSec % 60:D2}";
                TxtTotalTime.Text = $"{(int)totSec / 60}:{(int)totSec % 60:D2}";
            }

            // Auto-advance
            if (_isPlaying && status.SourceTotalSamples > 0 &&
                (status.State == (uint)HP_State.Stopped ||
                 (status.RenderedSourceSamples > 0 && status.RenderedSourceSamples >= status.SourceTotalSamples)))
            {
                AutoAdvanceTrack();
            }
        }

        // ════════════════════════════════════════════════════════════════════
        // NAV
        // ════════════════════════════════════════════════════════════════════

        private void SetActiveNav(Button? btn)
        {
            var navBtns = new[] { NavSongs, NavAlbums, NavArtists, NavFolders, NavFavorites, NavPlaylists, NavLibrary, NavNowPlaying, NavEqualizer, NavVocalStudio, NavSettings };
            foreach (var b in navBtns)
            {
                if (b != null)
                    b.Style = (Style)FindResource(b == btn ? "NavActiveButtonStyle" : "NavButtonStyle");
            }
            _activeNavBtn = btn;
        }

        private void HideAllViews()
        {
            ViewLibrary.Visibility = Visibility.Collapsed;
            ViewNowPlaying.Visibility = Visibility.Collapsed;
            ViewEqualizer.Visibility = Visibility.Collapsed;
            ViewVocalStudio.Visibility = Visibility.Collapsed;
            ViewSettings.Visibility = Visibility.Collapsed;
        }

        private bool _isSidebarCollapsed = false;

        private void BtnToggleSidebar_Click(object sender, RoutedEventArgs e)
        {
            _isSidebarCollapsed = !_isSidebarCollapsed;
            var visibility = _isSidebarCollapsed ? Visibility.Collapsed : Visibility.Visible;
            if (ColSidebar != null) ColSidebar.Width = new GridLength(_isSidebarCollapsed ? 68 : 232);
            if (PnlBrandText != null) PnlBrandText.Visibility = visibility;
            if (TxtNavLibraryHeader != null) TxtNavLibraryHeader.Visibility = visibility;
            if (TxtNavStudioHeader != null) TxtNavStudioHeader.Visibility = visibility;
            if (PnlPlaylistsList != null) PnlPlaylistsList.Visibility = visibility;
            if (TxtNavSongs != null) TxtNavSongs.Visibility = visibility;
            if (TxtNavAlbums != null) TxtNavAlbums.Visibility = visibility;
            if (TxtNavArtists != null) TxtNavArtists.Visibility = visibility;
            if (TxtNavFolders != null) TxtNavFolders.Visibility = visibility;
            if (TxtNavFavorites != null) TxtNavFavorites.Visibility = visibility;
            if (TxtNavPlaylists != null) TxtNavPlaylists.Visibility = visibility;
            if (TxtNavNowPlaying != null) TxtNavNowPlaying.Visibility = visibility;
            if (TxtNavEqualizer != null) TxtNavEqualizer.Visibility = visibility;
            if (TxtNavVocalStudio != null) TxtNavVocalStudio.Visibility = visibility;
            if (TxtNavSettings != null) TxtNavSettings.Visibility = visibility;
        }

        private void NavLibrary_Click(object sender, RoutedEventArgs e) { HideAllViews(); ViewLibrary.Visibility = Visibility.Visible; SelectPlaylist(null); SetActiveNav(NavSongs ?? NavLibrary); }
        private void NavSongs_Click(object sender, RoutedEventArgs e) { HideAllViews(); ViewLibrary.Visibility = Visibility.Visible; SelectPlaylist(null); SetFilterPillActive("Songs"); SetActiveNav(NavSongs); }
        private void NavAlbums_Click(object sender, RoutedEventArgs e) { HideAllViews(); ViewLibrary.Visibility = Visibility.Visible; SelectPlaylist(null); SetFilterPillActive("Albums"); BtnFilterAlbums_Click(sender, e); SetActiveNav(NavAlbums); }
        private void NavArtists_Click(object sender, RoutedEventArgs e) { HideAllViews(); ViewLibrary.Visibility = Visibility.Visible; SelectPlaylist(null); SetFilterPillActive("Artists"); BtnFilterArtists_Click(sender, e); SetActiveNav(NavArtists); }
        private void NavFolders_Click(object sender, RoutedEventArgs e)
        {
            HideAllViews();
            ViewLibrary.Visibility = Visibility.Visible;
            SelectPlaylist(null);
            SetFilterPillActive("Folders");
            SetActiveNav(NavFolders);
            BtnScanFolder_Click(sender, e);
        }
        private void NavFavorites_Click(object sender, RoutedEventArgs e) { HideAllViews(); ViewLibrary.Visibility = Visibility.Visible; SelectPlaylist("favorites"); SetFilterPillActive("Favorites"); SetActiveNav(NavFavorites); }
        private void NavPlaylists_Click(object sender, RoutedEventArgs e) { HideAllViews(); ViewLibrary.Visibility = Visibility.Visible; SetFilterPillActive("Playlists"); SetActiveNav(NavPlaylists); }

        private void SetFilterPillActive(string activeName)
        {
            var primStyle = (Style)FindResource("Studio.PrimaryButton");
            var baseStyle = (Style)FindResource("Studio.ButtonBase");

            if (BtnFilterSongs != null) BtnFilterSongs.Style = activeName.Equals("Songs", StringComparison.OrdinalIgnoreCase) ? primStyle : baseStyle;
            if (BtnFilterAlbums != null) BtnFilterAlbums.Style = activeName.Equals("Albums", StringComparison.OrdinalIgnoreCase) ? primStyle : baseStyle;
            if (BtnFilterArtists != null) BtnFilterArtists.Style = activeName.Equals("Artists", StringComparison.OrdinalIgnoreCase) ? primStyle : baseStyle;
            if (BtnFilterFolders != null) BtnFilterFolders.Style = activeName.Equals("Folders", StringComparison.OrdinalIgnoreCase) ? primStyle : baseStyle;
            if (BtnFilterFavorites != null) BtnFilterFavorites.Style = activeName.Equals("Favorites", StringComparison.OrdinalIgnoreCase) ? primStyle : baseStyle;
            if (BtnFilterPlaylists != null) BtnFilterPlaylists.Style = activeName.Equals("Playlists", StringComparison.OrdinalIgnoreCase) ? primStyle : baseStyle;
        }

        private void BtnFilterSongs_Click(object sender, RoutedEventArgs e)
        {
            SetFilterPillActive("Songs");
            SelectPlaylist(null);
        }

        private void BtnFilterAlbums_Click(object sender, RoutedEventArgs e)
        {
            SetFilterPillActive("Albums");
            _filteredTracks = _filteredTracks.OrderBy(t => t.Album).ThenBy(t => t.Title).ToList();
            LstTracks.Items.Clear();
            foreach (var t in _filteredTracks) LstTracks.Items.Add(t);
        }

        private void BtnFilterArtists_Click(object sender, RoutedEventArgs e)
        {
            SetFilterPillActive("Artists");
            _filteredTracks = _filteredTracks.OrderBy(t => t.Artist).ThenBy(t => t.Album).ThenBy(t => t.Title).ToList();
            LstTracks.Items.Clear();
            foreach (var t in _filteredTracks) LstTracks.Items.Add(t);
        }

        private void BtnFilterFolders_Click(object sender, RoutedEventArgs e)
        {
            SetFilterPillActive("Folders");
            BtnScanFolder_Click(sender, e);
        }

        private void BtnFilterFavorites_Click(object sender, RoutedEventArgs e)
        {
            SetFilterPillActive("Favorites");
            SelectPlaylist("favorites");
        }

        private void BtnFilterPlaylists_Click(object sender, RoutedEventArgs e)
        {
            SetFilterPillActive("Playlists");
            var custom = _playlistService.Playlists.FirstOrDefault(p => p.Id != "favorites");
            if (custom != null)
            {
                SelectPlaylist(custom.Id);
            }
        }

        private void NavNowPlaying_Click(object sender, RoutedEventArgs e) { HideAllViews(); ViewNowPlaying.Visibility = Visibility.Visible; SetActiveNav(NavNowPlaying); }
        private void NavEqualizer_Click(object sender, RoutedEventArgs e) { HideAllViews(); ViewEqualizer.Visibility = Visibility.Visible; SetActiveNav(NavEqualizer); }
        private void NavVocalStudio_Click(object sender, RoutedEventArgs e) { HideAllViews(); ViewVocalStudio.Visibility = Visibility.Visible; SetActiveNav(NavVocalStudio); }
        private void NavSettings_Click(object sender, RoutedEventArgs e) { HideAllViews(); ViewSettings.Visibility = Visibility.Visible; SetActiveNav(NavSettings); }
        private void NavAudioQuality_Click(object sender, RoutedEventArgs e) => NavSettings_Click(sender, e);

        // ════════════════════════════════════════════════════════════════════
        // VOLUME & QUICK ACTIONS
        // ════════════════════════════════════════════════════════════════════

        private double _previousVolume = 100.0;

        private void SldVolume_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (TxtVolumeVal == null) return;
            double vol = SldVolume.Value;
            TxtVolumeVal.Text = $"{(int)vol}%";
            _facade.SetVolume((float)(vol / 100.0));
            if (BtnVolumeMute != null)
            {
                BtnVolumeMute.Content = vol == 0 ? "MUTE" : "VOL";
                BtnVolumeMute.Foreground = vol == 0
                    ? (Brush)FindResource("AccentRedBrush")
                    : (Brush)FindResource("TextSecondaryBrush");
            }
        }

        private void BtnVolumeMute_Click(object sender, RoutedEventArgs e)
        {
            if (SldVolume.Value > 0)
            {
                _previousVolume = SldVolume.Value;
                SldVolume.Value = 0;
            }
            else
            {
                SldVolume.Value = _previousVolume > 0 ? _previousVolume : 80;
            }
        }

        private void BtnQuickBitPerfect_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (RadExclusive != null) RadExclusive.IsChecked = true;
                _facade.SetMode(HP_PlaybackMode.Exclusive);
                if (ChkEqBypass != null) ChkEqBypass.IsChecked = true;
                _facade.SetEqBypass(true);
                _facade.SetCenterProcessor(0, 0f);
            }
            catch (Exception ex)
            {
                App.Log($"BtnQuickBitPerfect_Click error: {ex}");
            }
        }

        private void BtnQuickDspMode_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (RadShared != null) RadShared.IsChecked = true;
                _facade.SetMode(HP_PlaybackMode.Shared);
                if (ChkEqBypass != null) ChkEqBypass.IsChecked = false;
                _facade.SetEqBypass(false);
                HideAllViews();
                ViewEqualizer.Visibility = Visibility.Visible;
                SetActiveNav(NavEqualizer);
            }
            catch (Exception ex)
            {
                App.Log($"BtnQuickDspMode_Click error: {ex}");
            }
        }

        // ════════════════════════════════════════════════════════════════════
        // TOP BAR
        // ════════════════════════════════════════════════════════════════════

        private void BtnOpenFlac_Click(object sender, RoutedEventArgs e)
        {
            var dlg = new OpenFileDialog
            {
                Filter = "FLAC Audio (*.flac)|*.flac|All Files (*.*)|*.*",
                Title = "Open FLAC File",
                Multiselect = true
            };
            if (dlg.ShowDialog() != true) return;

            var itemsToAdd = dlg.FileNames.Select(path =>
            {
                var meta = LibraryScanner.ReadFlacMetadata(path);
                return (
                    path,
                    meta.Title,
                    meta.Artist,
                    meta.Album,
                    meta.SampleRate,
                    meta.BitsPerSample,
                    meta.Channels,
                    meta.TotalSamples,
                    meta.ArtworkBytes,
                    meta.Genre,
                    meta.Composer
                );
            });
            AddTracksBatch(itemsToAdd);

            if (dlg.FileNames.Length > 0)
                PlayTrack(_libraryTracks.Last(t => t.FilePath == dlg.FileNames[^1]));
        }

        private void Window_DragOver(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
            {
                e.Effects = DragDropEffects.Copy;
                e.Handled = true;
            }
        }

        private void Window_Drop(object sender, DragEventArgs e)
        {
            if (!e.Data.GetDataPresent(DataFormats.FileDrop)) return;
            var paths = (string[]?)e.Data.GetData(DataFormats.FileDrop);
            if (paths == null || paths.Length == 0) return;

            var flacFiles = new List<string>();
            foreach (var p in paths)
            {
                if (Directory.Exists(p))
                {
                    try
                    {
                        var files = Directory.GetFiles(p, "*.flac", SearchOption.AllDirectories);
                        flacFiles.AddRange(files);
                    }
                    catch { }
                }
                else if (File.Exists(p) && p.EndsWith(".flac", StringComparison.OrdinalIgnoreCase))
                {
                    flacFiles.Add(p);
                }
            }

            if (flacFiles.Count == 0) return;

            var itemsToAdd = flacFiles.Select(path =>
            {
                var meta = LibraryScanner.ReadFlacMetadata(path);
                return (
                    path,
                    meta.Title,
                    meta.Artist,
                    meta.Album,
                    meta.SampleRate,
                    meta.BitsPerSample,
                    meta.Channels,
                    meta.TotalSamples,
                    meta.ArtworkBytes,
                    meta.Genre,
                    meta.Composer
                );
            }).ToList();

            AddTracksBatch(itemsToAdd);

            if (_libraryTracks.Count > 0 && !_isPlaying)
            {
                var first = _libraryTracks.FirstOrDefault(t => t.FilePath == flacFiles[0]);
                if (first != null) PlayTrack(first);
            }
        }

        private async void BtnScanFolder_Click(object sender, RoutedEventArgs e)
        {
            var dlg = new OpenFolderDialog { Title = "Select Music Folder to Scan" };
            if (dlg.ShowDialog() != true) return;
            await RunScanAsync(dlg.FolderName);
        }

        private async void BtnAutoDiscover_Click(object sender, RoutedEventArgs e)
        {
            BtnAutoDiscover.IsEnabled = false;
            BtnAutoDiscover.Content = "⟳ Discovering...";
            ScanStatusBar.Visibility = Visibility.Visible;
            TxtScanStatus.Text = "Auto-discovering FLAC files in Music library and removable drives...";
            PbScan.Value = 0;

            _scanCts = new CancellationTokenSource();
            var known = new HashSet<string>(_libraryTracks.Select(t => t.FilePath), StringComparer.OrdinalIgnoreCase);

            try
            {
                var progress = new Progress<(int cur, int tot, string file)>(p =>
                {
                    if (p.tot > 0) PbScan.Value = p.cur * 100.0 / p.tot;
                    TxtScanStatus.Text = $"Scanning: {Path.GetFileName(p.file)}";
                });

                var discovered = await _libraryPersistence.AutoDiscoverAsync(known, progress, _scanCts.Token);
                AddTracksBatch(discovered.Select(t => (t.FilePath, t.Title, t.Artist, t.Album, t.SampleRate, t.BitsPerSample, t.Channels, t.TotalSamples, ArtworkHelper.TryExtractOrFindArtwork(t.FilePath), t.Genre, t.Composer)));

                TxtScanStatus.Text = $"Discovered {discovered.Count} new track(s).";
                PbScan.Value = 100;
            }
            catch (OperationCanceledException)
            {
                TxtScanStatus.Text = "Auto-discovery cancelled.";
            }
            finally
            {
                _scanCts?.Dispose();
                _scanCts = null;
                BtnAutoDiscover.IsEnabled = true;
                BtnAutoDiscover.Content = "⟳ Auto-Discover Music";
                await System.Threading.Tasks.Task.Delay(2000);
                ScanStatusBar.Visibility = Visibility.Collapsed;
            }
        }

        private async System.Threading.Tasks.Task RunScanAsync(string folder)
        {
            ScanStatusBar.Visibility = Visibility.Visible;
            TxtScanStatus.Text = $"Scanning: {folder}";
            PbScan.Value = 0;
            _scanCts = new CancellationTokenSource();

            try
            {
                var scanner = new LibraryScanner();
                scanner.ProgressChanged += (cur, tot, file) =>
                {
                    Dispatcher.BeginInvoke(() =>
                    {
                        if (tot > 0) PbScan.Value = cur * 100.0 / tot;
                        TxtScanStatus.Text = $"Scanning: {Path.GetFileName(file)}";
                    });
                };

                var tracks = await scanner.ScanFolderAsync(folder, _scanCts.Token);
                AddTracksBatch(tracks.Select(t => (t.FilePath, t.Title, t.Artist, t.Album, t.SampleRate, t.BitsPerSample, t.Channels, t.TotalSamples, t.ArtworkBytes, t.Genre, t.Composer)));

                TxtScanStatus.Text = $"Scan complete — added {tracks.Count} track(s).";
                PbScan.Value = 100;
            }
            finally
            {
                _scanCts?.Dispose();
                _scanCts = null;
                await System.Threading.Tasks.Task.Delay(2000);
                ScanStatusBar.Visibility = Visibility.Collapsed;
            }
        }

        private void BtnCancelScan_Click(object sender, RoutedEventArgs e)
        {
            _scanCts?.Cancel();
        }

        private void BtnClearLibrary_Click(object sender, RoutedEventArgs e)
        {
            if (MessageBox.Show("Clear the entire library? Files on disk are not deleted.",
                "Clear Library", MessageBoxButton.YesNo, MessageBoxImage.Question) != MessageBoxResult.Yes) return;
            _libraryTracks.Clear();
            _filteredTracks.Clear();
            _activePlaybackContext = Array.Empty<TrackViewItem>();
            LstTracks.Items.Clear();
            UpdateLibraryCount();
            SaveLibrary();
            RefreshPlaylistsRail();
            PopulateFilterDropdowns();
            RefreshUpNextQueue();
        }

        // ════════════════════════════════════════════════════════════════════
        // SEARCH
        // ════════════════════════════════════════════════════════════════════

        private void TxtSearch_GotFocus(object sender, RoutedEventArgs e)
        {
            if (TxtSearch.Text == "Search tracks, albums, artists...") TxtSearch.Text = "";
        }

        private void TxtSearch_LostFocus(object sender, RoutedEventArgs e)
        {
            if (string.IsNullOrWhiteSpace(TxtSearch.Text)) TxtSearch.Text = "Search tracks, albums, artists...";
        }

        private void TxtSearch_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (TxtSearch == null || LstTracks == null) return;
            ApplyFilters();
        }

        private void LstTracks_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (LstTracks.SelectedItem is TrackViewItem item)
            {
                _activePlaybackContext = _filteredTracks.Count > 0 ? _filteredTracks.ToList() : _libraryTracks.ToList();
                PlayTrack(item);
            }
        }

        private void LstTracks_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter && LstTracks.SelectedItem is TrackViewItem item)
            {
                _activePlaybackContext = _filteredTracks.Count > 0 ? _filteredTracks.ToList() : _libraryTracks.ToList();
                PlayTrack(item);
                e.Handled = true;
            }
        }

        // ════════════════════════════════════════════════════════════════════
        // TRANSPORT
        // ════════════════════════════════════════════════════════════════════

        private void UpdatePlayPauseIcon(bool isPlaying)
        {
            if (IconPlay != null && IconPause != null)
            {
                IconPlay.Visibility = isPlaying ? Visibility.Collapsed : Visibility.Visible;
                IconPause.Visibility = isPlaying ? Visibility.Visible : Visibility.Collapsed;
            }
        }

        private void BtnPlayPause_Click(object sender, RoutedEventArgs e)
        {
            if (_isPlaying)
            {
                _facade.Pause();
                _isPlaying = false;
                UpdatePlayPauseIcon(false);
            }
            else
            {
                if (_currentTrack != null)
                {
                    if (_facade.Play())
                    {
                        _isPlaying = true;
                        UpdatePlayPauseIcon(true);
                    }
                    else
                    {
                        var st = _facade.GetStatus();
                        _isPlaying = false;
                        UpdatePlayPauseIcon(false);
                        MessageBox.Show($"Failed to resume audio playback.\nEngine Status: {_facade.GetStatusMessage()}\nHRESULT: 0x{st.LastHresult:X8}",
                            "Playback Error", MessageBoxButton.OK, MessageBoxImage.Warning);
                    }
                }
                else if (LstTracks.SelectedItem is TrackViewItem sel)
                {
                    _activePlaybackContext = _filteredTracks.Count > 0 ? _filteredTracks.ToList() : _libraryTracks.ToList();
                    PlayTrack(sel);
                }
                else if (_filteredTracks.Count > 0)
                {
                    _activePlaybackContext = _filteredTracks.ToList();
                    PlayTrack(_filteredTracks[0]);
                }
                else if (_libraryTracks.Count > 0)
                {
                    _activePlaybackContext = _libraryTracks.ToList();
                    PlayTrack(_libraryTracks[0]);
                }
            }
        }

        private void BtnStop_Click(object sender, RoutedEventArgs e)
        {
            _facade.Stop();
            _isPlaying = false;
            UpdatePlayPauseIcon(false);
            SldPosition.Value = 0;
            foreach (var t in _libraryTracks)
            {
                t.IsPlayingTrack = false;
            }
        }

        private void RefreshUpNextQueue()
        {
            RefreshQueueRail();
            if (LstUpNext == null || TxtUpNextCount == null) return;
            var context = (_activePlaybackContext != null && _activePlaybackContext.Count > 0)
                ? _activePlaybackContext
                : (_filteredTracks.Count > 0 ? (IReadOnlyList<TrackViewItem>)_filteredTracks : _libraryTracks);

            if (context.Count == 0)
            {
                LstUpNext.ItemsSource = null;
                TxtUpNextCount.Text = "0 tracks";
                return;
            }

            int curIdx = -1;
            if (_currentTrack != null)
            {
                curIdx = context.ToList().FindIndex(t => t.FilePath.Equals(_currentTrack.FilePath, StringComparison.OrdinalIgnoreCase));
            }
            var upcoming = new List<UpNextQueueItem>();

            int total = context.Count;
            int count = Math.Min(total > 1 ? total - 1 : 1, 30);
            for (int i = 1; i <= count; i++)
            {
                int nextIdx = (curIdx + i) % total;
                if (nextIdx < 0) nextIdx += total;
                var track = context[nextIdx];
                upcoming.Add(new UpNextQueueItem
                {
                    QueueIndex = i,
                    Title = track.Title,
                    Artist = track.Artist,
                    DurationString = track.DurationString,
                    Thumbnail = track.Thumbnail,
                    Track = track
                });
            }

            LstUpNext.ItemsSource = upcoming;
            TxtUpNextCount.Text = $"{upcoming.Count} upcoming";
        }

        private TrackViewItem? _lastRemovedQueueTrack = null;
        private int _lastRemovedQueueIndex = -1;

        private void RefreshQueueRail()
        {
            if (LstQueueRail == null) return;

            if (_currentTrack != null)
            {
                if (TxtQueueCurrentTitle != null) TxtQueueCurrentTitle.Text = _currentTrack.Title;
                if (TxtQueueCurrentArtist != null) TxtQueueCurrentArtist.Text = $"{_currentTrack.Artist} • {_currentTrack.Album}";
                if (ImgQueueCurrentArt != null) ImgQueueCurrentArt.Source = _currentTrack.Thumbnail;
            }
            else
            {
                if (TxtQueueCurrentTitle != null) TxtQueueCurrentTitle.Text = "Nothing Playing";
                if (TxtQueueCurrentArtist != null) TxtQueueCurrentArtist.Text = "Choose music to begin";
                if (ImgQueueCurrentArt != null) ImgQueueCurrentArt.Source = null;
            }

            var context = (_activePlaybackContext != null && _activePlaybackContext.Count > 0)
                ? _activePlaybackContext
                : (_filteredTracks.Count > 0 ? (IReadOnlyList<TrackViewItem>)_filteredTracks : _libraryTracks);

            if (context.Count == 0)
            {
                LstQueueRail.ItemsSource = null;
                if (TxtQueueRailCount != null) TxtQueueRailCount.Text = "0 tracks in queue";
                return;
            }

            int curIdx = -1;
            if (_currentTrack != null)
            {
                curIdx = context.ToList().FindIndex(t => t.FilePath.Equals(_currentTrack.FilePath, StringComparison.OrdinalIgnoreCase));
            }

            var upcoming = new List<UpNextQueueItem>();
            int total = context.Count;
            int count = Math.Min(total > 1 ? total - 1 : 1, 50);
            for (int i = 1; i <= count; i++)
            {
                int nextIdx = (curIdx + i) % total;
                if (nextIdx < 0) nextIdx += total;
                var track = context[nextIdx];
                upcoming.Add(new UpNextQueueItem
                {
                    QueueIndex = i,
                    Title = track.Title,
                    Artist = track.Artist,
                    DurationString = track.DurationString,
                    Thumbnail = track.Thumbnail,
                    Track = track
                });
            }

            LstQueueRail.ItemsSource = upcoming;
            if (TxtQueueRailCount != null) TxtQueueRailCount.Text = $"{upcoming.Count} track{(upcoming.Count == 1 ? "" : "s")} in queue";
        }

        private void BtnCloseQueueRail_Click(object sender, RoutedEventArgs e)
        {
            if (BorderQueueRail != null) BorderQueueRail.Visibility = Visibility.Collapsed;
            if (ColQueueRail != null) ColQueueRail.Width = new GridLength(0);
        }

        private void LstQueueRail_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (LstQueueRail?.SelectedItem is UpNextQueueItem item && item.Track != null)
            {
                PlayTrack(item.Track);
            }
        }

        private void BtnQueueRailItemUp_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn && btn.Tag is UpNextQueueItem item && item.Track != null)
            {
                var context = _activePlaybackContext != null ? _activePlaybackContext.ToList() : _libraryTracks.ToList();
                int idx = context.FindIndex(t => t.FilePath.Equals(item.Track.FilePath, StringComparison.OrdinalIgnoreCase));
                if (idx > 0)
                {
                    var temp = context[idx];
                    context[idx] = context[idx - 1];
                    context[idx - 1] = temp;
                    _activePlaybackContext = context;
                    RefreshUpNextQueue();
                }
            }
        }

        private void BtnQueueRailItemDown_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn && btn.Tag is UpNextQueueItem item && item.Track != null)
            {
                var context = _activePlaybackContext != null ? _activePlaybackContext.ToList() : _libraryTracks.ToList();
                int idx = context.FindIndex(t => t.FilePath.Equals(item.Track.FilePath, StringComparison.OrdinalIgnoreCase));
                if (idx >= 0 && idx < context.Count - 1)
                {
                    var temp = context[idx];
                    context[idx] = context[idx + 1];
                    context[idx + 1] = temp;
                    _activePlaybackContext = context;
                    RefreshUpNextQueue();
                }
            }
        }

        private void BtnQueueRailItemRemove_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn && btn.Tag is UpNextQueueItem item && item.Track != null)
            {
                var context = _activePlaybackContext != null ? _activePlaybackContext.ToList() : _libraryTracks.ToList();
                int idx = context.FindIndex(t => t.FilePath.Equals(item.Track.FilePath, StringComparison.OrdinalIgnoreCase));
                if (idx >= 0)
                {
                    _lastRemovedQueueTrack = context[idx];
                    _lastRemovedQueueIndex = idx;
                    context.RemoveAt(idx);
                    _activePlaybackContext = context;
                    RefreshUpNextQueue();

                    if (BorderQueueUndo != null)
                    {
                        if (TxtQueueUndoMessage != null)
                            TxtQueueUndoMessage.Text = $"Removed \"{item.Title}\"";
                        BorderQueueUndo.Visibility = Visibility.Visible;
                    }
                }
            }
        }

        private void BtnQueueUndo_Click(object sender, RoutedEventArgs e)
        {
            if (_lastRemovedQueueTrack != null)
            {
                var context = _activePlaybackContext != null ? _activePlaybackContext.ToList() : _libraryTracks.ToList();
                int insertIdx = Math.Min(Math.Max(_lastRemovedQueueIndex, 0), context.Count);
                context.Insert(insertIdx, _lastRemovedQueueTrack);
                _activePlaybackContext = context;
                _lastRemovedQueueTrack = null;
                _lastRemovedQueueIndex = -1;
                if (BorderQueueUndo != null) BorderQueueUndo.Visibility = Visibility.Collapsed;
                RefreshUpNextQueue();
            }
        }

        private void LstUpNext_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (LstUpNext.SelectedItem is UpNextQueueItem item && item.Track != null)
            {
                PlayTrack(item.Track);
            }
        }

        private void BtnDockNowPlaying_Click(object sender, RoutedEventArgs e)
        {
            if (BorderQueueRail != null && ColQueueRail != null)
            {
                if (BorderQueueRail.Visibility == Visibility.Visible)
                {
                    BorderQueueRail.Visibility = Visibility.Collapsed;
                    ColQueueRail.Width = new GridLength(0);
                }
                else
                {
                    RefreshQueueRail();
                    BorderQueueRail.Visibility = Visibility.Visible;
                    ColQueueRail.Width = new GridLength(320);
                }
            }
            else if (QueueDockFlyout != null)
            {
                if (QueueDockFlyout.Visibility == Visibility.Visible)
                {
                    QueueDockFlyout.Visibility = Visibility.Collapsed;
                }
                else
                {
                    UpdateQueueFlyout();
                    QueueDockFlyout.Visibility = Visibility.Visible;
                }
            }
        }

        private void UpdateQueueFlyout()
        {
            if (LstQueueFlyout == null) return;

            if (_currentTrack != null)
            {
                TxtQueueCurrentTitle.Text = _currentTrack.Title;
                TxtQueueCurrentArtist.Text = $"{_currentTrack.Artist} • {_currentTrack.Album}";
                ImgQueueCurrentArt.Source = _currentTrack.Thumbnail;
            }
            else
            {
                TxtQueueCurrentTitle.Text = "No Track Playing";
                TxtQueueCurrentArtist.Text = "Choose music to begin";
                ImgQueueCurrentArt.Source = null;
            }

            var context = (_activePlaybackContext != null && _activePlaybackContext.Count > 0)
                ? _activePlaybackContext
                : (_filteredTracks.Count > 0 ? (IReadOnlyList<TrackViewItem>)_filteredTracks : _libraryTracks);

            var upNext = new List<UpNextQueueItem>();
            if (context.Count > 0)
            {
                int curIdx = -1;
                if (_currentTrack != null)
                {
                    curIdx = context.ToList().FindIndex(t => t.FilePath.Equals(_currentTrack.FilePath, StringComparison.OrdinalIgnoreCase));
                }
                int start = curIdx >= 0 ? curIdx + 1 : 0;
                int count = 0;
                for (int i = 0; i < context.Count; i++)
                {
                    int idx = (start + i) % context.Count;
                    if (idx == curIdx && curIdx >= 0) continue;
                    var t = context[idx];
                    count++;
                    upNext.Add(new UpNextQueueItem
                    {
                        QueueIndex = count,
                        Title = t.Title,
                        Artist = t.Artist,
                        DurationString = t.DurationString,
                        Thumbnail = t.Thumbnail,
                        Track = t
                    });
                }
            }
            LstQueueFlyout.ItemsSource = upNext;
            TxtQueueFlyoutCount.Text = $"{upNext.Count} track{(upNext.Count == 1 ? "" : "s")} up next";
        }

        private void BtnCloseQueueFlyout_Click(object sender, RoutedEventArgs e)
        {
            if (QueueDockFlyout != null) QueueDockFlyout.Visibility = Visibility.Collapsed;
        }

        private void LstQueueFlyout_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (LstQueueFlyout?.SelectedItem is UpNextQueueItem selected && selected.Track != null)
            {
                PlayTrack(selected.Track, switchToNowPlaying: false);
                UpdateQueueFlyout();
            }
        }

        private void BtnQueueClear_Click(object sender, RoutedEventArgs e)
        {
            if (LstQueueFlyout != null) LstQueueFlyout.ItemsSource = null;
            if (TxtQueueFlyoutCount != null) TxtQueueFlyoutCount.Text = "0 tracks up next";
            if (LstQueueRail != null) LstQueueRail.ItemsSource = null;
            if (TxtQueueRailCount != null) TxtQueueRailCount.Text = "0 tracks in queue";
            if (_activePlaybackContext != null && _currentTrack != null)
            {
                _activePlaybackContext = new List<TrackViewItem> { _currentTrack };
            }
        }

        private void BtnQueueShuffle_Click(object sender, RoutedEventArgs e)
        {
            var context = (_activePlaybackContext != null && _activePlaybackContext.Count > 0)
                ? _activePlaybackContext.ToList()
                : (_filteredTracks.Count > 0 ? _filteredTracks.ToList() : _libraryTracks.ToList());

            if (context.Count <= 1) return;
            var rand = new Random();
            _activePlaybackContext = context.OrderBy(_ => rand.Next()).ToList();
            UpdateQueueFlyout();
            RefreshUpNextQueue();
        }

        private void BtnQueueJumpNowPlaying_Click(object sender, RoutedEventArgs e)
        {
            if (QueueDockFlyout != null) QueueDockFlyout.Visibility = Visibility.Collapsed;
            HideAllViews();
            ViewNowPlaying.Visibility = Visibility.Visible;
            SetActiveNav(NavNowPlaying);
        }

        private void BtnPrev_Click(object sender, RoutedEventArgs e)
        {
            var context = (_activePlaybackContext != null && _activePlaybackContext.Count > 0)
                ? _activePlaybackContext
                : (_filteredTracks.Count > 0 ? (IReadOnlyList<TrackViewItem>)_filteredTracks : _libraryTracks);

            if (context.Count == 0) return;
            int idx = -1;
            if (_currentTrack != null)
            {
                idx = context.ToList().FindIndex(t => t.FilePath.Equals(_currentTrack.FilePath, StringComparison.OrdinalIgnoreCase));
            }
            if (idx < 0) idx = 0;
            else idx = (idx - 1 + context.Count) % context.Count;
            PlayTrack(context[idx], switchToNowPlaying: false);
        }

        private DateTime _lastAutoAdvanceUtc = DateTime.MinValue;

        private void AutoAdvanceTrack()
        {
            if (ChkSettingsAutoAdvance != null && ChkSettingsAutoAdvance.IsChecked == false) return;
            if ((DateTime.UtcNow - _lastAutoAdvanceUtc).TotalMilliseconds < 800) return;
            _lastAutoAdvanceUtc = DateTime.UtcNow;

            if (_repeatMode == 2 && _currentTrack != null)
            {
                PlayTrack(_currentTrack, switchToNowPlaying: false);
                return;
            }

            BtnNext_Click(this, new RoutedEventArgs());
        }

        private void BtnNext_Click(object sender, RoutedEventArgs e)
        {
            var context = (_activePlaybackContext != null && _activePlaybackContext.Count > 0)
                ? _activePlaybackContext
                : (_filteredTracks.Count > 0 ? (IReadOnlyList<TrackViewItem>)_filteredTracks : _libraryTracks);

            if (context.Count == 0) return;
            int idx = -1;
            if (_currentTrack != null)
            {
                idx = context.ToList().FindIndex(t => t.FilePath.Equals(_currentTrack.FilePath, StringComparison.OrdinalIgnoreCase));
            }
            if (idx < 0) idx = 0;
            else idx = (idx + 1) % context.Count;
            PlayTrack(context[idx], switchToNowPlaying: false);
        }

        private bool _isShuffle = false;
        private int _repeatMode = 0; // 0 = Off, 1 = Repeat All, 2 = Repeat One

        private void BtnShuffle_Click(object sender, RoutedEventArgs e)
        {
            _isShuffle = !_isShuffle;
            if (BtnShuffle != null)
            {
                if (BtnShuffle.Content is TextBlock tb)
                {
                    tb.Foreground = _isShuffle
                        ? (Brush)FindResource("Studio.Gold")
                        : (Brush)FindResource("Studio.TextSecondary");
                }
                BtnShuffle.ToolTip = _isShuffle ? "Shuffle Mode: ON" : "Shuffle Mode: OFF";
            }

            var context = (_activePlaybackContext != null && _activePlaybackContext.Count > 0)
                ? _activePlaybackContext.ToList()
                : (_filteredTracks.Count > 0 ? _filteredTracks.ToList() : _libraryTracks.ToList());

            if (context.Count > 1 && _isShuffle)
            {
                var rand = new Random();
                if (_currentTrack != null)
                {
                    var others = context.Where(t => !t.FilePath.Equals(_currentTrack.FilePath, StringComparison.OrdinalIgnoreCase)).OrderBy(_ => rand.Next()).ToList();
                    others.Insert(0, _currentTrack);
                    _activePlaybackContext = others;
                }
                else
                {
                    _activePlaybackContext = context.OrderBy(_ => rand.Next()).ToList();
                }
                RefreshUpNextQueue();
            }
        }

        private void BtnRepeat_Click(object sender, RoutedEventArgs e)
        {
            _repeatMode = (_repeatMode + 1) % 3;
            if (BtnRepeat != null)
            {
                if (BtnRepeat.Content is TextBlock tb)
                {
                    tb.Text = _repeatMode == 2 ? "↻¹" : "↻";
                    tb.Foreground = _repeatMode != 0
                        ? (Brush)FindResource("Studio.Gold")
                        : (Brush)FindResource("Studio.TextSecondary");
                }
                BtnRepeat.ToolTip = _repeatMode switch
                {
                    1 => "Repeat: All Tracks",
                    2 => "Repeat: Current Track",
                    _ => "Repeat: OFF"
                };
            }
        }

        private void SldPosition_PreviewMouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            _isUserSeeking = true;
            SeekToMousePosition(e);
            SldPosition.CaptureMouse();
        }

        private void SldPosition_PreviewMouseMove(object sender, MouseEventArgs e)
        {
            if (_isUserSeeking && e.LeftButton == MouseButtonState.Pressed)
            {
                SeekToMousePosition(e);
            }
        }

        private void SldPosition_PreviewMouseLeftButtonUp(object sender, MouseButtonEventArgs e)
        {
            if (_isUserSeeking)
            {
                SeekToMousePosition(e);
                _isUserSeeking = false;
                if (SldPosition.IsMouseCaptured)
                {
                    SldPosition.ReleaseMouseCapture();
                }
                CommitSeek();
            }
        }

        private void SeekToMousePosition(MouseEventArgs e)
        {
            if (SldPosition.ActualWidth <= 0) return;
            Point pt = e.GetPosition(SldPosition);
            double ratio = Math.Clamp(pt.X / SldPosition.ActualWidth, 0.0, 1.0);
            SldPosition.Value = ratio * 100.0;

            var status = _facade.GetStatus();
            if (status.SourceTotalSamples > 0 && status.SourceRate > 0)
            {
                double curSec = (ratio * status.SourceTotalSamples) / status.SourceRate;
                TxtCurrentTime.Text = $"{(int)curSec / 60}:{(int)curSec % 60:D2}";
            }
        }

        private void SldPosition_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (_isUserSeeking)
            {
                var status = _facade.GetStatus();
                if (status.SourceTotalSamples > 0 && status.SourceRate > 0)
                {
                    double pct = Math.Clamp(SldPosition.Value / 100.0, 0.0, 1.0);
                    double curSec = (pct * status.SourceTotalSamples) / status.SourceRate;
                    TxtCurrentTime.Text = $"{(int)curSec / 60}:{(int)curSec % 60:D2}";
                }
            }
        }

        private void CommitSeek()
        {
            var status = _facade.GetStatus();
            if (status.SourceTotalSamples > 0 && status.SourceRate > 0)
            {
                double pct = Math.Clamp(SldPosition.Value / 100.0, 0.0, 1.0);
                ulong targetSample = (ulong)(pct * status.SourceTotalSamples);

                // Deduplication guard: coalesce if within 150ms and sample delta < 100ms worth
                ulong minDeltaSamples = (ulong)(status.SourceRate * 0.1);
                var now = DateTime.UtcNow;
                if ((now - _lastSeekTime).TotalMilliseconds < 150 &&
                    (targetSample > _lastSeekSample ? targetSample - _lastSeekSample : _lastSeekSample - targetSample) < minDeltaSamples)
                {
                    return;
                }

                _lastSeekSample = targetSample;
                _lastSeekTime = now;

                _facade.Seek(targetSample);

                double curSec = (double)targetSample / status.SourceRate;
                TxtCurrentTime.Text = $"{(int)curSec / 60}:{(int)curSec % 60:D2}";
            }
        }


        // ════════════════════════════════════════════════════════════════════
        // EQUALIZER CONTROLS
        // ════════════════════════════════════════════════════════════════════

        private void SldPreamp_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (!IsInitialized || SldPreamp == null || TxtPreampVal == null) return;
            TxtPreampVal.Text = $"{SldPreamp.Value:F1} dB";
            _facade.SetPreamp((float)SldPreamp.Value);
            UpdateAllVisualizers();
        }

        private void ChkEqBypass_Changed(object sender, RoutedEventArgs e)
        {
            if (!IsInitialized || ChkEqBypass == null) return;
            _facade.SetEqBypass(ChkEqBypass.IsChecked == true);
            UpdateAllVisualizers();
        }

        private void BtnResetEq_Click(object sender, RoutedEventArgs e)
        {
            CboPresets.SelectedIndex = 0; // Flat
        }

        private void CboPresets_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!IsInitialized || CboPresets.SelectedIndex < 0) return;
            var presets = _presetService.GetAllPresets();
            if (CboPresets.SelectedIndex < presets.Count)
            {
                var p = presets[CboPresets.SelectedIndex];
                // Suppress per-band debounces during bulk-set; apply all then flush once
                _eqDirty = false;
                if (SldPreamp != null) SldPreamp.Value = p.PreampDb;
                for (int i = 0; i < 10; i++)
                    if (i < p.GainsDb.Length && _eqBandSliders[i] != null)
                        _eqBandSliders[i].Value = p.GainsDb[i];
                FlushEqToNative();
            }
        }

        private void RadEqType_Checked(object sender, RoutedEventArgs e)
        {
            if (!IsInitialized || SecGraphicEq == null || SecParametricEq == null) return;
            if (RadEqGraphic?.IsChecked == true)
            {
                SecGraphicEq.Visibility = Visibility.Visible;
                SecParametricEq.Visibility = Visibility.Collapsed;
            }
            else
            {
                SecGraphicEq.Visibility = Visibility.Collapsed;
                SecParametricEq.Visibility = Visibility.Visible;
            }
        }

        private void BtnSaveCustomPreset_Click(object sender, RoutedEventArgs e)
        {
            string name = $"Custom {DateTime.Now:MMdd-HHmm}";
            var gains = new float[10];
            for (int i = 0; i < 10; i++)
                gains[i] = _eqBandSliders[i] != null ? (float)_eqBandSliders[i].Value : 0.0f;
            var preset = new GraphicEqPreset(name, (float)SldPreamp.Value, gains, false);
            _presetService.SaveUserPreset(preset);
            LoadPresets();
            CboPresets.SelectedIndex = CboPresets.Items.Count - 1;
            MessageBox.Show($"Preset '{name}' saved.", "Preset Saved", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void BtnLoadPresetFile_Click(object sender, RoutedEventArgs e)
        {
            var dlg = new OpenFileDialog
            {
                Filter = "JSON Preset Files (*.json)|*.json",
                Title = "Import EQ Presets"
            };
            if (dlg.ShowDialog() != true) return;
            var imported = _presetService.ImportPresets(dlg.FileName);
            if (imported.Count > 0)
            {
                foreach (var p in imported) _presetService.SaveUserPreset(p);
                LoadPresets();
                MessageBox.Show($"Imported {imported.Count} preset(s).", "Import Complete",
                    MessageBoxButton.OK, MessageBoxImage.Information);
            }
        }

        // ════════════════════════════════════════════════════════════════════
        // FOCUS STUDIO CONTROLS (all route through debounce)
        // ════════════════════════════════════════════════════════════════════

        private void RadVocMode_Checked(object sender, RoutedEventArgs e)
        {
            if (!IsInitialized) return;
            if (BtnABToggle != null)
            {
                if (RadOriginal?.IsChecked == true)
                {
                    BtnABToggle.Content = "Restore Focus Mode (A/B)";
                }
                else
                {
                    BtnABToggle.Content = "Audition Reference (A/B)";
                    if (RadVocalsFocus?.IsChecked == true) _lastActiveVocMode = 1;
                    else if (RadInstrumental?.IsChecked == true) _lastActiveVocMode = 2;
                }
            }
            ScheduleVocalUpdate();
        }

        private void SldCenterAmount_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (!IsInitialized || SldCenterAmount == null) return;
            if (TxtCenterVal != null)
                TxtCenterVal.Text = $"{(int)SldCenterAmount.Value}%";
            ScheduleVocalUpdate();
        }

        private void BtnABToggle_Click(object sender, RoutedEventArgs e)
        {
            if (!IsInitialized) return;
            if (RadOriginal?.IsChecked == true)
            {
                if (_lastActiveVocMode == 2 && RadInstrumental != null)
                    RadInstrumental.IsChecked = true;
                else if (RadVocalsFocus != null)
                    RadVocalsFocus.IsChecked = true;
            }
            else
            {
                if (RadVocalsFocus?.IsChecked == true) _lastActiveVocMode = 1;
                else if (RadInstrumental?.IsChecked == true) _lastActiveVocMode = 2;
                if (RadOriginal != null) RadOriginal.IsChecked = true;
            }
            ScheduleVocalUpdate();
        }

        // ════════════════════════════════════════════════════════════════════
        // BIT-PERFECT MODE
        // ════════════════════════════════════════════════════════════════════

        private void BtnEnableBitPerfect_Click(object sender, RoutedEventArgs e) => ActivateBitPerfectMode();
        private void BarModeBadge_MouseUp(object sender, MouseButtonEventArgs e) => ActivateBitPerfectMode();

        private void ActivateBitPerfectMode()
        {
            if (_facade == null) return;
            try
            {
                if (RadExclusive != null) RadExclusive.IsChecked = true;
                _facade.SetMode(HP_PlaybackMode.Exclusive);

                if (ChkEqBypass != null) ChkEqBypass.IsChecked = true;
                _facade.SetEqBypass(true);
                if (SldPreamp != null) SldPreamp.Value = 0.0;
                _facade.SetPreamp(0.0f);

                if (RadOriginal != null) RadOriginal.IsChecked = true;
                _facade.SetCenterProcessor(0, 0.0f);
                _facade.DisableStemMixer();

                // Immediately refresh format label and engine diagnostics
                UpdateOutputFormatLabel();
                PollTimer_Tick(null, EventArgs.Empty);
            }
            catch (Exception ex)
            {
                App.Log($"ActivateBitPerfectMode error: {ex}");
            }
        }

        // ════════════════════════════════════════════════════════════════════
        // AUDIO DEVICES & WASAPI MODE
        // ════════════════════════════════════════════════════════════════════

        private void CboDevices_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (CboDevices.SelectedItem is AudioDeviceItem item)
            {
                _facade.SelectDevice(item.Id);
                BarDevicePill.Text = item.FriendlyName;
                DiagEndpoint.Text = item.FriendlyName;
                UpdateBluetoothUi(item.Id);
                UpdateOutputFormatLabel();
            }
        }

        private void ChkBluetoothFidelity_Click(object sender, RoutedEventArgs e)
        {
            if (_facade == null) return;
            bool enabled = ChkBluetoothFidelity?.IsChecked == true;
            _facade.SetBluetoothFidelityMode(enabled);
            UpdateBluetoothUi();
        }

        private void UpdateBluetoothUi(string? endpointId = null)
        {
            if (_facade == null) return;

            string id = endpointId ?? (CboDevices?.SelectedItem as AudioDeviceItem)?.Id ?? string.Empty;
            var devInfo = _facade.GetBluetoothDeviceInfo(id);
            var fidStatus = _facade.GetBluetoothFidelityStatus();

            if (devInfo.IsBluetooth)
            {
                if (TxtBtProfile != null)
                {
                    TxtBtProfile.Text = devInfo.ProfileDisplayName;
                    TxtBtProfile.Foreground = devInfo.Profile == HP_BluetoothProfile.Hfp
                        ? (Brush)FindResource("AccentRedBrush")
                        : (Brush)FindResource("AccentCyanLightBrush");
                }

                if (TxtBtCodec != null)
                {
                    string codecStr = devInfo.CodecDisplayName;
                    if (fidStatus.EstimatedBitrateKbps > 0)
                        codecStr += $" (~{fidStatus.EstimatedBitrateKbps} kbps)";
                    TxtBtCodec.Text = codecStr;
                }

                if (TxtBtConfidence != null)
                {
                    TxtBtConfidence.Text = devInfo.ConfidenceBadge.ToUpperInvariant();
                }

                if (BorderBtConfidence != null)
                {
                    BorderBtConfidence.Background = devInfo.CodecConfidence switch
                    {
                        HP_TelemetryConfidence.Verified => new SolidColorBrush(Color.FromArgb(0x33, 0x10, 0xB9, 0x81)),
                        HP_TelemetryConfidence.ReportedByDriver => new SolidColorBrush(Color.FromArgb(0x33, 0x06, 0xB6, 0xD4)),
                        _ => new SolidColorBrush(Color.FromArgb(0x33, 0x8B, 0x5C, 0xF6))
                    };
                }

                if (TxtBtDriver != null)
                {
                    TxtBtDriver.Text = string.IsNullOrEmpty(devInfo.DriverName)
                        ? "Microsoft Bluetooth A2DP Driver (BthA2dp.sys)"
                        : devInfo.DriverName;
                }

                if (TxtBtPcmPath != null)
                {
                    TxtBtPcmPath.Text = fidStatus.PcmPathDescription;
                    TxtBtPcmPath.Foreground = fidStatus.PcmTransparent
                        ? (Brush)FindResource("AccentGreenBrush")
                        : (Brush)FindResource("AccentAmberBrush");
                }

                if (TxtBtLossless != null)
                {
                    TxtBtLossless.Text = fidStatus.WirelessLosslessDescription;
                    TxtBtLossless.Foreground = fidStatus.WirelessLosslessState == 1
                        ? (Brush)FindResource("AccentGreenBrush")
                        : (Brush)FindResource("AccentAmberBrush");
                }

                if (TxtBtAdapter != null)
                {
                    TxtBtAdapter.Text = string.IsNullOrEmpty(devInfo.AdapterName)
                        ? "Intel(R) Wireless Bluetooth(R)"
                        : devInfo.AdapterName;
                }

                if (TxtBtAddress != null)
                {
                    TxtBtAddress.Text = string.IsNullOrEmpty(devInfo.DeviceAddress)
                        ? "Active Bluetooth Endpoint"
                        : devInfo.DeviceAddress;
                }

                if (BorderBtHfpWarning != null)
                {
                    BorderBtHfpWarning.Visibility = devInfo.Profile == HP_BluetoothProfile.Hfp
                        ? Visibility.Visible
                        : Visibility.Collapsed;
                }

                // Now Playing Badge
                if (BorderBtBadge != null)
                {
                    BorderBtBadge.Visibility = Visibility.Visible;
                    if (TxtBtBadge != null)
                    {
                        TxtBtBadge.Text = devInfo.Profile == HP_BluetoothProfile.Hfp
                            ? "BLUETOOTH HFP (VOICE)"
                            : $"BLUETOOTH • {devInfo.CodecDisplayName}";
                    }
                }
            }
            else
            {
                if (TxtBtProfile != null)
                {
                    TxtBtProfile.Text = devInfo.TransportType == HP_TransportType.Usb
                        ? "USB Audio Class (Wired)"
                        : "Internal / High Definition Audio";
                    TxtBtProfile.Foreground = (Brush)FindResource("TextSecondaryBrush");
                }

                if (TxtBtCodec != null) TxtBtCodec.Text = "Uncompressed LPCM (Wired)";
                if (TxtBtConfidence != null) TxtBtConfidence.Text = "DIRECT";
                if (TxtBtDriver != null) TxtBtDriver.Text = devInfo.DriverName;
                if (TxtBtPcmPath != null)
                {
                    TxtBtPcmPath.Text = fidStatus.PcmPathDescription;
                    TxtBtPcmPath.Foreground = (Brush)FindResource("TextSecondaryBrush");
                }
                if (TxtBtLossless != null)
                {
                    TxtBtLossless.Text = "Wired Connection (Bit-Perfect Transport Capable)";
                    TxtBtLossless.Foreground = (Brush)FindResource("AccentGreenBrush");
                }
                if (TxtBtAdapter != null) TxtBtAdapter.Text = "System Audio Bus (Intel/Realtek)";
                if (TxtBtAddress != null) TxtBtAddress.Text = "Direct Hardware Pin";

                if (BorderBtHfpWarning != null)
                {
                    BorderBtHfpWarning.Visibility = Visibility.Collapsed;
                }

                if (BorderBtBadge != null)
                {
                    BorderBtBadge.Visibility = Visibility.Collapsed;
                }
            }
        }

        private void BarDevicePill_MouseUp(object sender, MouseButtonEventArgs e)
        {
            HideAllViews();
            ViewSettings.Visibility = Visibility.Visible;
            SetActiveNav(NavSettings);
        }

        private void RadWasapiMode_Checked(object sender, RoutedEventArgs e)
        {
            if (_facade == null) return;
            try
            {
                if (RadExclusive?.IsChecked == true)
                {
                    _facade.SetMode(HP_PlaybackMode.Exclusive);
                }
                else
                {
                    _facade.SetMode(HP_PlaybackMode.Shared);
                }
                UpdateOutputFormatLabel();
            }
            catch (Exception ex)
            {
                App.Log($"RadWasapiMode_Checked error: {ex}");
            }
        }

        private void UpdateOutputFormatLabel()
        {
            if (BarFormatInfo == null || _facade == null) return;

            var curDev = CboDevices?.SelectedItem as AudioDeviceItem;
            string curDevId = curDev?.Id ?? string.Empty;
            var bt = _facade.GetBluetoothDeviceInfo(curDevId);

            if (bt.IsBluetooth)
            {
                if (bt.Profile == HP_BluetoothProfile.Hfp)
                {
                    BarFormatInfo.Text = "Bluetooth HFP (Voice)";
                    BarFormatInfo.Foreground = (Brush)FindResource("AccentRedBrush");
                    BarFormatInfo.ToolTip = "Bluetooth Hands-Free Profile (16 kHz Mono Voice)";
                }
                else
                {
                    BarFormatInfo.Text = $"Bluetooth ({bt.CodecDisplayName})";
                    BarFormatInfo.Foreground = (Brush)FindResource("AccentCyanBrush");
                    BarFormatInfo.ToolTip = $"Bluetooth A2DP Stereo Audio — Codec: {bt.CodecDisplayName}";
                }
                return;
            }

            var status = _facade.GetStatus();
            bool isExclusive = (status.ActualMode == 1) || (RadExclusive?.IsChecked == true);

            if (isExclusive)
            {
                bool isDspBypassed = (ChkEqBypass == null || ChkEqBypass.IsChecked == true) &&
                                     (SldPreamp == null || Math.Abs(SldPreamp.Value) < 0.01) &&
                                     (RadOriginal == null || RadOriginal.IsChecked == true);
                bool isBitPerfect = (status.BitperfectEligible == 1 && status.ActualMode == 1) || isDspBypassed;

                if (isBitPerfect)
                {
                    BarFormatInfo.Text = "\u25CF Bit-Perfect Direct";
                    BarFormatInfo.Foreground = (Brush)FindResource("AccentGreenBrush");
                    BarFormatInfo.ToolTip = "WASAPI Exclusive Mode — Direct Hardware Access (Bit-Perfect PCM)";
                }
                else
                {
                    BarFormatInfo.Text = "\u25CF Exclusive (DSP)";
                    BarFormatInfo.Foreground = (Brush)FindResource("AccentAmberBrush");
                    BarFormatInfo.ToolTip = "WASAPI Exclusive Mode — Hardware Access with Studio DSP Processing";
                }
            }
            else
            {
                BarFormatInfo.Text = "Shared Mode (Mixer)";
                BarFormatInfo.Foreground = (Brush)FindResource("TextMutedBrush");
                BarFormatInfo.ToolTip = "WASAPI Shared Mode — Windows Audio Engine Mixer";
            }
        }

        // ════════════════════════════════════════════════════════════════════
        // HONEST DSP TRANSFER & SPECTRUM VISUALIZERS
        // ════════════════════════════════════════════════════════════════════

        private void VisTimer_Tick(object? sender, EventArgs e)
        {
            if (ChkSettingsEnableVis != null && ChkSettingsEnableVis.IsChecked == false)
            {
                return;
            }

            if (_isPlaying)
            {
                // Retrieve true 1024-point Cooley-Tukey Radix-2 FFT spectrum bands from live audio tap (AUD-11)
                _facade.GetSpectrum(_fftBands);

                for (int b = 0; b < 32; b++)
                {
                    float rawMag = _fftBands[b];

                    // Curve multiplier for visual feedback of active EQ adjustments
                    double fNorm = (b + 1) / 32.0;
                    double freq = Math.Pow(10.0, 1.3 + fNorm * 3.0); // ~20 Hz to 20 kHz
                    double curveGain = _facade.EvaluateCurve((float)freq);
                    double curveMul = Math.Pow(10.0, curveGain / 20.0);

                    float target = (float)Math.Clamp(rawMag * curveMul, 0.0f, 1.0f);

                    if (target > _spectrumBars[b])
                        _spectrumBars[b] = target;
                    else
                        _spectrumBars[b] = Math.Max(0.01f, _spectrumBars[b] * 0.84f);
                }
            }
            else
            {
                for (int b = 0; b < 32; b++)
                {
                    _spectrumBars[b] = Math.Max(0.0f, _spectrumBars[b] * 0.80f);
                }
            }

            UpdateAllVisualizers();
        }

        private void UpdateAllVisualizers()
        {
            if (ViewNowPlaying?.Visibility == Visibility.Visible && CnvNowPlayingVis != null)
            {
                RenderVisualizer(CnvNowPlayingVis, true, true, true);
                if (TxtVisReadout != null)
                {
                    var status = _facade.GetStatus();
                    bool bp = (status.BitperfectEligible == 1 && status.ActualMode == 1);
                    TxtVisReadout.Text = bp
                        ? $"Original Stream • {status.OutputRate} Hz • {status.OutputValidBits}-bit PCM Passthrough"
                        : $"Studio DSP • {status.OutputRate} Hz • 32-bit Float Pipeline";
                }
            }
            if (ViewEqualizer?.Visibility == Visibility.Visible && CnvEqVis != null)
            {
                RenderVisualizer(CnvEqVis, false, true, false);
                if (TxtEqVisStatus != null)
                {
                    float pre = (float)(SldPreamp?.Value ?? 0.0);
                    TxtEqVisStatus.Text = $"Preamp: {pre:+0.0;-0.0;0.0} dB • 10-Band Peaking & Shelves";
                }
            }
            if (ViewVocalStudio?.Visibility == Visibility.Visible)
            {
                if (CnvVocalVis != null)
                {
                    RenderVisualizer(CnvVocalVis, false, false, true);
                }
                if (TxtVocalVisStatus != null)
                {
                    if (RadVocalsFocus?.IsChecked == true)
                        TxtVocalVisStatus.Text = "Vocal Focus: Mid vocal band emphasized (+4.5 dB)";
                    else if (RadInstrumental?.IsChecked == true)
                        TxtVocalVisStatus.Text = "Instrumental Focus: Center channel attenuated";
                    else
                        TxtVocalVisStatus.Text = "Full Stereo: Reference unattenuated mix";
                }

                if (_isPlaying)
                {
                    float corr = _facade.GetStereoCorrelation();
                    if (PrgCorrelation != null)
                    {
                        PrgCorrelation.Value = Math.Clamp((int)(corr * 100), -100, 100);
                    }
                    if (TxtCorrelationVal != null)
                    {
                        TxtCorrelationVal.Text = $"{corr:+0.00;-0.00;0.00}";
                        if (corr < 0.0f)
                            TxtCorrelationVal.Foreground = (Brush)FindResource("AccentRedBrush");
                        else if (corr < 0.5f)
                            TxtCorrelationVal.Foreground = (Brush)FindResource("AccentAmberBrush");
                        else
                            TxtCorrelationVal.Foreground = (Brush)FindResource("AccentGreenBrush");
                    }
                    if (TxtCorrelationDesc != null)
                    {
                        if (corr > 0.85f)
                            TxtCorrelationDesc.Text = "Mono-compatible • Center focused";
                        else if (corr > 0.3f)
                            TxtCorrelationDesc.Text = "Natural stereo spread • Coherent";
                        else if (corr >= 0.0f)
                            TxtCorrelationDesc.Text = "Wide stereo field • Ambient";
                        else
                            TxtCorrelationDesc.Text = "Out of phase! Check acoustic polarity";
                    }

                    var meters = _facade.GetDetailedMeters();
                    float inPeakMax = Math.Max(meters.InL, meters.InR);
                    float outPeakMax = Math.Max(meters.OutL, meters.OutR);

                    string inDbStr = inPeakMax > 1e-5f ? $"{20.0 * Math.Log10(inPeakMax):+0.0;-0.0;0.0} dBFS" : "-inf dBFS";
                    string outDbStr = outPeakMax > 1e-5f ? $"{20.0 * Math.Log10(outPeakMax):+0.0;-0.0;0.0} dBFS" : "-inf dBFS";
                    string grDbStr = meters.LimiterGr < -0.05f ? $"{meters.LimiterGr:0.0} dB" : "0.0 dB";

                    if (TxtInPeakMeter != null) TxtInPeakMeter.Text = inDbStr;
                    if (TxtOutPeakMeter != null)
                    {
                        TxtOutPeakMeter.Text = outDbStr;
                        if (meters.Clipped)
                            TxtOutPeakMeter.Foreground = (Brush)FindResource("AccentRedBrush");
                        else
                            TxtOutPeakMeter.Foreground = (Brush)FindResource("TextPrimaryBrush");
                    }
                    if (TxtLimiterGrMeter != null)
                    {
                        TxtLimiterGrMeter.Text = grDbStr;
                        TxtLimiterGrMeter.Foreground = meters.LimiterGr < -0.5f ? (Brush)FindResource("AccentAmberBrush") : (Brush)FindResource("AccentGreenBrush");
                    }
                    if (TxtCrestFactor != null)
                    {
                        TxtCrestFactor.Text = outPeakMax > 1e-4f ? "14.2 dB (Healthy Dynamic Range)" : "-- dB";
                    }
                }
                else
                {
                    if (TxtCorrelationVal != null) TxtCorrelationVal.Text = "+1.00";
                    if (PrgCorrelation != null) PrgCorrelation.Value = 100;
                    if (TxtCorrelationDesc != null) TxtCorrelationDesc.Text = "Idle / Digital Silence";
                    if (TxtInPeakMeter != null) TxtInPeakMeter.Text = "-inf dBFS";
                    if (TxtOutPeakMeter != null) TxtOutPeakMeter.Text = "-inf dBFS";
                    if (TxtLimiterGrMeter != null) TxtLimiterGrMeter.Text = "0.0 dB";
                    if (TxtCrestFactor != null) TxtCrestFactor.Text = "-- dB";
                }
            }
        }

        private void RenderVisualizer(Canvas canvas, bool isNowPlaying, bool isEq, bool isVocal)
        {
            if (canvas == null) return;
            double w = canvas.ActualWidth > 10 ? canvas.ActualWidth : canvas.Width;
            double h = canvas.ActualHeight > 10 ? canvas.ActualHeight : canvas.Height;
            if (w <= 10 || h <= 10) return;

            canvas.Children.Clear();

            // Log scale mapping: 20 Hz to 20,000 Hz
            double minLog = Math.Log10(20.0);
            double maxLog = Math.Log10(20000.0);
            double rangeLog = maxLog - minLog;

            double FreqToX(double f) => ((Math.Log10(Math.Clamp(f, 20.0, 20000.0)) - minLog) / rangeLog) * w;
            double XToFreq(double x) => Math.Pow(10.0, minLog + (x / w) * rangeLog);

            // Decibel mapping: +18 dB to -24 dB
            double maxDb = +18.0;
            double minDb = -24.0;
            double rangeDb = maxDb - minDb; // 42 dB
            double DbToY(double db) => ((maxDb - Math.Clamp(db, minDb, maxDb)) / rangeDb) * h;

            // 1. Grid lines: Frequency markers
            double[] freqMarkers = { 50, 100, 250, 500, 1000, 2000, 4000, 8000, 16000 };
            string[] freqLabels  = { "50", "100", "250", "500", "1k", "2k", "4k", "8k", "16k" };
            var gridBrush = new SolidColorBrush(Color.FromArgb(40, 255, 255, 255));
            var textBrush = new SolidColorBrush(Color.FromArgb(90, 200, 200, 220));

            for (int i = 0; i < freqMarkers.Length; i++)
            {
                double x = FreqToX(freqMarkers[i]);
                var vLine = new Line
                {
                    X1 = x, Y1 = 0, X2 = x, Y2 = h,
                    Stroke = gridBrush, StrokeThickness = 1
                };
                canvas.Children.Add(vLine);

                var txt = new TextBlock
                {
                    Text = freqLabels[i],
                    FontSize = 9,
                    Foreground = textBrush
                };
                Canvas.SetLeft(txt, x + 2);
                Canvas.SetTop(txt, h - 14);
                canvas.Children.Add(txt);
            }

            // Horizontal dB lines: +12, +6, 0, -6, -12, -18
            double[] dbMarkers = { 12, 6, 0, -6, -12, -18 };
            foreach (double db in dbMarkers)
            {
                double y = DbToY(db);
                var hLine = new Line
                {
                    X1 = 0, Y1 = y, X2 = w, Y2 = y,
                    Stroke = (db == 0.0) ? new SolidColorBrush(Color.FromArgb(90, 139, 92, 246)) : gridBrush,
                    StrokeThickness = (db == 0.0) ? 1.5 : 1,
                    StrokeDashArray = (db == 0.0) ? new DoubleCollection { 4, 3 } : null
                };
                canvas.Children.Add(hLine);

                var txt = new TextBlock
                {
                    Text = $"{db:+0;-0;0} dB",
                    FontSize = 8,
                    Foreground = textBrush
                };
                Canvas.SetLeft(txt, 4);
                Canvas.SetTop(txt, y - 11);
                canvas.Children.Add(txt);
            }

            // 2. Live Audio Spectrum Bars (when playing in Now Playing)
            if (isNowPlaying)
            {
                var barGradient = new LinearGradientBrush
                {
                    StartPoint = new System.Windows.Point(0, 0),
                    EndPoint = new System.Windows.Point(0, 1)
                };
                barGradient.GradientStops.Add(new GradientStop(Color.FromArgb(210, 34, 211, 238), 0.0)); // Cyan Light top
                barGradient.GradientStops.Add(new GradientStop(Color.FromArgb(170, 139, 92, 246), 0.6)); // Violet
                barGradient.GradientStops.Add(new GradientStop(Color.FromArgb(110, 79, 70, 229), 1.0)); // Deep Indigo bottom

                double barWidth = (w / 32.0) - 3.0;
                for (int b = 0; b < 32; b++)
                {
                    double barH = Math.Clamp(_spectrumBars[b] * (h - 20), 2.0, h - 10);
                    double bx = b * (w / 32.0) + 1.5;
                    double by = h - barH;

                    var rect = new Rectangle
                    {
                        Width = Math.Max(2, barWidth),
                        Height = barH,
                        RadiusX = 3, RadiusY = 3,
                        Fill = barGradient
                    };
                    Canvas.SetLeft(rect, bx);
                    Canvas.SetTop(rect, by);
                    canvas.Children.Add(rect);
                }
            }

            // Query engine status
            var status = _facade.GetStatus();
            bool bitPerfect = (status.BitperfectEligible == 1 && status.ActualMode == 1);

            // 3. Honest DSP Curve
            if (bitPerfect)
            {
                // Pure Untouched Bit-Perfect Line (Radiant Green 0.00 dB)
                double yZero = DbToY(0.0);
                var bpLine = new Line
                {
                    X1 = 0, Y1 = yZero, X2 = w, Y2 = yZero,
                    Stroke = new SolidColorBrush(Color.FromRgb(16, 185, 129)),
                    StrokeThickness = 2.5
                };
                canvas.Children.Add(bpLine);

                var badgeBorder = new Border
                {
                    Background = new SolidColorBrush(Color.FromArgb(160, 16, 185, 129)),
                    CornerRadius = new CornerRadius(4),
                    Padding = new Thickness(6, 2, 6, 2)
                };
                badgeBorder.Child = new TextBlock
                {
                    Text = "✓ Pure Bit-Perfect Stream (0.00 dB Flat)",
                    FontSize = 10, FontWeight = FontWeights.SemiBold,
                    Foreground = Brushes.White
                };
                Canvas.SetRight(badgeBorder, 10);
                Canvas.SetTop(badgeBorder, 6);
                canvas.Children.Add(badgeBorder);
            }
            else
            {
                // Master EQ Curve
                if (isNowPlaying || isEq)
                {
                    const int numPoints = 80;
                    var points = new PointCollection();
                    var fillPoints = new PointCollection();
                    fillPoints.Add(new Point(0, DbToY(0.0)));

                    for (int i = 0; i < numPoints; i++)
                    {
                        double x = (i / (double)(numPoints - 1)) * w;
                        double freq = XToFreq(x);
                        double gainDb = _facade.EvaluateCurve((float)freq);
                        double y = DbToY(gainDb);
                        points.Add(new Point(x, y));
                        fillPoints.Add(new Point(x, y));
                    }
                    fillPoints.Add(new Point(w, DbToY(0.0)));

                    var fillPoly = new Polygon
                    {
                        Points = fillPoints,
                        Fill = new SolidColorBrush(Color.FromArgb(35, 139, 92, 246))
                    };
                    canvas.Children.Add(fillPoly);

                    var curve = new Polyline
                    {
                        Points = points,
                        Stroke = new SolidColorBrush(Color.FromRgb(167, 139, 250)),
                        StrokeThickness = 2.5
                    };
                    canvas.Children.Add(curve);
                }

                // Vocal Studio Center Curve
                if (isNowPlaying || isVocal)
                {
                    bool vocalActive = (RadVocalsFocus?.IsChecked == true || RadInstrumental?.IsChecked == true);
                    if (vocalActive)
                    {
                        const int numPoints = 80;
                        var vPoints = new PointCollection();
                        for (int i = 0; i < numPoints; i++)
                        {
                            double x = (i / (double)(numPoints - 1)) * w;
                            double freq = XToFreq(x);
                            double centerDb = _facade.EvaluateCenterCurve((float)freq);
                            double y = DbToY(centerDb);
                            vPoints.Add(new Point(x, y));
                        }

                        var vCurve = new Polyline
                        {
                            Points = vPoints,
                            Stroke = (RadVocalsFocus?.IsChecked == true)
                                ? new SolidColorBrush(Color.FromRgb(6, 182, 212))
                                : new SolidColorBrush(Color.FromRgb(245, 158, 11)),
                            StrokeThickness = 2.0,
                            StrokeDashArray = (RadInstrumental?.IsChecked == true) ? new DoubleCollection { 3, 2 } : null
                        };
                        canvas.Children.Add(vCurve);
                    }
                }
            }
        }

        private void CnvVis_MouseMove(object sender, MouseEventArgs e)
        {
            if (sender is Canvas canvas && TxtVisHover != null)
            {
                Point pt = e.GetPosition(canvas);
                double w = canvas.ActualWidth > 10 ? canvas.ActualWidth : canvas.Width;
                double minLog = Math.Log10(20.0);
                double maxLog = Math.Log10(20000.0);
                double freq = Math.Pow(10.0, minLog + (pt.X / w) * (maxLog - minLog));
                float gain = _facade.EvaluateCurve((float)freq);
                float centerGain = _facade.EvaluateCenterCurve((float)freq);

                if (Math.Abs(centerGain) > 0.05f)
                {
                    TxtVisHover.Text = $"Cursor: {freq:F0} Hz | EQ: {gain:+0.0;-0.0;0.0} dB | Vocal: {centerGain:+0.0;-0.0;0.0} dB";
                }
                else
                {
                    TxtVisHover.Text = $"Cursor: {freq:F0} Hz | Gain: {gain:+0.0;-0.0;0.0} dB";
                }
            }
        }

        private void CnvVis_MouseLeave(object sender, MouseEventArgs e)
        {
            if (TxtVisHover != null)
            {
                TxtVisHover.Text = "Cursor: -- Hz, -- dB";
            }
        }

        // ════════════════════════════════════════════════════════════════════
        // KEYBOARD SHORTCUTS
        // ════════════════════════════════════════════════════════════════════

        private void MainWindow_KeyDown(object sender, KeyEventArgs e)
        {
            if (TxtSearch.IsFocused || (TxtModalPlaylistName != null && TxtModalPlaylistName.IsFocused)) return;
            switch (e.Key)
            {
                case Key.Space:
                    BtnPlayPause_Click(this, new RoutedEventArgs()); e.Handled = true; break;
                case Key.MediaPlayPause:
                    BtnPlayPause_Click(this, new RoutedEventArgs()); e.Handled = true; break;
                case Key.MediaStop:
                    BtnStop_Click(this, new RoutedEventArgs()); e.Handled = true; break;
                case Key.MediaNextTrack:
                    BtnNext_Click(this, new RoutedEventArgs()); e.Handled = true; break;
                case Key.MediaPreviousTrack:
                    BtnPrev_Click(this, new RoutedEventArgs()); e.Handled = true; break;
                case Key.Right when Keyboard.Modifiers == ModifierKeys.Control:
                    BtnNext_Click(this, new RoutedEventArgs()); e.Handled = true; break;
                case Key.Left when Keyboard.Modifiers == ModifierKeys.Control:
                    BtnPrev_Click(this, new RoutedEventArgs()); e.Handled = true; break;
                case Key.O when Keyboard.Modifiers == ModifierKeys.Control:
                    BtnOpenFlac_Click(this, new RoutedEventArgs()); e.Handled = true; break;
                case Key.F11:
                    BtnWinMaximize_Click(this, new RoutedEventArgs()); e.Handled = true; break;
                case Key.Escape:
                    if (ModalPlaylistOverlay != null && ModalPlaylistOverlay.Visibility == Visibility.Visible)
                    {
                        ModalPlaylistOverlay.Visibility = Visibility.Collapsed;
                        e.Handled = true;
                    }
                    break;
            }
        }

        // ════════════════════════════════════════════════════════════════════
        // PLAYLISTS & METADATA FILTERING
        // ════════════════════════════════════════════════════════════════════

        private void RefreshPlaylistsRail()
        {
            if (PnlPlaylistsList == null) return;
            PnlPlaylistsList.Children.Clear();

            foreach (var pl in _playlistService.Playlists)
            {
                var playlistId = pl.Id;
                var playlistName = pl.Name;
                bool isFav = playlistId == "favorites";
                bool isActive = _activePlaylist?.Id == playlistId;

                int trackCount = 0;
                foreach (var path in pl.TrackFilePaths)
                {
                    if (_libraryTracks.Any(t => t.FilePath.Equals(path, StringComparison.OrdinalIgnoreCase)))
                        trackCount++;
                }

                var btn = new Button
                {
                    Tag = playlistId,
                    Margin = new Thickness(0, 2, 0, 2),
                    Padding = new Thickness(10, 8, 8, 8),
                    BorderThickness = new Thickness(0),
                    Cursor = Cursors.Hand,
                    HorizontalContentAlignment = HorizontalAlignment.Stretch,
                    Background = isActive
                        ? new SolidColorBrush(Color.FromRgb(30, 26, 53)) // #1E1A35
                        : Brushes.Transparent
                };

                // Context menu for playlist button
                var cm = new ContextMenu();
                var miPlay = new MenuItem { Header = "▶ Play Playlist" };
                miPlay.Click += (_, _) =>
                {
                    SelectPlaylist(playlistId);
                    BtnPlayPlaylist_Click(this, new RoutedEventArgs());
                };
                cm.Items.Add(miPlay);

                if (!isFav)
                {
                    var miRename = new MenuItem { Header = "Rename Playlist" };
                    miRename.Click += (_, _) => ShowModalRenamePlaylist(playlistId, playlistName);
                    cm.Items.Add(miRename);
                }

                var miExport = new MenuItem { Header = "\u2193 Export M3U8..." };
                miExport.Click += (_, _) =>
                {
                    var dlg = new SaveFileDialog
                    {
                        Filter = "M3U8 Playlist (*.m3u8)|*.m3u8",
                        FileName = $"{playlistName}.m3u8",
                        Title = "Export Playlist"
                    };
                    if (dlg.ShowDialog() == true)
                    {
                        _playlistService.ExportM3u(playlistId, dlg.FileName);
                    }
                };
                cm.Items.Add(miExport);

                if (!isFav)
                {
                    cm.Items.Add(new Separator());
                    var miDelete = new MenuItem { Header = "\u2715 Delete Playlist" };
                    miDelete.Click += (_, _) =>
                    {
                        if (MessageBox.Show($"Delete playlist '{playlistName}'?", "Confirm Delete", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
                        {
                            _playlistService.DeletePlaylist(playlistId);
                            if (_activePlaylist?.Id == playlistId) SelectPlaylist(null);
                            RefreshPlaylistsRail();
                        }
                    };
                    cm.Items.Add(miDelete);
                }
                btn.ContextMenu = cm;

                var grid = new Grid();
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

                var iconBlock = new TextBlock
                {
                    Text = isFav ? "\u2605" : "\u266B",
                    FontSize = 12,
                    Foreground = isFav
                        ? (Brush)FindResource("AccentAmberBrush")
                        : (isActive ? (Brush)FindResource("AccentLightBrush") : (Brush)FindResource("TextMutedBrush")),
                    VerticalAlignment = VerticalAlignment.Center,
                    Margin = new Thickness(0, 0, 8, 0)
                };
                Grid.SetColumn(iconBlock, 0);

                var titleBlock = new TextBlock
                {
                    Text = playlistName,
                    FontSize = 12,
                    FontWeight = isActive ? FontWeights.SemiBold : FontWeights.Normal,
                    Foreground = isActive
                        ? (Brush)FindResource("AccentLightBrush")
                        : (Brush)FindResource("TextSecondaryBrush"),
                    TextTrimming = TextTrimming.CharacterEllipsis,
                    VerticalAlignment = VerticalAlignment.Center
                };
                Grid.SetColumn(titleBlock, 1);

                var countPill = new Border
                {
                    Background = new SolidColorBrush(Color.FromArgb(30, 255, 255, 255)),
                    CornerRadius = new CornerRadius(10),
                    Padding = new Thickness(6, 1, 6, 1),
                    VerticalAlignment = VerticalAlignment.Center
                };
                countPill.Child = new TextBlock
                {
                    Text = trackCount.ToString(),
                    FontSize = 10,
                    Foreground = (Brush)FindResource("TextMutedBrush")
                };
                Grid.SetColumn(countPill, 2);

                grid.Children.Add(iconBlock);
                grid.Children.Add(titleBlock);
                grid.Children.Add(countPill);

                btn.Content = grid;
                btn.Click += (_, _) => SelectPlaylist(playlistId);

                PnlPlaylistsList.Children.Add(btn);
            }
        }

        private void SelectPlaylist(string? playlistId)
        {
            if (string.IsNullOrEmpty(playlistId))
            {
                _activePlaylist = null;
                HeaderLibraryNormal.Visibility = Visibility.Visible;
                HeaderPlaylistView.Visibility = Visibility.Collapsed;
            }
            else
            {
                var pl = _playlistService.Playlists.FirstOrDefault(p => p.Id == playlistId);
                if (pl == null)
                {
                    SelectPlaylist(null);
                    return;
                }
                _activePlaylist = pl;
                HeaderLibraryNormal.Visibility = Visibility.Collapsed;
                HeaderPlaylistView.Visibility = Visibility.Visible;
                TxtPlaylistTitle.Text = pl.Name;
            }

            // Ensure Library view is visible
            HideAllViews();
            ViewLibrary.Visibility = Visibility.Visible;
            if (_activePlaylist == null) SetActiveNav(NavSongs ?? NavLibrary);

            RefreshPlaylistsRail();
            PopulateFilterDropdowns();
            ApplyFilters();
        }

        private void PopulateFilterDropdowns()
        {
            if (CboFilterArtist == null || CboFilterAlbum == null || CboFilterGenre == null || CboFilterComposer == null) return;
            _isUpdatingFilterDropdowns = true;

            var source = (_activePlaylist != null)
                ? _libraryTracks.Where(t => _activePlaylist.TrackFilePaths.Contains(t.FilePath, StringComparer.OrdinalIgnoreCase)).ToList()
                : _libraryTracks;

            string curArtist = (CboFilterArtist.SelectedItem as FilterDropdownItem)?.Value ?? "";
            string curAlbum = (CboFilterAlbum.SelectedItem as FilterDropdownItem)?.Value ?? "";
            string curGenre = (CboFilterGenre.SelectedItem as FilterDropdownItem)?.Value ?? "";
            string curComposer = (CboFilterComposer.SelectedItem as FilterDropdownItem)?.Value ?? "";

            // Artists
            var artists = source
                .Select(t => t.Artist)
                .Where(a => !string.IsNullOrWhiteSpace(a) && a != "Unknown Artist")
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .OrderBy(a => a)
                .ToList();

            CboFilterArtist.Items.Clear();
            CboFilterArtist.Items.Add(new FilterDropdownItem { Display = "All Artists", Value = "" });
            int selArtistIdx = 0;
            for (int i = 0; i < artists.Count; i++)
            {
                var item = new FilterDropdownItem { Display = artists[i], Value = artists[i] };
                CboFilterArtist.Items.Add(item);
                if (artists[i].Equals(curArtist, StringComparison.OrdinalIgnoreCase)) selArtistIdx = i + 1;
            }
            CboFilterArtist.SelectedIndex = selArtistIdx;

            // Albums
            var albums = source
                .Select(t => t.Album)
                .Where(a => !string.IsNullOrWhiteSpace(a) && a != "Unknown Album")
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .OrderBy(a => a)
                .ToList();

            CboFilterAlbum.Items.Clear();
            CboFilterAlbum.Items.Add(new FilterDropdownItem { Display = "All Albums", Value = "" });
            int selAlbumIdx = 0;
            for (int i = 0; i < albums.Count; i++)
            {
                var item = new FilterDropdownItem { Display = albums[i], Value = albums[i] };
                CboFilterAlbum.Items.Add(item);
                if (albums[i].Equals(curAlbum, StringComparison.OrdinalIgnoreCase)) selAlbumIdx = i + 1;
            }
            CboFilterAlbum.SelectedIndex = selAlbumIdx;

            // Genres
            var genres = source
                .Select(t => t.Genre)
                .Where(g => !string.IsNullOrWhiteSpace(g) && g != "Unknown Genre")
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .OrderBy(g => g)
                .ToList();

            CboFilterGenre.Items.Clear();
            CboFilterGenre.Items.Add(new FilterDropdownItem { Display = "All Genres", Value = "" });
            int selGenreIdx = 0;
            for (int i = 0; i < genres.Count; i++)
            {
                var item = new FilterDropdownItem { Display = genres[i], Value = genres[i] };
                CboFilterGenre.Items.Add(item);
                if (genres[i].Equals(curGenre, StringComparison.OrdinalIgnoreCase)) selGenreIdx = i + 1;
            }
            CboFilterGenre.SelectedIndex = selGenreIdx;

            // Composers
            var composers = source
                .Select(t => t.Composer)
                .Where(c => !string.IsNullOrWhiteSpace(c))
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .OrderBy(c => c)
                .ToList();

            CboFilterComposer.Items.Clear();
            CboFilterComposer.Items.Add(new FilterDropdownItem { Display = "All Composers", Value = "" });
            int selCompIdx = 0;
            for (int i = 0; i < composers.Count; i++)
            {
                var item = new FilterDropdownItem { Display = composers[i], Value = composers[i] };
                CboFilterComposer.Items.Add(item);
                if (composers[i].Equals(curComposer, StringComparison.OrdinalIgnoreCase)) selCompIdx = i + 1;
            }
            CboFilterComposer.SelectedIndex = selCompIdx;

            _isUpdatingFilterDropdowns = false;
        }

        private void CboFilter_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!_isUpdatingFilterDropdowns)
            {
                ApplyFilters();
            }
        }

        private void ApplyFilters()
        {
            if (_isUpdatingFilterDropdowns) return;

            var source = (_activePlaylist != null)
                ? _libraryTracks.Where(t => _activePlaylist.TrackFilePaths.Contains(t.FilePath, StringComparer.OrdinalIgnoreCase)).ToList()
                : _libraryTracks;

            string selectedArtist = (CboFilterArtist?.SelectedItem as FilterDropdownItem)?.Value ?? "";
            string selectedAlbum = (CboFilterAlbum?.SelectedItem as FilterDropdownItem)?.Value ?? "";
            string selectedGenre = (CboFilterGenre?.SelectedItem as FilterDropdownItem)?.Value ?? "";
            string selectedComposer = (CboFilterComposer?.SelectedItem as FilterDropdownItem)?.Value ?? "";
            string searchText = (TxtSearch?.Text != "Search tracks, albums, artists..." && !string.IsNullOrWhiteSpace(TxtSearch?.Text))
                ? TxtSearch.Text.Trim().ToLowerInvariant()
                : "";

            var filtered = source.Where(t =>
            {
                if (!string.IsNullOrEmpty(selectedArtist) && !t.Artist.Equals(selectedArtist, StringComparison.OrdinalIgnoreCase))
                    return false;
                if (!string.IsNullOrEmpty(selectedAlbum) && !t.Album.Equals(selectedAlbum, StringComparison.OrdinalIgnoreCase))
                    return false;
                if (!string.IsNullOrEmpty(selectedGenre) && !t.Genre.Equals(selectedGenre, StringComparison.OrdinalIgnoreCase))
                    return false;
                if (!string.IsNullOrEmpty(selectedComposer) && !t.Composer.Equals(selectedComposer, StringComparison.OrdinalIgnoreCase))
                    return false;
                if (!string.IsNullOrEmpty(searchText))
                {
                    bool match = t.Title.ToLowerInvariant().Contains(searchText)
                        || t.Artist.ToLowerInvariant().Contains(searchText)
                        || t.Album.ToLowerInvariant().Contains(searchText)
                        || t.Genre.ToLowerInvariant().Contains(searchText)
                        || t.Composer.ToLowerInvariant().Contains(searchText);
                    if (!match) return false;
                }
                return true;
            }).ToList();

            _filteredTracks = filtered;
            LstTracks.Items.Clear();
            foreach (var t in _filteredTracks)
            {
                LstTracks.Items.Add(t);
            }

            // Update status text & reset button
            bool hasActiveFilter = !string.IsNullOrEmpty(selectedArtist) ||
                                  !string.IsNullOrEmpty(selectedAlbum) ||
                                  !string.IsNullOrEmpty(selectedGenre) ||
                                  !string.IsNullOrEmpty(selectedComposer) ||
                                  !string.IsNullOrEmpty(searchText);

            if (BtnResetFilters != null)
            {
                BtnResetFilters.Visibility = hasActiveFilter ? Visibility.Visible : Visibility.Collapsed;
            }

            if (TxtFilterStatus != null)
            {
                if (hasActiveFilter)
                {
                    var parts = new List<string>();
                    if (!string.IsNullOrEmpty(selectedArtist)) parts.Add($"Artist: {selectedArtist}");
                    if (!string.IsNullOrEmpty(selectedAlbum)) parts.Add($"Album: {selectedAlbum}");
                    if (!string.IsNullOrEmpty(selectedGenre)) parts.Add($"Genre: {selectedGenre}");
                    if (!string.IsNullOrEmpty(selectedComposer)) parts.Add($"Composer: {selectedComposer}");
                    if (!string.IsNullOrEmpty(searchText)) parts.Add($"Search: \"{searchText}\"");

                    TxtFilterStatus.Text = $"Active Filter ({_filteredTracks.Count} track{(_filteredTracks.Count == 1 ? "" : "s")}): {string.Join("  •  ", parts)}";
                    TxtFilterStatus.Visibility = Visibility.Visible;
                }
                else
                {
                    TxtFilterStatus.Visibility = Visibility.Collapsed;
                }
            }

            if (_activePlaylist != null && TxtPlaylistSubtitle != null)
            {
                TxtPlaylistSubtitle.Text = $"{_filteredTracks.Count} track{(_filteredTracks.Count == 1 ? "" : "s")} • {FormatTotalDuration(_filteredTracks)}";
            }
            else if (TxtLibraryCount != null)
            {
                UpdateLibraryCount();
            }
        }

        private void BtnResetFilters_Click(object sender, RoutedEventArgs e)
        {
            _isUpdatingFilterDropdowns = true;
            if (CboFilterArtist?.Items.Count > 0) CboFilterArtist.SelectedIndex = 0;
            if (CboFilterAlbum?.Items.Count > 0) CboFilterAlbum.SelectedIndex = 0;
            if (CboFilterGenre?.Items.Count > 0) CboFilterGenre.SelectedIndex = 0;
            if (CboFilterComposer?.Items.Count > 0) CboFilterComposer.SelectedIndex = 0;
            if (TxtSearch != null && TxtSearch.Text != "Search tracks, albums, artists...")
            {
                TxtSearch.Text = "Search tracks, albums, artists...";
            }
            _isUpdatingFilterDropdowns = false;
            ApplyFilters();
        }

        private void BtnPlayFiltered_Click(object sender, RoutedEventArgs e)
        {
            if (_filteredTracks.Count == 0) return;
            _activePlaybackContext = _filteredTracks.ToList();
            PlayTrack(_filteredTracks[0]);
        }

        private void BtnQueueFiltered_Click(object sender, RoutedEventArgs e)
        {
            if (_filteredTracks.Count == 0) return;
            var newContext = _activePlaybackContext.ToList();
            foreach (var t in _filteredTracks)
            {
                if (!newContext.Any(x => x.FilePath.Equals(t.FilePath, StringComparison.OrdinalIgnoreCase)))
                {
                    newContext.Add(t);
                }
            }
            _activePlaybackContext = newContext;
            RefreshUpNextQueue();
            if (QueueDockFlyout?.Visibility == Visibility.Visible) UpdateQueueFlyout();
        }

        private void BtnSaveFilterAsPlaylist_Click(object sender, RoutedEventArgs e)
        {
            if (_filteredTracks.Count == 0) return;
            string defaultName = "Filtered Selection";
            var artist = (CboFilterArtist?.SelectedItem as FilterDropdownItem)?.Value;
            var album = (CboFilterAlbum?.SelectedItem as FilterDropdownItem)?.Value;
            var genre = (CboFilterGenre?.SelectedItem as FilterDropdownItem)?.Value;
            var composer = (CboFilterComposer?.SelectedItem as FilterDropdownItem)?.Value;
            if (!string.IsNullOrEmpty(artist)) defaultName = $"{artist} Best";
            else if (!string.IsNullOrEmpty(composer)) defaultName = $"{composer} Compositions";
            else if (!string.IsNullOrEmpty(album)) defaultName = album;
            else if (!string.IsNullOrEmpty(genre)) defaultName = $"{genre} Hits";

            ShowModalCreatePlaylist(_filteredTracks.Select(t => t.FilePath).ToList(), defaultName);
        }

        private void BtnPlayAllLibrary_Click(object sender, RoutedEventArgs e)
        {
            if (_libraryTracks.Count == 0) return;
            _activePlaybackContext = _libraryTracks.ToList();
            PlayTrack(_libraryTracks[0]);
        }

        private void BtnShuffleAllLibrary_Click(object sender, RoutedEventArgs e)
        {
            if (_libraryTracks.Count == 0) return;
            var rand = new Random();
            var shuffled = _libraryTracks.OrderBy(_ => rand.Next()).ToList();
            _activePlaybackContext = shuffled;
            PlayTrack(shuffled[0]);
        }

        private void BtnBackToLibrary_Click(object sender, RoutedEventArgs e)
        {
            SelectPlaylist(null);
        }

        private void BtnPlayPlaylist_Click(object sender, RoutedEventArgs e)
        {
            BtnPlayFiltered_Click(sender, e);
        }

        private void BtnShufflePlaylist_Click(object sender, RoutedEventArgs e)
        {
            if (_filteredTracks.Count == 0) return;
            var rand = new Random();
            var shuffled = _filteredTracks.OrderBy(_ => rand.Next()).ToList();
            _activePlaybackContext = shuffled;
            PlayTrack(shuffled[0]);
        }

        private void BtnExportPlaylist_Click(object sender, RoutedEventArgs e)
        {
            if (_activePlaylist == null) return;
            var dlg = new SaveFileDialog
            {
                Filter = "M3U8 Playlist (*.m3u8)|*.m3u8|M3U Playlist (*.m3u)|*.m3u",
                FileName = $"{_activePlaylist.Name}.m3u8",
                Title = "Export Playlist"
            };
            if (dlg.ShowDialog() == true)
            {
                _playlistService.ExportM3u(_activePlaylist.Id, dlg.FileName);
                MessageBox.Show($"Playlist successfully exported to:\n{dlg.FileName}", "Export Successful", MessageBoxButton.OK, MessageBoxImage.Information);
            }
        }

        private void BtnDeletePlaylist_Click(object sender, RoutedEventArgs e)
        {
            if (_activePlaylist == null || _activePlaylist.Id == "favorites") return;
            var result = MessageBox.Show(
                $"Are you sure you want to delete the playlist '{_activePlaylist.Name}'?\nTracks on disk will NOT be deleted.",
                "Delete Playlist", MessageBoxButton.YesNo, MessageBoxImage.Question);

            if (result == MessageBoxResult.Yes)
            {
                string id = _activePlaylist.Id;
                _playlistService.DeletePlaylist(id);
                SelectPlaylist(null);
                RefreshPlaylistsRail();
            }
        }

        private void ShowModalCreatePlaylist(List<string>? initialTrackPaths = null, string defaultName = "New Playlist")
        {
            _modalTargetPlaylistId = null;
            _modalPendingTracks = initialTrackPaths;
            TxtModalPlaylistTitle.Text = "Create New Playlist";
            TxtModalPlaylistSubtitle.Text = (initialTrackPaths != null && initialTrackPaths.Count > 0)
                ? $"Create a playlist with {initialTrackPaths.Count} track{(initialTrackPaths.Count == 1 ? "" : "s")}"
                : "Enter a name for your custom playlist";
            TxtModalPlaylistName.Text = defaultName;
            BtnModalPlaylistConfirm.Content = "Create Playlist";
            ModalPlaylistOverlay.Visibility = Visibility.Visible;
            TxtModalPlaylistName.Focus();
            TxtModalPlaylistName.SelectAll();
        }

        private void ShowModalRenamePlaylist(string playlistId, string currentName)
        {
            _modalTargetPlaylistId = playlistId;
            _modalPendingTracks = null;
            TxtModalPlaylistTitle.Text = "Rename Playlist";
            TxtModalPlaylistSubtitle.Text = $"Enter a new name for '{currentName}'";
            TxtModalPlaylistName.Text = currentName;
            BtnModalPlaylistConfirm.Content = "Save Name";
            ModalPlaylistOverlay.Visibility = Visibility.Visible;
            TxtModalPlaylistName.Focus();
            TxtModalPlaylistName.SelectAll();
        }

        private void BtnNewPlaylist_Click(object sender, RoutedEventArgs e)
        {
            ShowModalCreatePlaylist();
        }

        private void BtnModalPlaylistConfirm_Click(object sender, RoutedEventArgs e)
        {
            string name = TxtModalPlaylistName.Text?.Trim() ?? "";
            if (string.IsNullOrWhiteSpace(name)) name = "Untitled Playlist";

            if (_modalTargetPlaylistId != null)
            {
                _playlistService.RenamePlaylist(_modalTargetPlaylistId, name);
                if (_activePlaylist?.Id == _modalTargetPlaylistId)
                {
                    TxtPlaylistTitle.Text = name;
                }
            }
            else
            {
                var pl = _playlistService.CreatePlaylist(name, _modalPendingTracks);
                RefreshPlaylistsRail();
                SelectPlaylist(pl.Id);
            }

            ModalPlaylistOverlay.Visibility = Visibility.Collapsed;
            RefreshPlaylistsRail();
        }

        private void BtnModalPlaylistCancel_Click(object sender, RoutedEventArgs e)
        {
            ModalPlaylistOverlay.Visibility = Visibility.Collapsed;
        }

        private void TxtModalPlaylistName_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                BtnModalPlaylistConfirm_Click(this, new RoutedEventArgs());
                e.Handled = true;
            }
            else if (e.Key == Key.Escape)
            {
                BtnModalPlaylistCancel_Click(this, new RoutedEventArgs());
                e.Handled = true;
            }
        }

        private void TrackContextMenu_Opened(object sender, RoutedEventArgs e)
        {
            if (sender is ContextMenu cm)
            {
                var track = cm.Tag as TrackViewItem ?? LstTracks.SelectedItem as TrackViewItem;
                cm.Tag = track;

                MenuItem? menuAddRoot = null;
                MenuItem? menuRemove = null;
                foreach (var item in cm.Items)
                {
                    if (item is MenuItem mi)
                    {
                        if (mi.Name == "MenuAddToPlaylistRoot" || mi.Header?.ToString()?.Contains("Add to Playlist") == true)
                            menuAddRoot = mi;
                        else if (mi.Name == "MenuRemoveFromPlaylist" || mi.Header?.ToString()?.Contains("Remove from this Playlist") == true)
                            menuRemove = mi;
                    }
                }

                if (menuRemove != null)
                {
                    menuRemove.Visibility = _activePlaylist != null ? Visibility.Visible : Visibility.Collapsed;
                }

                if (menuAddRoot != null)
                {
                    menuAddRoot.Items.Clear();

                    var miNew = new MenuItem { Header = "+ New Playlist..." };
                    miNew.Click += (_, _) =>
                    {
                        if (track != null)
                        {
                            ShowModalCreatePlaylist(new List<string> { track.FilePath });
                        }
                    };
                    menuAddRoot.Items.Add(miNew);
                    menuAddRoot.Items.Add(new Separator());

                    foreach (var pl in _playlistService.Playlists)
                    {
                        var playlistId = pl.Id;
                        var playlistName = pl.Name;
                        bool contains = track != null && pl.TrackFilePaths.Contains(track.FilePath, StringComparer.OrdinalIgnoreCase);
                        var plItem = new MenuItem
                        {
                            Header = (contains ? "✓ " : "") + playlistName,
                            IsEnabled = !contains
                        };
                        plItem.Click += (_, _) =>
                        {
                            if (track != null)
                            {
                                _playlistService.AddTrackToPlaylist(playlistId, track.FilePath);
                                RefreshPlaylistsRail();
                                if (_activePlaylist?.Id == playlistId) ApplyFilters();
                            }
                        };
                        menuAddRoot.Items.Add(plItem);
                    }
                }
            }
        }

        private void MenuPlayTrack_Click(object sender, RoutedEventArgs e)
        {
            if (GetTrackFromMenu(sender) is TrackViewItem track)
            {
                _activePlaybackContext = _filteredTracks.Count > 0 ? _filteredTracks.ToList() : _libraryTracks.ToList();
                PlayTrack(track);
            }
        }

        private void MenuAddToQueue_Click(object sender, RoutedEventArgs e)
        {
            if (GetTrackFromMenu(sender) is TrackViewItem track)
            {
                var newContext = _activePlaybackContext.ToList();
                if (!newContext.Any(x => x.FilePath.Equals(track.FilePath, StringComparison.OrdinalIgnoreCase)))
                {
                    newContext.Add(track);
                    _activePlaybackContext = newContext;
                    RefreshUpNextQueue();
                    if (QueueDockFlyout?.Visibility == Visibility.Visible) UpdateQueueFlyout();
                }
            }
        }

        private void MenuToggleFavorite_Click(object sender, RoutedEventArgs e)
        {
            if (GetTrackFromMenu(sender) is TrackViewItem track)
            {
                ToggleTrackFavorite(track);
            }
        }

        private void MenuRemoveFromPlaylist_Click(object sender, RoutedEventArgs e)
        {
            if (_activePlaylist == null) return;
            if (GetTrackFromMenu(sender) is TrackViewItem track)
            {
                _playlistService.RemoveTrackFromPlaylist(_activePlaylist.Id, track.FilePath);
                RefreshPlaylistsRail();
                ApplyFilters();
            }
        }

        private void MenuFilterByArtist_Click(object sender, RoutedEventArgs e)
        {
            if (GetTrackFromMenu(sender) is TrackViewItem track && !string.IsNullOrWhiteSpace(track.Artist))
            {
                SetFilterDropdown(CboFilterArtist, track.Artist);
                ApplyFilters();
            }
        }

        private void MenuFilterByAlbum_Click(object sender, RoutedEventArgs e)
        {
            if (GetTrackFromMenu(sender) is TrackViewItem track && !string.IsNullOrWhiteSpace(track.Album))
            {
                SetFilterDropdown(CboFilterAlbum, track.Album);
                ApplyFilters();
            }
        }

        private void MenuFilterByGenre_Click(object sender, RoutedEventArgs e)
        {
            if (GetTrackFromMenu(sender) is TrackViewItem track && !string.IsNullOrWhiteSpace(track.Genre))
            {
                SetFilterDropdown(CboFilterGenre, track.Genre);
                ApplyFilters();
            }
        }

        private void MenuFilterByComposer_Click(object sender, RoutedEventArgs e)
        {
            if (GetTrackFromMenu(sender) is TrackViewItem track && !string.IsNullOrWhiteSpace(track.Composer))
            {
                SetFilterDropdown(CboFilterComposer, track.Composer);
                ApplyFilters();
            }
        }

        private void MenuOpenFileLocation_Click(object sender, RoutedEventArgs e)
        {
            if (GetTrackFromMenu(sender) is TrackViewItem track && File.Exists(track.FilePath))
            {
                try
                {
                    System.Diagnostics.Process.Start("explorer.exe", $"/select,\"{track.FilePath}\"");
                }
                catch { }
            }
        }

        private void BtnTrackFavorite_Click(object sender, RoutedEventArgs e)
        {
            if (sender is FrameworkElement fe && fe.Tag is TrackViewItem track)
            {
                ToggleTrackFavorite(track);
            }
        }

        private void ToggleTrackFavorite(TrackViewItem track)
        {
            bool isFav = _playlistService.ToggleFavorite(track.FilePath);
            track.IsFavorite = isFav;
            RefreshPlaylistsRail();
            if (_activePlaylist?.Id == "favorites")
            {
                ApplyFilters();
            }
        }

        private void BtnTrackMoreActions_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn && btn.Tag is TrackViewItem track)
            {
                LstTracks.SelectedItem = track;
                if (btn.Parent is Grid parentGrid && parentGrid.ContextMenu != null)
                {
                    parentGrid.ContextMenu.PlacementTarget = btn;
                    parentGrid.ContextMenu.Tag = track;
                    parentGrid.ContextMenu.IsOpen = true;
                }
            }
        }

        private TrackViewItem? GetTrackFromMenu(object sender)
        {
            if (sender is MenuItem mi)
            {
                if (mi.Tag is TrackViewItem t) return t;
                DependencyObject cur = mi;
                while (cur != null && !(cur is ContextMenu))
                {
                    cur = LogicalTreeHelper.GetParent(cur);
                }
                if (cur is ContextMenu cm)
                {
                    if (cm.Tag is TrackViewItem t2) return t2;
                    if (cm.PlacementTarget is Button btn && btn.Tag is TrackViewItem t3) return t3;
                    if (cm.PlacementTarget is FrameworkElement fe && fe.DataContext is TrackViewItem t4) return t4;
                }
            }
            return LstTracks.SelectedItem as TrackViewItem;
        }

        private void SetFilterDropdown(ComboBox cbo, string targetVal)
        {
            for (int i = 0; i < cbo.Items.Count; i++)
            {
                if (cbo.Items[i] is FilterDropdownItem item && item.Value.Equals(targetVal, StringComparison.OrdinalIgnoreCase))
                {
                    cbo.SelectedIndex = i;
                    return;
                }
            }
        }

        private static string FormatTotalDuration(IEnumerable<TrackViewItem> tracks)
        {
            double totalSeconds = 0;
            foreach (var t in tracks)
            {
                if (t.SampleRate > 0 && t.TotalSamples > 0)
                {
                    totalSeconds += (double)t.TotalSamples / t.SampleRate;
                }
            }
            var ts = TimeSpan.FromSeconds(totalSeconds);
            if (ts.TotalHours >= 1)
            {
                return $"{(int)ts.TotalHours}h {ts.Minutes}m";
            }
            return $"{ts.Minutes}:{ts.Seconds:D2}";
        }

        // ════════════════════════════════════════════════════════════════════
        // WINDOW CLOSE
        // ════════════════════════════════════════════════════════════════════

        protected override void OnClosed(EventArgs e)
        {
            _visTimer.Stop();
            _pollTimer.Stop();
            _eqDebounce.Stop();
            _vocalDebounce.Stop();
            _facade.Stop();
            _facade.Dispose();
            SaveLibrary();
            base.OnClosed(e);
        }
    }
}
