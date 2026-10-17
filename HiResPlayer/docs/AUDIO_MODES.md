# HiResPlayer — Audio Modes & Bit-Perfect Policy

This document defines the audio playback architecture and quality policies implemented in HiResPlayer for Windows.

---

## 1. Executive Summary

HiResPlayer provides two distinct, non-overlapping audio pipelines:
1. **Original / Bit-Perfect-Eligible Mode**: For purist audiophile listening. Delivers decoded FLAC PCM samples directly to the audio hardware without alteration.
2. **Enhanced Audio Pipeline**: For studio customization, room correction, and vocal isolation. Incorporates real-time DSP, graphic/parametric equalization, and stem mixing.

Every visible control and status indicator reflects the active engine state. **Enhanced Audio streams are never mislabeled as bit-perfect.**

---

## 2. Mode 1: Original / Bit-Perfect-Eligible

### 2.1 The 7-Point Eligibility Gate

To be classified as **Bit-Perfect Eligible**, the audio stream must simultaneously pass all 7 automated checks in `AudioDiagnostics`:

| # | Check | Condition | Description |
|---|---|---|---|
| 1 | `decoder_no_errors` | `== 1` | libFLAC decoded the stream without CRC errors, sync loss, or frame corruption. |
| 2 | `rate_match` | `source_rate == output_rate` | WASAPI endpoint accepted the native sampling frequency without resampling. |
| 3 | `bits_match` | `source_valid_bits == output_valid_bits` | Sample bit depth (16 or 24 valid bits) is preserved without truncating precision. |
| 4 | `channels_match` | `source_channels == output_channels` | Channel count (stereo/mono) matches endpoint configuration without downmixing or matrixing. |
| 5 | `exclusive_active` | `== 1` | WASAPI stream was opened with `AUDCLNT_SHAREMODE_EXCLUSIVE`, bypassing the Windows Audio Engine mixer. |
| 6 | `dsp_disabled` | `== 1` | Preamp, Graphic EQ, Parametric EQ, and Vocal Isolation DSP filters are completely bypassed. |
| 7 | `pcm_passthrough` | `== 1` | Integer PCM samples are packed directly to the hardware wire format without float conversion. |

### 2.2 Truth in Labeling

Eligibility is a software guarantee that HiResPlayer has not modified the decoded PCM samples before sending them to the driver. The UI labels this mode:
```text
Original PCM — Bit-Perfect Eligible
```
If any of the 7 criteria fails (e.g. shared mode mixer active, or device in use), the stream is immediately demoted to:
```text
Processed Audio — Not Bit-perfect
```

---

## 3. Mode 2: Enhanced Audio Pipeline

When EQ, preamp, or vocal isolation features are engaged, audio routing switches to the 64-bit/32-bit floating-point DSP graph:

```text
FLAC Decoder (libFLAC 1.4.3)
      │
      ▼
Linear Float32 Conversion
      │
      ├─► Stem Mixer (Vocals / Instrumental)
      │
      ├─► Mid/Side Center Channel Processor (Traditional Vocal Isolation)
      │
      ├─► Preamp Gain (-18 dB to +6 dB)
      │
      ├─► Biquad Filter Chain (10-Band Graphic EQ / 12-Band Parametric EQ)
      │
      ├─► Peak & Headroom Monitoring
      │
      ▼
Output Format Packing / Dither
      │
      ▼
WASAPI Renderer (Shared or Exclusive)
```

### 3.1 Status Display
Even if output is sent via WASAPI Exclusive mode, the active pipeline displays:
```text
Processed Audio — Not Bit-perfect
```
This ensures total transparency to the user.
