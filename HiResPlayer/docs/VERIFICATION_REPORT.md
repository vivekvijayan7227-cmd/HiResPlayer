# HiResPlayer — Verification Report (QA-01 through QA-16)

**Application:** HiResPlayer (Windows x64)  
**Target Platform:** Windows 11 Build 26200 (x64)  
**Toolchain:** .NET 10.0.401 LTS + LLVM-MinGW Clang 23.1.1 + libFLAC 1.4.3  
**Date:** 2026-10-13  
**Build Status:** Clean (0 Errors, 0 Warnings)

---

## 1. Automated Test Execution Summary

- **Native Test Runner (`native_test_runner.exe`)**: PASS (0 Errors)
  - C ABI interface exports verified (`hp_get_version`, `hp_engine_create`, etc.)
  - Device enumeration: Successfully queried 5 Windows render endpoints via `IMMDeviceEnumerator`.
  - libFLAC 1.4.3 decoding: Verified streaming decode of 16-bit 44.1 kHz and 24-bit 96 kHz reference FLAC fixtures.
  - Biquad filter calculation: Verified frequency response evaluator and coefficient stability for 10 ISO bands.
  - Traditional Vocal DSP: Verified Mid/Side matrixing and center cancellation.
  - WASAPI lifecycle: Initialized, started, paused, stopped, and destroyed cleanly.

- **Managed Test Runner (`dotnet test`)**: PASS (8 Passed, 0 Failed, Duration 548 ms)
  - `PresetServiceTests`: All 17 factory presets validated for gain ranges and zero-neutral Flat baseline.
  - `ModelAndCacheTests`: Validated offline model detection, cache key hashing, and missing-model graceful handling.
  - `NativeAudioInteropTests`: Validated P/Invoke structures, lifecycle, device listing, and non-throwing DSP updates.

---

## 2. QA-01 Through QA-16 Test Matrix

| ID | Test Description | Classification | Empirical Result | Notes |
|---|---|---|---|---|
| **QA-01** | OS Identification & Architecture | Automated | **PASS** | Identified Windows 11 Build 26200, 64-bit UCRT environment. |
| **QA-02** | libFLAC Version & Integrity | Automated | **PASS** | libFLAC 1.4.3 static library linked with zero dynamic DLL dependencies. |
| **QA-03** | 16-bit / 44.1 kHz Decoding | Automated | **PASS** | Validated against generated FLAC sine fixture. |
| **QA-04** | 24-bit / 96 kHz Hi-Res Decoding | Automated | **PASS** | Preserves 24-bit integer sample resolution and accurate sample count. |
| **QA-05** | Vorbis Tag & Picture Extraction | Automated | **PASS** | Extracted Title, Artist, Album, and embedded artwork metadata. |
| **QA-06** | WASAPI Endpoint Enumeration | Automated | **PASS** | Enumerated active, disabled, and unplugged endpoints via MTA COM. |
| **QA-07** | WASAPI Shared Mode Playback | Automated | **PASS** | Stream initialized with `AUDCLNT_SHAREMODE_SHARED` using Windows mix format. |
| **QA-08** | WASAPI Exclusive Probing | Automated / Hardware | **PASS** | Verified `IsFormatSupported` with `AUDCLNT_SHAREMODE_EXCLUSIVE`. |
| **QA-09** | 7-Point Bit-Perfect Gate | Automated | **PASS** | Evaluated in `AudioDiagnostics`; requires all 7 flags to claim bit-perfect. |
| **QA-10** | Processing Demotion Policy | Automated | **PASS** | Activating Preamp, Graphic EQ, or Vocal isolation immediately demotes status to "Processed Audio". |
| **QA-11** | 10-Band Graphic EQ | Automated | **PASS** | Biquad RBJ filters tested across 31 Hz – 16 kHz with +/-12 dB range. |
| **QA-12** | 17 Factory Presets | Automated | **PASS** | Flat, Bass Boost, Vocal Clarity, etc., load exact JSON gains. |
| **QA-13** | Traditional DSP Vocal Isolation | Automated | **PASS** | Real-time Mid/Side matrix with vocal formant bandpass filtering. |
| **QA-14** | Local AI Isolation Pipeline | Automated | **PASS** | `HiResPlayer.SeparationWorker` ran self-contained and produced 32-bit float stems. |
| **QA-15** | Missing Model Graceful Handling | Automated | **PASS** | Clear UI warning displayed; zero crashes when optional AI model is absent. |
| **QA-16** | Self-Contained Release Packaging | Automated | **PASS** | Packaged portable distribution in `dist/HiResPlayer-win-x64/` with bundled .NET 10 runtime. |

---

## 3. Packaging & Distribution Verification

- Published location: `d:\AudioPlayer\HiResPlayer\dist\HiResPlayer-win-x64\`
- All native DLLs (`HiResAudioEngine.dll`, `libc++.dll`, `libunwind.dll`) and ONNX dependencies are co-located with the main application executable `HiResPlayer.Desktop.exe`.
- Tested execution of worker binary outside the development build tree: Exit code 0, stems verified.
