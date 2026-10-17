# WPF Integration Notes

## Resource dictionaries
Merge the dictionaries into `App.xaml` (adjust paths if you change folders):

```xml
<Application.Resources>
    <ResourceDictionary>
        <ResourceDictionary.MergedDictionaries>
            <ResourceDictionary Source="Wpf/Theme/StudioColors.xaml" />
            <ResourceDictionary Source="Wpf/Theme/StudioStyles.xaml" />
        </ResourceDictionary.MergedDictionaries>
    </ResourceDictionary>
</Application.Resources>
```

If the resources are compiled into a different assembly or folder, use the correct `pack://application:,,,/AssemblyName;component/...` URI.

## View model contract for sample mini-player
The sample expects properties/commands similar to:
- `CurrentTrack.Title`, `CurrentTrack.Artist`
- `PositionText`, `DurationText`
- `PositionSeconds`, `DurationSeconds`
- `Volume`
- `ToggleShuffleCommand`, `PreviousCommand`, `PlayPauseCommand`, `NextCommand`, `ToggleRepeatCommand`, `OpenQueueCommand`

Rename bindings to match your real view models. Seeking via a two-way Slider can issue many updates while dragging; in a production player, consider a seek-preview value and commit on drag completion so the media engine isn't repeatedly repositioned.

## Main window sample caveats
`MainWindow.Layout.Sample.xaml` is a layout reference, not a drop-in replacement for an existing `MainWindow.xaml`. It uses example `Tracks`, `SelectedTrack`, and `SearchQuery` bindings. It does not implement library scanning, filtering, queue data, album art decoding, playback, or keyboard commands.

## Validate before shipping
- Confirm the desktop app is WPF.
- Ensure all resource keys resolve.
- Add appropriate `x:Class` code-behind and view models.
- Add commands and keyboard shortcuts to the existing app architecture.
- Check long file paths and metadata, empty artwork, and missing tags.
- Test high DPI and Windows display scaling.
- Test accessibility names, keyboard focus, contrast, and Windows high contrast.
- Keep player state in the existing playback service; the view should observe state, not duplicate it.
