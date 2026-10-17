# HiResPlayer Windows — Obsidian Studio UI/UX Pack

A desktop-first UI/UX specification and WPF resource/component starter using the Obsidian Studio palette.

## Important scope note
The uploaded PDB identifies `HiResPlayer.Installer` as a .NET 10 WPF project. It does not establish that `HiResPlayer.Desktop.exe` uses WPF. These XAML resources are ready for a WPF desktop client; if the actual player uses WinUI 3, WinForms, Avalonia, Electron, or another stack, use the design specification and port the tokens/components to that framework.

## Files
- `docs/WINDOWS_UI_UX_SPEC.md` — recommended changes and desktop interaction rules.
- `docs/DESIGN_TOKENS.md` — palette, typography, spacing, sizing, states.
- `assets/palette.json` — machine-readable palette.
- `Wpf/Theme/StudioColors.xaml` — shared brushes and color tokens.
- `Wpf/Theme/StudioStyles.xaml` — reusable styles for windows, text, buttons, cards, list items, text boxes, sliders, scrollbars, and navigation.
- `Wpf/Controls/TrackRow.xaml` — reusable desktop track-row template.
- `Wpf/Controls/MiniPlayer.xaml` — persistent mini-player component.
- `Wpf/Shell/MainWindow.Layout.Sample.xaml` — shell layout sample with sidebar, library region, queue rail, and mini-player.
- `Wpf/README-INTEGRATION.md` — integration guidance and caveats.

## Suggested integration
1. Confirm the actual UI framework of `HiResPlayer.Desktop.exe` from its source project or `.csproj`.
2. If it is WPF, copy the `Wpf` directory into the desktop project.
3. Merge `StudioColors.xaml` and `StudioStyles.xaml` into `App.xaml` resources, using relative pack URIs appropriate to your project.
4. Start by applying the window/background/text styles, then replace shared components one at a time.
5. Connect the sample controls to the existing playback/library/queue view-models. Do not create a second playback state in the view layer.
6. Test at 100%, 125%, 150%, and 200% Windows display scaling.

These are starter resources, not a drop-in replacement for your existing application. Bindings and command names in the shell sample are examples; adjust them to the actual view models.
