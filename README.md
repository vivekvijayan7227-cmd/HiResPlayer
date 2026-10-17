# HiResPlayer — Obsidian Studio

<p align="center">
  <img src="HiResPlayer/assets/app_icon.png" alt="HiResPlayer Obsidian Studio Icon" width="160" height="160" />
</p>

<p align="center">
  <strong>Mastering-Grade Bit-Perfect FLAC Audio Player & Acoustic Mastering Studio for Windows</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%2011%20%7C%2010%20(x64)-0078D6?style=for-the-badge&logo=windows" alt="Windows" />
  <img src="https://img.shields.io/badge/Framework-.NET%2010%20WPF-512BD4?style=for-the-badge&logo=dotnet" alt=".NET 10" />
  <img src="https://img.shields.io/badge/Core-C%2B%2B20%20Native%20WASAPI-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++20" />
  <img src="https://img.shields.io/badge/Playback-Bit--Perfect%20Direct-F5C76A?style=for-the-badge" alt="Bit-Perfect" />
  <img src="https://img.shields.io/badge/Theme-Obsidian%20Studio-07090E?style=for-the-badge" alt="Obsidian Studio" />
  <img src="https://img.shields.io/badge/License-MIT-green.svg?style=for-the-badge" alt="License" />
</p>

---

## Overview

**HiResPlayer** is a dedicated high-resolution FLAC audio player and digital signal processing acoustic studio built natively for Windows 11 and 10. Designed with a pure studio hardware aesthetic—**Obsidian Studio** (dark glass surfaces `#07090E` / `#101521` and brushed champagne gold `#F5C76A`)—HiResPlayer bypasses the Windows audio mixer completely to deliver unadulterated, bit-perfect high-resolution PCM audio straight to your DAC.

100% offline, zero telemetry, zero accounts, and zero cloud dependencies.

---

## Key Features

### 🎧 Bit-Perfect WASAPI Exclusive Pipeline
- **Direct Hardware Rate Switching:** Automatically queries endpoint capabilities and re-configures the audio hardware format on the fly (up to 192 kHz / 24-bit).
- **Windows Mixer Bypass:** Bypasses Windows Audio Session API shared mixer (`audiodg.exe`) and OS sample-rate converters, eliminating distortion, limiter clipping, and unwanted resampling.
- **Precision Diagnostics:** Real-time stream telemetry displaying active endpoint, source sample rate/bit depth, negotiated output format, buffer latency, and bit-perfect eligibility status.

### ⚡ C++20 High-Performance Native Core
- **Native libFLAC Engine:** Fast, zero-copy FLAC decoding running in a dedicated native DLL (`HiResAudioEngine.dll`).
- **Lock-Free PcmRingBuffer:** Single-producer single-consumer lock-free ring buffer decouples file decoding from real-time audio rendering.
- **MMCSS Real-Time Priority:** Audio render threads run under Windows Multimedia Class Scheduler Service (MMCSS `Pro Audio` / `Audio` tasks) to prevent buffer underruns.

### 🎛️ 10-Band Studio Graphic Equalizer
- **Mastering-Grade Digital Biquad Filters:** 10 precision standard ISO frequency bands (31 Hz, 62 Hz, 125 Hz, 250 Hz, 500 Hz, 1 kHz, 2 kHz, 4 kHz, 8 kHz, 16 kHz).
- **Linear-Phase Preamp & Attenuation:** Dynamic ±12 dB slider control with software headroom management to prevent digital inter-sample clipping.
- **Acoustic Presets:** Instant recall for Flat, Bass Boost, Vocal Enhance, Acoustic, Studio Monitor, Classical, and Custom user presets.

### 🎙️ Focus Studio (Vocal Isolation & Mastering Audition)
- **M/S Center-Channel Extraction:** Real-time Mid/Side phase cancellation algorithm to isolate or extract center-panned vocals.
- **Instrumental Focus Mode:** Suppresses vocals for clean acoustic and instrumental auditioning.
- **Fine Bandwidth & Attenuation Controls:** Dial in frequency bandwidths (120 Hz – 8 kHz) and suppression depths in real time.

### 📶 Bluetooth Fidelity Engine
- **Profile & Codec Auto-Detection:** Automatically identifies active Bluetooth endpoints and reports capability tiers (AAC, aptX, LDAC, SBC).
- **Dynamic Latency Compensation:** Compensates for wireless audio latency with automatic buffer management.

### 💎 Obsidian Studio Design
- **Luxury Hardware Aesthetic:** Clean dark glass chassis (`#07090E`), brushed titanium bevels, and champagne gold primary controls (`#F5C76A`).
- **Seamless Taskbar Integration:** Custom Win32 `WM_GETMINMAXINFO` window procedure hook ensures the window respects monitor working areas (`rcWork`) and never hides beneath the Windows taskbar.
- **Persistent Playback Dock:** Full transport controls, volume slider, interactive seekbar, and quick-queue drawer accessible across all views.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│              HiResPlayer Desktop UI (WPF / .NET 10)         │
│  - Obsidian Studio Design System (Styles, Themes, Tokens)   │
│  - Library Management, Playlists, Search, Queue Rail        │
│  - Real-Time Spectrum Visualizer & Diagnostics HUD          │
└──────────────────────────────┬──────────────────────────────┘
                               │ P/Invoke (NativeAudioFacade)
┌──────────────────────────────▼──────────────────────────────┐
│           HiResAudioEngine (C++20 Native DLL)               │
│  - FlacDecoder (libFLAC 1.4.3 C++ integration)              │
│  - PcmRingBuffer (Lock-Free SPSC FIFO)                      │
│  - AudioFormatNegotiator (Exclusive & Shared Prober)        │
│  - WasapiRenderer (MMCSS Audio Thread, Event-Driven)        │
│  - DspEngine (10-Band Biquad EQ & Center-Extraction M/S)    │
└──────────────────────────────┬──────────────────────────────┘
                               │ WASAPI Exclusive Mode
┌──────────────────────────────▼──────────────────────────────┐
│                    Hardware DAC / Endpoint                  │
│  (Realtek / USB DAC / External Headphone Amplifier / BT)   │
└─────────────────────────────────────────────────────────────┘
```

---

## Download & Installation

### Option 1: Standalone Single-File Setup (Recommended)
Download the latest `HiResPlayer-Setup-v1.0.1.exe` from the [Releases](https://github.com/vivekvijayan7227-cmd/HiResPlayer/releases) page:
1. Run `HiResPlayer-Setup-v1.0.1.exe`.
2. Follow the setup wizard to choose an installation directory.
3. Launch from the desktop shortcut or Start Menu.

### Option 2: Portable Distribution
1. Download `HiResPlayer-win-x64-v1.0.1.zip` from Releases.
2. Extract to any directory.
3. Run `HiResPlayer.Desktop.exe`.

---

## Building from Source

### Prerequisites
- **Operating System:** Windows 10 (1809+) or Windows 11 (64-bit)
- **.NET SDK:** [.NET 10 SDK](https://dotnet.microsoft.com/)
- **C++ Compiler:** Clang / LLVM (MinGW) or MSVC with C++20 support
- **CMake:** Version 3.20 or newer
- **PowerShell:** PowerShell 5.1 or PowerShell 7+

### 1. Build Native Audio Engine
```powershell
.\HiResPlayer\scripts\build_engine.ps1
```

### 2. Build Desktop Application
```powershell
.\HiResPlayer\scripts\publish.ps1
```

### 3. Build Standalone Installer
```powershell
.\HiResPlayer\scripts\build_installer.ps1
```

Outputs will be placed into `dist/HiResPlayer-win-x64` and `Installer/HiResPlayer-Setup-v1.0.1.exe`.

---

## Engineering Quality & Verification

HiResPlayer v1.0.1 incorporates extensive native engine hardening and architectural verification:
- **WASAPI Format Negotiation:** Exact vs. closest-match negotiation with `IsFormatSupported(..., &pClosestMatch)`, preventing unexpected format fallbacks.
- **Prebuffered Startup:** Decoupled buffer preparation (`Prepare`) from hardware DAC clock start (`StartStream`), ensuring ~100ms PCM prebuffering to eliminate initial click/pop artifacts.
- **Diagnostic Precision:** Granular tracking of full and partial ring buffer underruns, frame starvation counts, and dynamic bit-perfect validation (`pcm_pass`).
- **Wait-Free Spectrum Synchronization:** Lock-free double-buffered sequence lock (`seqlock`) eliminating render thread contention during FFT snapshot generation.
- **Security Hardening:** Installer verified against Zip Slip path traversal vulnerabilities.
- **Automated Verification:** 40 / 40 passing managed unit & integration tests covering golden-reference PCM fidelity, rapid seek continuity, natural track replay, and format preservation.

---

## Keyboard Shortcuts

| Shortcut | Action |
|:---|:---|
| <kbd>Space</kbd> | Play / Pause |
| <kbd>Ctrl</kbd> + <kbd>→</kbd> | Next Track |
| <kbd>Ctrl</kbd> + <kbd>←</kbd> | Previous Track |
| <kbd>↑</kbd> | Volume Up (+5%) |
| <kbd>↓</kbd> | Volume Down (-5%) |
| <kbd>M</kbd> | Toggle Mute |
| <kbd>F</kbd> | Toggle Favorite for active track |
| <kbd>/</kbd> | Focus Library Search |
| <kbd>Esc</kbd> | Close Modals / Clear Search |

---

## License

This project is licensed under the [MIT License](LICENSE).

---

## Acknowledgments
- [Xiph.Org Foundation](https://xiph.org/flac/) for libFLAC.
- Microsoft for the Windows Audio Session API (WASAPI) and WPF platform.
