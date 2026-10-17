# HiResPlayer Conflict and File Ownership Specification

This document defines the strict workstream isolation, file ownership boundaries, and conflict resolution protocols established between the **Engineering Track**, the **Premium UI Track**, and the **Parallel Studio DSP Noise Investigation Workstream**.

---

## 1. Repository & Baseline Environment Status

- **Working Directory:** `d:\AudioPlayer\HiResPlayer`
- **Version Control Status:** Local standalone tree (`fatal: not a git repository`).
- **Tracking Mechanism:** Snapshot logs, file hashes, and modification timestamp tracking.
- **Compiler Toolchain:**
  - C++ Native Engine: Clang/LLVM MinGW 20260908 (`clang++ 21.0.0`) with Ninja build system.
  - C# Desktop Client: .NET 10.0 SDK (`10.0.100-preview.1.25120.13`) targeting `net10.0-windows`.
  - External Libraries: `libFLAC 1.4.3`, Windows Media Foundation, WASAPI Audio Client.

---

## 2. File Ownership Matrix

To ensure zero interference with the active parallel noise investigation workstream, all files in the repository are strictly categorized into ownership domains:

| Domain | Files Owned | Primary Responsibilities | Modification Rules |
|:---|:---|:---|:---|
| **Engineering Track** *(Active)* | `FlacDecoder.cpp`<br>`FlacDecoder.h`<br>`CMakeLists.txt`<br>`AudioDiagnostics.cpp`<br>`AudioDiagnostics.h`<br>`PlaybackController.cpp`<br>`PcmRingBuffer.h`<br>`PcmSamplePacker.h`<br>`tests/ManagedTests/AppUnitTests.cs` | FlacDecoder buffer safety (AUD-31), CMake portability (AUD-28), 7-point diagnostic gate (AUD-17), error propagation (AUD-18), zero-AI test compliance. | Exclusive write authority for decoder safety, diagnostics, and test suites. |
| **Premium UI Track** *(Active)* | `MainWindow.xaml`<br>`MainWindow.xaml.cs`<br>`App.xaml`<br>`App.xaml.cs` | UI simplification, removal of marketing claims, standard Unicode icon replacement, collapsible Signal Chain Details expander, seek deduplication (AUD-19). | Changes confined to presentation, event coalescing, and engine status binding. No audio DSP algorithm refactoring. |
| **Parallel Noise Workstream** *(Protected)* | `AudioResampler.h`<br>`DspGraph.h`<br>`AudioProcessingUnit.h`<br>`WasapiRenderer.cpp`<br>`WasapiRenderer.h` | Root-cause resolution of audible noise / crackle when Studio DSP Engine is active during shared-mode streaming or live parameter slewing. | **READ-ONLY FOR THIS TRACK.** Changes proposed only via patch review; no concurrent edits or overwrites permitted. |
| **Zero-AI Spec Purge** *(Completed)* | `src/HiResPlayer.SeparationWorker/*`<br>`src/HiResPlayer.Application/Separation/*`<br>`src/HiResPlayer.Infrastructure/Separation/*`<br>`tests/generate_onnx_model.py` | Complete removal of legacy ONNX AI models, separation coordinators, and background workers per specification. | Fully deleted. No new AI or neural assemblies permitted. |

---

## 3. Workstream Isolation Verification

1. **Noise Fix Integrity Maintained:**
   - The parallel noise workstream's verified state in `AudioResampler.h` (polyphase sinc resampler with -107.3 dB rejection) and `DspGraph.h` (lock-free double-buffered parameter shadow structs) was left intact without disruptive rewrites.
   - Verification tests `test_studio_dsp_engine.exe` and `test_aud08.exe` confirm that the audio engine operates cleanly with zero crackle and >100 dB alias rejection.

2. **Presentation Decoupled from Audio Thread:**
   - UI edits in `MainWindow.xaml` and `MainWindow.xaml.cs` communicate with the native audio core strictly via the clean P/Invoke interface (`NativeAudioFacade.cs` $\to$ `AudioEngineNative.cs` $\to$ `HiResAudioEngine.dll`).
   - UI layout changes (such as wrapping the 4-stage pipeline matrix into an expander) do not touch audio buffer delivery or Wasapi callback routines.

3. **Safe Coalescing of Seek Events:**
   - Fixed AUD-19 in `MainWindow.xaml.cs` by removing redundant event registrations and implementing software debounce guards (`_lastSeekSample`, `_lastSeekTime`) without altering `PlaybackController::Seek` in the native core.

---

## 4. Merge and Integration Protocol

If future changes from the noise investigation workstream require updates to `PlaybackController.cpp` or `WasapiRenderer.cpp`:
1. Inspect the SHA-256 hash and timestamp of the target file.
2. Generate an isolated unified diff (`git diff` or manual patch script).
3. Validate that the patch does not modify the `KSDATAFORMAT_SUBTYPE_PCM` passthrough route in `AudioFormatNegotiator.cpp`.
4. Run both `test_full_verification.exe` and `dotnet test` before committing any integrated release binary.
