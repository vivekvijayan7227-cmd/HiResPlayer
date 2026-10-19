// Copyright (c) HiResPlayer contributors. Focused keyboard handling partial class.
// Place in HiResPlayer/src/HiResPlayer.Desktop/MainWindow.Keyboard.cs
// Scope: Focus-aware keyboard input when the main WPF window receives focus.

using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Media3D;

namespace HiResPlayer.Desktop
{
    public partial class MainWindow
    {
        private const string SearchHint = "Search tracks, albums, artists...";

        private void InstallPlaybackKeyboardShortcuts()
        {
            // Tunneling is intentional: children may mark bubbling KeyDown handled.
            PreviewKeyDown += MainWindow_PreviewPlaybackKeyDown;

            // Native Slider keyboard behavior is preserved when volume has focus.
            if (SldVolume != null)
            {
                SldVolume.SmallChange = 5;
                SldVolume.LargeChange = 10;
                SldVolume.ToolTip = "Output Device Master Volume (Ctrl+Up / Ctrl+Down: 5%)";
            }

            if (BtnPlayPause != null)
            {
                BtnPlayPause.ToolTip = "Play / Pause (Space, Ctrl+P)";
            }
            if (BtnNext != null)
            {
                BtnNext.ToolTip = "Next track (Ctrl+Right)";
            }
            if (BtnPrev != null)
            {
                BtnPrev.ToolTip = "Previous track (Ctrl+Left)";
            }
            if (SldPosition != null)
            {
                SldPosition.ToolTip = "Seek (focus slider, then Left/Right: 5 s; PgUp/PgDn: 30 s; Home/End: start/end)";
            }
        }

        // Avoid assuming every focused object is a WPF visual (ContentElement
        // instances and items within templates may be logical children).
        private static T? FindAncestorOrSelf<T>(DependencyObject? current)
            where T : DependencyObject
        {
            while (current != null)
            {
                if (current is T match) return match;
                if (current is Visual || current is Visual3D)
                    current = VisualTreeHelper.GetParent(current);
                else
                    current = LogicalTreeHelper.GetParent(current);
            }
            return null;
        }

        private void MainWindow_PreviewPlaybackKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Handled) return;

            Key key = e.Key == Key.System ? e.SystemKey : e.Key;
            ModifierKeys mods = Keyboard.Modifiers;
            DependencyObject? focused = Keyboard.FocusedElement as DependencyObject
                                        ?? e.OriginalSource as DependencyObject;

            // Never steal text editing, including search, playlist name, or future editor fields.
            bool isTextEntry = FindAncestorOrSelf<TextBoxBase>(focused) != null
                            || FindAncestorOrSelf<PasswordBox>(focused) != null
                            || (FindAncestorOrSelf<ComboBox>(focused) is { IsEditable: true });

            // System media-key processing here works only if WPF actually receives
            // the event; background headset keys require separate Windows SMTC integration.
            if (mods == ModifierKeys.None)
            {
                Action? mediaAction = key switch
                {
                    Key.MediaPlayPause => () => BtnPlayPause_Click(this, new RoutedEventArgs()),
                    Key.MediaStop => () => BtnStop_Click(this, new RoutedEventArgs()),
                    Key.MediaNextTrack => () => BtnNext_Click(this, new RoutedEventArgs()),
                    Key.MediaPreviousTrack => () => BtnPrev_Click(this, new RoutedEventArgs()),
                    _ => null
                };
                if (mediaAction != null)
                {
                    if (!e.IsRepeat) mediaAction();
                    e.Handled = true;
                    return;
                }
            }

            // A modal is modal for keyboard shortcuts as well as for the mouse.
            if (ModalPlaylistOverlay?.Visibility == Visibility.Visible)
            {
                if (key == Key.Escape && mods == ModifierKeys.None)
                {
                    BtnModalPlaylistCancel_Click(this, new RoutedEventArgs());
                    if (BtnNewPlaylist != null && BtnNewPlaylist.IsVisible) BtnNewPlaylist.Focus();
                    e.Handled = true;
                }
                return;
            }

            if (key == Key.Escape && mods == ModifierKeys.None)
            {
                if (TxtSearch?.IsKeyboardFocusWithin == true)
                {
                    TxtSearch.Text = string.Empty;
                    Keyboard.ClearFocus(); // Existing LostFocus restores the hint.
                    e.Handled = true;
                    return;
                }

                if ((BorderQueueRail != null && BorderQueueRail.Visibility == Visibility.Visible) ||
                    (QueueDockFlyout != null && QueueDockFlyout.Visibility == Visibility.Visible))
                {
                    if (BorderQueueRail != null) BorderQueueRail.Visibility = Visibility.Collapsed;
                    if (ColQueueRail != null) ColQueueRail.Width = new GridLength(0);
                    if (QueueDockFlyout != null) QueueDockFlyout.Visibility = Visibility.Collapsed;
                    BtnDockNowPlaying?.Focus();
                    e.Handled = true;
                    return;
                }

                if (TxtSearch != null && !string.IsNullOrWhiteSpace(TxtSearch.Text)
                                      && TxtSearch.Text != SearchHint)
                {
                    TxtSearch.Text = string.Empty;
                    TxtSearch.Text = SearchHint;
                    Keyboard.ClearFocus();
                    e.Handled = true;
                }
                return;
            }

            if (isTextEntry) return;

            // Seek keys when the seek control has focus. Do not hijack unrelated
            // list navigation or the volume slider's native arrow handling.
            if (mods == ModifierKeys.None && SldPosition?.IsKeyboardFocusWithin == true &&
                (key == Key.Left || key == Key.Right || key == Key.Home ||
                 key == Key.End || key == Key.PageUp || key == Key.PageDown))
            {
                SeekFromKeyboard(key);
                e.Handled = true;
                return;
            }

            if (mods == ModifierKeys.Control)
            {
                // Repeats allowed only for volume adjustment.
                switch (key)
                {
                    case Key.Up:
                        if (SldVolume != null) SldVolume.Value = Math.Min(100, SldVolume.Value + 5);
                        e.Handled = true;
                        return;
                    case Key.Down:
                        if (SldVolume != null) SldVolume.Value = Math.Max(0, SldVolume.Value - 5);
                        e.Handled = true;
                        return;
                    case Key.Right:
                        if (!e.IsRepeat) BtnNext_Click(this, new RoutedEventArgs());
                        e.Handled = true;
                        return;
                    case Key.Left:
                        if (!e.IsRepeat) BtnPrev_Click(this, new RoutedEventArgs());
                        e.Handled = true;
                        return;
                    case Key.O:
                        if (!e.IsRepeat) BtnOpenFlac_Click(this, new RoutedEventArgs());
                        e.Handled = true;
                        return;
                    case Key.Space:
                    case Key.P:
                        if (!e.IsRepeat) BtnPlayPause_Click(this, new RoutedEventArgs());
                        e.Handled = true;
                        return;
                }
                return;
            }

            // Alt/Shift/Win chords must remain available to accessibility tools
            // and the OS unless specifically defined by the application.
            if (mods != ModifierKeys.None) return;

            // CheckBox and RadioButton use Space for native toggle when explicitly focused
            bool isToggleControl = FindAncestorOrSelf<CheckBox>(focused) != null
                                || FindAncestorOrSelf<RadioButton>(focused) != null;

            switch (key)
            {
                case Key.Space when !isToggleControl:
                    if (!e.IsRepeat) BtnPlayPause_Click(this, new RoutedEventArgs());
                    e.Handled = true;
                    return;
                case Key.F11:
                    if (!e.IsRepeat) BtnWinMaximize_Click(this, new RoutedEventArgs());
                    e.Handled = true;
                    return;
                case Key.M:
                    if (!e.IsRepeat) BtnVolumeMute_Click(this, new RoutedEventArgs());
                    e.Handled = true;
                    return;
                case Key.F:
                    if (!e.IsRepeat) ToggleCurrentOrSelectedFavorite();
                    e.Handled = true;
                    return;
                case Key.OemQuestion:
                case Key.Divide:
                    if (!e.IsRepeat && TxtSearch != null)
                    {
                        HideAllViews();
                        ViewLibrary.Visibility = Visibility.Visible;
                        SetActiveNav(NavSongs ?? NavLibrary);
                        TxtSearch.Focus();
                        if (TxtSearch.Text == SearchHint) TxtSearch.Clear();
                        TxtSearch.SelectAll();
                    }
                    e.Handled = true;
                    return;
            }

            // Bare Up/Down work only when they don't conflict with native
            // keyboard navigation inside buttons, selectors, combos or sliders.
            bool hasDirectionalWidget = FindAncestorOrSelf<ButtonBase>(focused) != null
                                     || FindAncestorOrSelf<Selector>(focused) != null
                                     || FindAncestorOrSelf<ComboBox>(focused) != null
                                     || FindAncestorOrSelf<Slider>(focused) != null;
            if (!hasDirectionalWidget && SldVolume != null)
            {
                if (key == Key.Up)
                {
                    SldVolume.Value = Math.Min(100, SldVolume.Value + 5);
                    e.Handled = true;
                }
                else if (key == Key.Down)
                {
                    SldVolume.Value = Math.Max(0, SldVolume.Value - 5);
                    e.Handled = true;
                }
            }
        }

        private void SeekFromKeyboard(Key key)
        {
            var status = _facade.GetStatus();
            if (status.SourceRate == 0 || status.SourceTotalSamples == 0 || SldPosition == null) return;

            double duration = status.SourceTotalSamples / (double)status.SourceRate;
            if (duration <= 0) return;

            double requested = duration * Math.Clamp(SldPosition.Value / 100.0, 0.0, 1.0);
            requested = key switch
            {
                Key.Left => requested - 5,
                Key.Right => requested + 5,
                Key.PageDown => requested - 30,
                Key.PageUp => requested + 30,
                Key.Home => 0,
                Key.End => duration,
                _ => requested
            };
            requested = Math.Clamp(requested, 0, duration);
            SldPosition.Value = (requested / duration) * 100.0;
            CommitSeek(); // Issues the actual native seek to WASAPI engine!
        }
    }
}
