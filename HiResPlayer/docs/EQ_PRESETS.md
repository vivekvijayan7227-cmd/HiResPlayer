# HiResPlayer — Equalizer Profiles & Factory Presets

This document specifies the frequency configurations, gains, and filter characteristics of the equalizers in HiResPlayer.

---

## 1. 10-Band Graphic Equalizer

The Graphic Equalizer operates with standard ISO octave band center frequencies:
- **Band 0**: 31 Hz (Sub-bass)
- **Band 1**: 62 Hz (Bass)
- **Band 2**: 125 Hz (Upper Bass)
- **Band 3**: 250 Hz (Warmth / Low-mid)
- **Band 4**: 500 Hz (Midrange)
- **Band 5**: 1,000 Hz (Presence)
- **Band 6**: 2,000 Hz (Definition)
- **Band 7**: 4,000 Hz (Clarity)
- **Band 8**: 8,000 Hz (Brilliance)
- **Band 9**: 16,000 Hz (Air)

Gain range: **-12.0 dB to +12.0 dB** per band.  
Preamp range: **-18.0 dB to +6.0 dB** with clipping prevention.

---

## 2. 17 Factory Presets

HiResPlayer ships with 17 factory presets implemented in `PresetService.cs`:

| # | Preset Name | Preamp | Gains (31Hz -> 16kHz) | Intended Use Case |
|---|---|---|---|---|
| 1 | **Flat** | 0.0 dB | 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 | Reference neutral playback |
| 2 | **Balanced** | 0.0 dB | +1, +1, 0, 0, 0, 0, 0, +1, +1, +1 | Gentle contouring for headphones |
| 3 | **Bass Boost** | -3.0 dB | +5, +4, +3, +2, 0, 0, 0, 0, 0, 0 | Low-end reinforcement (preamp reduced to prevent clipping) |
| 4 | **Bass Reduction** | 0.0 dB | -5, -4, -3, -2, 0, 0, 0, 0, 0, 0 | Thinning bloated low frequencies |
| 5 | **Treble Boost** | -2.0 dB | 0, 0, 0, 0, 0, +1, +2, +3, +4, +5 | Enhancing high-frequency detail and cymbal decay |
| 6 | **Treble Reduction** | 0.0 dB | 0, 0, 0, 0, 0, -1, -2, -3, -4, -5 | Softening harsh or fatigue-inducing tracks |
| 7 | **Warm** | -1.0 dB | +3, +2, +2, +1, 0, 0, -1, -2, -2, -3 | Classic analog roll-off and warm low-mids |
| 8 | **Bright** | -2.0 dB | -2, -2, -1, 0, 0, +1, +2, +3, +4, +4 | Modern open soundstage |
| 9 | **Vocal Clarity** | -2.0 dB | -2, -2, -1, 0, +2, +3, +3, +2, 0, -1 | Accentuates vocal formant presence and intelligibility |
| 10 | **Acoustic** | -1.5 dB | +2, +2, +1, 0, +1, +2, +2, +2, +1, +1 | Unplugged guitar, strings, and live folk |
| 11 | **Classical** | -1.0 dB | +3, +3, +2, 0, 0, 0, 0, +1, +2, +3 | Symphonic balance for large orchestral dynamics |
| 12 | **Jazz** | -1.0 dB | +2, +2, 0, +1, +1, 0, 0, +1, +2, +2 | Upright bass and brass texture |
| 13 | **Rock** | -3.0 dB | +4, +3, +2, 0, -1, 0, +2, +3, +3, +4 | Punchy drums and electric guitar bite |
| 14 | **Pop** | -2.5 dB | +2, +3, +1, 0, +1, +2, +3, +2, +1, +2 | Radio presence and tight kick |
| 15 | **Electronic** | -3.5 dB | +5, +5, +3, 0, -1, +1, 0, +2, +4, +5 | Sub-bass extension and crisp synthesizer sparkle |
| 16 | **Hip-Hop** | -4.0 dB | +6, +5, +3, +1, 0, 0, +1, +2, +2, +1 | Deep 808 sub-harmonics with clipping protection |
| 17 | **Night Listening** | -1.0 dB | -3, -2, -1, 0, +1, +2, +1, 0, -2, -4 | Low-fatigue evening listening; tames peaks |

---

## 3. Filter Implementation

- Equalizer filters are implemented using second-order Direct Form II Transposed (DF2T) biquads derived from the Robert Bristow-Johnson (RBJ) Audio EQ Cookbook.
- Peak filters use constant-Q bandwidth formulations.
- Parameter updates are recalculated off the audio render thread and updated safely with zero audio dropouts.
