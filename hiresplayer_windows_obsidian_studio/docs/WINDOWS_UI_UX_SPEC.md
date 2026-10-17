# HiResPlayer Windows — Obsidian Studio UI/UX Specification

## 1. Design objective
Make the desktop player feel premium, deliberate, and efficient. The interface should resemble a refined listening room, not a generic media dashboard and not a digital audio workstation with unnecessary complexity.

Visual hierarchy:
1. Current track and playback controls.
2. Library browsing and search.
3. Queue and playlist management.
4. Audio configuration and technical metadata.

## 2. Recommended desktop shell
Use a three-region shell at typical desktop widths:
- Left navigation rail/sidebar: 220–248 px.
- Main content: flexible, minimum 520 px.
- Optional queue rail: 300–360 px.
- Bottom playback dock: 88–112 px high, depending on artwork and controls.

At narrower widths:
- Collapse the queue rail to a drawer/panel.
- Reduce sidebar to icon rail around 72 px.
- Keep play/pause, previous/next, seek, and volume reachable.
- Avoid horizontal overflow; do not shrink text below comfortable reading sizes.

## 3. Palette and semantics
- Background `#07090E`: primary window canvas.
- Surface `#101521`: sidebar, playback dock, main panels.
- Elevated `#182033`: cards, popovers, context menus.
- Selected `#202B40`: selected row/navigation item.
- Gold `#F5C76A`: primary actions, current track, active navigation.
- Blue `#67B7FF`: secondary technical states, waveform/spectrum, audio analysis.
- Primary text `#E8ECF4`; secondary `#A0A9BA`; muted `#737F93`.
- Divider `#293246`; progress track `#343D50`.

Gold must not mean “everything important”; reserve it for primary action and current/selected playback state. Blue should be used sparingly for technical or secondary information.

## 4. Navigation
Suggested sidebar:
- Home / Overview (optional)
- Songs
- Albums
- Artists
- Genres
- Folders
- Playlists
- Queue / History
- Favorites
- Equalizer
- Settings

Use a clear selected item with a narrow gold marker or subtle gold-tinted surface. Avoid a glowing outline around the entire sidebar item.

## 5. Library and list views
- A clear title row with search, sort, filter, and view-mode actions.
- Table/list columns where useful: Track, Artist, Album, Duration, Format/Quality (optional).
- Use consistent row height around 56–64 px for dense desktop lists.
- Artwork thumbnails 36–44 px; use 8–10 px corner radius.
- Current track: selected navy surface plus gold playback icon/indicator.
- Hover: subtle surface change; don't animate row backgrounds heavily.
- Support keyboard navigation, multi-select, context menus, and drag/drop where supported.
- Provide useful empty states with Scan/Choose Folder actions.

## 6. Now Playing / artwork
- Use a dedicated Now Playing page or expandable main-content view.
- Large artwork, track/artist/title metadata, favorite, and track actions.
- Keep the transport bar persistent even when browsing the library.
- Avoid oversized artwork that hides the queue or controls on short windows.
- Artwork-derived ambient tint may be used behind artwork only if text contrast remains strong.

## 7. Playback dock
- Left: artwork thumbnail + title and artist.
- Center: shuffle/repeat, previous, play/pause, next, seek bar, elapsed/total time.
- Right: queue, output/device if supported, volume, and overflow.
- Main play/pause button: 48–56 px, gold fill, dark foreground.
- Secondary transport controls: neutral icons, gold only when active.
- Seek bar uses gold progress and a subdued navy-gray track.
- Ensure controls have tooltips, keyboard shortcuts where appropriate, and accessible names.

## 8. Queue panel
- Clearly label “Playing next” or “Queue”.
- Current track gets a gold indicator and subtle selected surface.
- Support reorder and remove actions; show undo for accidental removal.
- Distinguish a temporary playback queue from saved playlists.
- Keep the queue visible without covering essential library controls.
- Include a useful empty queue state.

## 9. Search and filtering
- Global search should search local metadata and supported tags.
- Search field must have clear button and keyboard focus state.
- Filter chips or dropdowns for Songs, Albums, Artists, Folders, and Playlists.
- Sort options should be explicit, not hidden behind ambiguous icons.
- Provide loading, no results, and permission/indexing error states.

## 10. Equalizer and technical panels
- Use blue for spectrum/response visualization.
- Use gold for selected preset and active controls.
- Show only capabilities supported by the audio engine/device.
- Technical metadata should be readable but secondary to playback.
- Avoid fake or decorative sliders that don't affect actual audio output.

## 11. Settings
Group into sections:
- Playback: gapless, crossfade if supported, resume behavior, replay gain if supported.
- Audio: equalizer, output device, audio effects.
- Library: scan folders, exclusions, metadata, sorting.
- Appearance: theme, accent intensity, density, artwork.
- Keyboard: shortcuts and remapping if supported.
- Notifications: Windows media controls and notification behavior.
- Storage: library location, cache, diagnostics.
- About: version, licenses, feedback.

## 12. Interaction details
- Button hover: slightly brighter surface, no dramatic glow.
- Pressed: brief lower-brightness or subtle scale feedback.
- Selected: persistent selected state; do not rely on hover alone.
- Menus: dark elevated surface, 8–12 px corner radius, clear keyboard focus.
- Tooltips: show after a short delay for icon-only controls.
- Seek dragging should preview position and commit through the existing playback service.
- Confirm destructive library/file operations; don't confirm every harmless queue action.
- Provide undo for reversible actions.

## 13. Motion
- Hover/pressed feedback: 100–160 ms.
- Sidebar/content transition: 160–220 ms.
- Queue drawer: 200–280 ms.
- Artwork crossfade: 180–280 ms.
Respect reduced-motion preferences and avoid animations that delay transport controls.

## 14. Accessibility and Windows behavior
- Keyboard focus must be clearly visible.
- Support Tab/Shift+Tab, Enter/Space, Escape, arrow navigation in lists, and common playback shortcuts.
- Every icon-only control needs an AutomationProperties.Name or equivalent accessible label.
- Respect Windows text scaling and display scaling at 100%, 125%, 150%, and 200%.
- Do not encode state with color alone.
- Test high contrast and system reduced-motion settings where applicable.
- Use minimum 32–36 px visible targets for dense desktop controls and 40–48 px for primary playback/touch-friendly controls.

## 15. Recommended implementation order
1. Theme tokens and typography.
2. Main shell/sidebar/content regions.
3. Persistent playback dock.
4. Track rows, album cards, selection/hover states.
5. Queue panel and search.
6. Equalizer/settings.
7. Keyboard, accessibility, scaling, empty/error/loading states.
8. Visual polish and performance testing.
