# Obsidian Studio — Windows Design Tokens

## Palette

| Token | Hex | Intended role |
|---|---|---|
| Background | `#07090E` | Window canvas |
| Surface | `#101521` | Main panels/sidebar/playback dock |
| Surface Elevated | `#182033` | Menus, cards, dialogs |
| Surface Selected | `#202B40` | Selected row/nav item |
| Gold | `#F5C76A` | Primary action/current selection |
| Gold Highlight | `#FFE5A3` | Rare highlight |
| Blue | `#67B7FF` | Technical secondary accent |
| Text Primary | `#E8ECF4` | Main text |
| Text Secondary | `#A0A9BA` | Artist/metadata |
| Text Muted | `#737F93` | Nonessential labels |
| Divider | `#293246` | Subtle borders |
| Progress Track | `#343D50` | Inactive timeline |
| Success | `#66D6A3` | Success states |
| Warning | `#F2B866` | Warnings |
| Error | `#FF777F` | Errors |

## Desktop sizing
- Sidebar expanded: 220–248 px.
- Sidebar collapsed: 68–76 px.
- Queue rail: 300–360 px.
- Bottom playback dock: 88–112 px.
- Main screen padding: 24–32 px.
- Track row: 56–64 px.
- Track artwork: 36–44 px.
- Album card artwork: 140–220 px depending on available width.
- Main play/pause: 48–56 px.
- Standard clickable control: 36–44 px; primary controls 44–48 px or more.

## Shape
- Artwork thumbnails: 8–10 px.
- Cards/panels: 12–16 px.
- Menus: 8–12 px.
- Dialogs: 16–22 px.
- Pills: fully rounded.

## Typography
Use Segoe UI / Segoe UI Variable for Windows-native feel.
- Page title: 28–32 px, semibold.
- Section title: 18–22 px, semibold.
- Track title: 14–16 px, medium.
- Artist/metadata: 12–14 px.
- Timeline: 11–12 px, tabular numbers if available.
Do not shrink text to fit. Use truncation and tooltips for long names.

## Ratios
Treat 70% dark structural surfaces / 20% elevated surfaces / 10% accents as a loose visual guide, not a literal pixel quota.

## Contrast
Target WCAG AA contrast: 4.5:1 normal text and 3:1 large text / essential graphical components. Verify actual foreground/background combinations. Do not use `#737F93` for critical text without checking contrast.
