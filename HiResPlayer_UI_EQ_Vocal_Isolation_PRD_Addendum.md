# HiResPlayer — PRD Addendum: Minimalist UX, Advanced Equalizer, and Vocal/Instrumental Isolation

**Status:** Proposed feature expansion for the existing Windows HiResPlayer PRD and Antigravity M0–M6 implementation plan  
**Date:** 2026-10-13  
**Target:** Windows desktop, WPF/.NET 10 UI, C++20 + libFLAC + WASAPI engine, built-in analog output and external USB DAC  
**Priority:** UI and EQ are required for the revised product; source separation is a separate advanced feature and must be fully implemented and tested before being represented as available.

## 1. Product outcome

Build an elegant, responsive, offline-first FLAC music player with two deliberately distinct playback experiences:

1. **Original / Bit-perfect-eligible:** Preserve the decoded source PCM in supported WASAPI exclusive mode. EQ, source separation, software gain/normalization, resampling and other DSP must be bypassed. Eligibility is a software claim, not proof of what the DAC outputs.
2. **Enhanced Audio:** Enable a highly configurable EQ, preset management, an audio visualizer, and optional AI-generated vocal/instrumental tracks. The UI must clearly label this path **Processed Audio — Not Bit-perfect**. WASAPI exclusive mode may still be selected for processed audio; exclusive and bit-perfect are not synonyms.

**Important semantics:** “Mute background” is interpreted as reducing music accompaniment and focusing on vocals. “Mute vocals” is interpreted as creating or playing an approximate instrumental. Neither should be described as perfect removal. Environmental noise cancellation is a separate, out-of-scope feature.

## 2. Scope and release gates

| Feature | Scope | Ship criterion |
| --- | --- | --- |
| Minimal, responsive UI | Revised M4 | Required before UI milestone passes |
| Dark / light / system theme | Revised M4 | Required |
| Graphic EQ with factory presets | New M7 | Required for expanded HiResPlayer release |
| Parametric EQ + user presets | New M7 | Required for expanded HiResPlayer release |
| Bit-perfect / DSP policy enforcement | M5 + M7 | Required; cannot be labeled bit-perfect when audio is modified |
| Vocals vs instrumental two-stem tool | New M8 | Advanced feature gate; must not be shown as functional until working |
| Spectrum display | M7 or later | Optional if it affects release schedule; no fake rendering |
| Four-stem mixer / karaoke lyrics / streaming | Future | Explicitly out of scope |

Continue M0–M6 for the working audio foundation. Add M7 (EQ), M8 (source separation), M9 (UX and DSP regression testing) rather than replacing native-playback milestones. The app can ship a core build before M8, but do not represent vocal separation as completed unless M8 criteria pass.

## 3. UX design principles

- Calm, modern, restrained visual design inspired by Windows Fluent conventions. No overloaded dashboards or gratuitous glass blur.
- Music controls must be immediately available. The EQ and isolation tools must be discoverable without crowding the Now Playing screen.
- Use WPF / .NET 10 with styles based on Fluent `ThemeMode` where supported; provide robust dark, light, and system-theme behavior. Preserve system high-contrast behavior.
- Responsive layouts at 1280x720, 1440x900, 1920x1080 and Windows scaling 100%, 150%, 200%. Minimum usable app size **1000x650** unless a narrower compact layout is implemented. No clipped controls at supported sizes.
- Single-window app with consistent navigation, keyboard focus, screen-reader labels, tooltips for audio terminology, visible loading/error states and reduced-motion option.
- Optional album-art-derived accent color, but ensure contrast and user override. No dependence on networking for artwork or library functionality.
- Playback controls and seek bar should always remain usable while scans, cache generation or model processing run.

### 3.1 Screen inventory

1. **Home / Library** — left collapsible sidebar with Library, Albums, Artists, Playlists, Folders, and Settings; main track table with search, sort and virtualized lists; pinned bottom transport bar.
2. **Now Playing** — prominent album cover, title/artist/album, progress, play/pause/next/previous, queue, output device, source badge and requested/negotiated output format.
3. **Equalizer** — accessible from Now Playing and Settings. Two tabs: `Graphic EQ` and `Parametric EQ`; preset selector, reset, bypass, save/rename/delete/import/export; live response-curve preview.
4. **Vocal / Instrumental Studio** — simple preset buttons `Original Mix`, `Vocals Focus`, `Instrumental Focus`, then Advanced stem sliders. Clearly show `Not processed`, `Processing (percent)`, `Ready`, `Failed`, or `Model unavailable`.
5. **Audio Device / Quality** — actual Windows render endpoints, shared/exclusive selection, output-format capability and bit-perfect eligibility explanations.
6. **Settings** — appearance, keyboard controls, scan locations, cache management, model download/selection, hardware acceleration and privacy.

### 3.2 Wireframe (conceptual)

```text
+--------------------------------------------------------------------------+
| HiResPlayer                  Search tracks, albums, artists        [⚙]  |
+---------------+----------------------------------------------------------+
| Library       | NOW PLAYING                                               |
| Albums        |     [ album art ]   Track Title                             |
| Artists       |                     Artist / Album                        |
| Playlists     |                     FLAC • 24-bit / 96 kHz                |
| Folders       |                                                          |
|               |    [Original] [Equalizer] [Vocal Studio] [Output]          |
|               |                                                          |
|               |    Advanced controls appear only in their own panel       |
+---------------+----------------------------------------------------------+
| Album  Track / Artist         01:23  [previous] [play] [next]  04:12     |
| Output: USB DAC     Mode: Enhanced Audio     EQ: Warm     [Queue]         |
+--------------------------------------------------------------------------+
```

### 3.3 UX acceptance tests

- New user can import a music folder and play a FLAC file without a tutorial.
- New user can select an external DAC, choose an EQ preset, reset to Flat and revert to original playback within a few clicks.
- Primary actions have keyboard shortcuts and accessible labels; keyboard-only navigation covers the EQ panel and model-processing status.
- Library search/scroll remains responsive for 10,000 tracks through list virtualization; long metadata scans and AI jobs do not freeze the UI.
- Every requested mode reflects **actual current engine state**, not merely the last button pressed.
- Visual transitions should not delay audio start/stop or incur CPU-heavy rendering on the WASAPI audio thread.

## 4. EQ functional requirements

### 4.1 Graphic equalizer

- **10-band EQ** centers: 31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 Hz.
- Gain range per band: **-12 dB to +12 dB**, with a numeric field and reset-to-0 action.
- Master **preamp: -18 dB to +6 dB** with clear clipping/headroom indication. Consider auto-headroom as an explicit optional setting; it modifies PCM and must not be described as bit-perfect.
- Realtime edits while playing; smoothing or safe coefficient interpolation must avoid clicks/zipper noise.
- Responsive gain curve showing predicted response, not fabricated measurements.

### 4.2 Parametric equalizer

- At least **12 independently configurable bands**.
- Supported filter types: peak/bell, low shelf, high shelf, low pass, high pass, notch. Each band can be toggled on/off.
- Each band has frequency, gain (where applicable), Q/bandwidth, and channel/link controls only if technically supported; do not expose inert options.
- Validate all parameters against Nyquist and coefficient-stability constraints for the active sample rate.
- Provide precise number entry as well as sliders and draggable response-curve handles, with synchronized values.
- If high-order filters are desired, implement as cascaded, tested biquads rather than promising unsupported behavior.

### 4.3 Factory presets

Provide at least 16 editable-as-copy factory profiles: **Flat, Balanced, Bass Boost, Bass Reduction, Treble Boost, Treble Reduction, Warm, Bright, Vocal Clarity, Acoustic, Classical, Jazz, Rock, Pop, Electronic, Hip-Hop, and Night Listening**. Names do not promise objective acoustic optimization. Store actual frequency/gain/Q data as versioned JSON. Users may duplicate factory presets, create unlimited custom presets, rename, delete, import and export them without overwriting factory defaults.

Support optional EQ profiles per DAC endpoint ID and a clearly labeled global default. Profiles must not switch silently during bit-perfect playback.

### 4.4 DSP implementation

Add a C++ `DspGraph` after FLAC decoding and before PCM packing/WASAPI rendering:

```text
FLAC file -> libFLAC decoder -> decoded integer PCM
              | Original mode:
              +--> identity/sample-pack (bit-exact) ---> WASAPI exclusive
              |
              | Enhanced Audio mode:
              +--> convert to float32/float64 --> optional stems mixer
                    --> preamp --> EQ biquad chain --> peak/headroom handling
                    --> output format conversion / resampling if needed
                    --> WASAPI shared or exclusive (PROCESSED)
```

- Implement tested biquad filters, e.g. following the W3C / RBJ Audio EQ Cookbook. Calculate coefficient updates off the real-time render thread.
- Use per-channel stable filter state, denormal handling, coefficient smoothing, and fixed allocation on the render path.
- Protect against clipping: show peak status, default negative headroom when boost is applied; a limiter can be an *explicit optional* feature, not an invisible processing stage.
- If quantizing floating DSP output to 16-/24-bit integer PCM, choose and document a tested rounding/dither policy. Dither should be optional where appropriate and never applied in Original mode.
- When the output sample rate changes between tracks, rebuild filters safely using the new Fs.
- Never claim that an exclusive stream containing EQ-processed audio is bit-perfect.
- Bypass button: ensure the original decoded PCM bypasses every DSP conversion, gain and stem stage, and tests compare produced bytes to expected integer PCM.

## 5. Vocal / instrumental isolation functional requirements

### 5.1 End-user experience

- Show three simple modes: **Original Mix**, **Vocals Focus**, **Instrumental Focus**.
- Advanced controls after processing: independent **Vocals** and **Instrumental** volume sliders (0–100% for first release), with Solo/Mute actions. All controls must be real and routed to the audio engine.
- `Vocals Focus` mutes/reduces the estimated accompaniment stem; `Instrumental Focus` mutes/reduces the estimated vocal stem. Expose the raw estimated stems as audibly imperfect results, not guaranteed perfect isolation.
- When a track lacks cached stems: offer `Prepare stems` (user action) and show model installation status, processing progress, cancel, and available disk space. Continue original audio playback during the background job.
- Instant stem slider interaction is required **after stems are prepared**, not while inference is still running.
- Switching back to `Original Mix` uses the original PCM path, not the potentially imperfect sum of model-generated stems.

### 5.2 Architecture and implementation choice

Do **not** put AI inference in the real-time WASAPI render thread. Keep source separation as an optional **local process/service** with queue and cancellation. The primary player must still start, run and play original FLAC files if the AI component or model is absent.

Architecture:

```text
WPF Vocal Studio UI --> SeparationCoordinator (C#)
    --> low-priority background SeparationWorker (isolated process)
    --> local model backend (provenance/license verified)
    --> time-aligned float PCM stems (vocals + accompaniment)
    --> keyed stem-cache + metadata/verification
    --> C++ StemMixer (reads cached prepared stems)
    --> Enhanced-Audio DSP graph --> WASAPI endpoint
```

Suggested initial reference implementation: **Demucs/HTDemucs two-stem export via an isolated Python process** for feasibility testing. The upstream Demucs repository was archived in January 2025, so it must not be treated as a maintained long-term dependency without explicit review. Validate the license of both code **and pretrained weights**, plus dependency and redistribution obligations.

For production, evaluate a separately packaged ONNX model with **Windows ML / ONNX Runtime** (CPU fallback, hardware acceleration where compatible). Do not assume arbitrary Demucs model weights can be loaded through ONNX Runtime without conversion and validation. The processing backend should remain swappable via an `IStemSeparationBackend` contract.

- CPU-only operation should be possible but may take substantial time; detect and report acceleration support honestly.
- Model download is opt-in, with size, license attribution, model ID/version and checksum; never silently download a large model.
- After a model has been installed, track preparation and playback should work offline.
- The underlying original FLAC must never be overwritten or modified.

### 5.3 Sample-rate alignment and cache

- Account for the source separation model's required input sample rate and channel layout. Convert solely within the isolation pipeline and accurately report `AI processed` in the UI.
- Store output stems in a lossless working format with explicit sample rate, channel count, precision and exact sample lengths; align both stems to a common timeline, compensating for model padding/delay.
- Cache key combines original file identity/content fingerprint, size/mtime as safeguards, separation model/version, processing settings and output representation.
- Write temporary files atomically; cancel and crash recovery must remove or retry partial results.
- Add configurable cache quota (e.g., default 5 GB) and clear-cache UI. A model download folder is distinct from audio stem cache.
- The `Original Mix` button must be immediate even when no stems exist. Playback must never pause waiting for AI inference.

### 5.4 Correctness and limitations

- AI separation is approximate and may introduce musical artifacts, residual vocals/instruments, altered timbre and lower effective resolution. Avoid claims such as `studio-perfect isolation`.
- Distinguish stored FLAC source bit depth/sample rate from the model's processing sample rate and the current WASAPI output format. Do not display `Hi-Res bit-perfect` during isolated playback.
- Avoid representing basic mid-channel cancellation or EQ as reliable vocal separation. If the model is missing or fails, show the feature as unavailable with recovery instructions.
- Source-separation inference can take significantly longer than the track's play duration on some machines, especially on CPU. Display measured progress, not a fabricated time estimate.

## 6. Proposed new modules and files

```text
src/
  HiResPlayer.Desktop/
    Views/EqualizerView.xaml
    Views/VocalStudioView.xaml
    Views/AudioQualityView.xaml
    Themes/FluentThemeResources.xaml
    ViewModels/EqualizerViewModel.cs
    ViewModels/VocalStudioViewModel.cs
  HiResPlayer.Application/
    Equalizer/EqualizerService.cs
    Equalizer/PresetService.cs
    Separation/SeparationCoordinator.cs
    Separation/IStemSeparationBackend.cs
    Separation/SeparationJob.cs
  HiResPlayer.Infrastructure/
    Presets/PresetRepository.cs
    Separation/StemCacheRepository.cs
    Separation/ModelAssetManager.cs
  HiResAudioEngine/
    include/dsp_api.h
    src/dsp/DspGraph.cpp
    src/dsp/BiquadEq.cpp
    src/dsp/ParametricEqualizer.cpp
    src/dsp/Preamp.cpp
    src/dsp/StemMixer.cpp
  HiResPlayer.SeparationWorker/
    README.md
    [validated Python model runner or ONNX backend]

tests/
  NativeTests/BiquadEqTests.cpp
  NativeTests/DspBypassTests.cpp
  NativeTests/StemMixerTests.cpp
  ManagedTests/PresetPersistenceTests.cs
  ManagedTests/SeparationCoordinatorTests.cs
  UiTests/KeyboardAccessibilityChecklist.md

docs/
  AUDIO_MODES.md
  EQ_PRESETS.md
  MODEL_LICENSES_AND_PROVENANCE.md
  STEM_SEPARATION_QA.md
```

Use descriptive equivalents if the repository's existing convention differs. **Do not expose unimplemented controls**; finished features must have functioning engine integration and tests.

## 7. Milestone changes for Antigravity

### Update M4: UI foundation

Implement theme tokens, layouts, album-art fallbacks, responsive navigation, persistent transport controls, output-quality badges and keyboard accessibility. Add disabled states for advanced features until engines exist. Demonstrate real playback integrated into the UI before considering M4 complete.

### Update M5: output/quality policy

Maintain the strict bit-perfect signal path. Define an engine-level `AudioMode` that governs what is and is not permitted. Introduce an explicit `Processed Audio — Not Bit-perfect` state for every EQ or separation operation. Validate endpoint/format state after every change.

### New M7: Complete advanced equalizer

Deliver 10-band graphic EQ, 12-band parametric EQ, 16+ factory presets, save/import/export custom presets, bypass, clipping/headroom feedback and native DSP tests. Verify no audio dropouts or audible clicks during ordinary parameter changes. Prove the DSP bypass path is bit-exact.

### New M8: Complete vocal/instrumental studio

Deliver a tested model-backed two-stem local separation workflow, processing UI, cache, cancel/retry, backed stem mixer, original-mode immediate return and measured QA on a supported Windows machine. Gracefully support machines without a dedicated GPU. Document backend provenance and model assets.

### New M9: Release hardening

Test UI accessibility, long library lists, 100%/150%/200% DPI, audio modes, preset persistence, clipping protection, cancel/retry, missing model, corrupted cache, switching DACs with EQ on, exclusive-to-processed transitions, close during inference, clean-machine portable packaging and multi-hour uninterrupted original playback.

## 8. Concrete acceptance tests

| ID | Test | Expected result |
| --- | --- | --- |
| UX-01 | Launch at 1280x720 and 150% DPI | No clipped controls, player usable |
| UX-02 | Navigate keyboard-only to EQ and back | Every control has focus and accessible name |
| UX-03 | Open 10,000-track library | Virtualized list, responsive search and playback |
| EQ-01 | Select Flat | Transfer function approximates unity; no unintended gain |
| EQ-02 | Apply known +6 dB peak at 1 kHz | Frequency-response test matches expected magnitude within chosen test tolerance |
| EQ-03 | Adjust EQ during playback | No crashes, clicks or buffer underruns under reference workload |
| EQ-04 | Save user preset, restart app | Preset and all band parameters persist exactly |
| EQ-05 | EQ enabled under WASAPI exclusive | UI shows **Processed Audio — Not Bit-perfect** |
| EQ-06 | EQ bypass in strict Original mode | Reference PCM output bytes match expected decoded source (accounting for container packing) |
| EQ-07 | 44.1 to 192 kHz tracks | Filters recreated correctly and audio remains stable |
| STEM-01 | User presses `Prepare stems` | Model is invoked; progress and cancellation work |
| STEM-02 | Prepare an ordinary stereo FLAC | Vocals + accompaniment stem files cached and sample-aligned |
| STEM-03 | Select Vocals Focus / Instrumental Focus | Distinct audible outputs from model stems, not EQ tricks |
| STEM-04 | Move Vocals/Instrumental sliders | Mix changes audibly without restarting track |
| STEM-05 | Select Original Mix | Original source PCM path restored, not reconstructed stems |
| STEM-06 | Offline after model installation | Stem preparation works with networking disabled |
| STEM-07 | No model or insufficient memory | Clear error, normal playback unaffected |
| STEM-08 | Close app during separation | Worker cancelled/stopped; temporary cache cleaned safely |
| STEM-09 | Use stem modes with external DAC | Correct processed status, stable selected endpoint |
| QA-01 | Source and engine formats differ | UI labels `Source`, `Processing`, `Output` separately |
| QA-02 | Install portable release on clean Windows PC | Player launches and original FLAC playback works without dev environment |

For audio assertions use source/reference signals and PCM data captured before rendering; physical DAC accuracy remains independently testable and must not be claimed without evidence.

## 9. Agent execution instructions

1. Read the original HiResPlayer PRD, the current M0–M6 implementation plan, and this addendum. Treat this document as an **additive feature specification**.
2. Preserve the C# WPF + native C++ WASAPI architecture. Do not replace with a browser-only or simulated audio player.
3. Update the Antigravity implementation plan and task list to include UI changes plus M7–M9, with clear acceptance criteria and dependency order.
4. Build and test actual playback first, then DSP, then model inference. No fake equalizer, visualizer or vocal-isolation controls.
5. Keep Original / Bit-perfect-eligible and Enhanced Audio separate at the engine state-machine level and throughout the UI.
6. For every milestone, provide evidence: compiled artifacts, test output, screenshots and a concise verification report. Label hardware-unavailable tests `NOT VERIFIED`, never `PASS`.
7. If a maintained deployable source separation model cannot be validated, report the blocker and ship a truthful disabled feature instead of substituting ineffective center cancellation.
8. Do not download or redistribute unverified model assets without checking their license and provenance.

## 10. Technical sources and implementation references

- WPF Fluent/ThemeMode: https://learn.microsoft.com/en-us/dotnet/api/system.windows.application.thememode?view=windowsdesktop-10.0
- Windows accessibility guidelines: https://learn.microsoft.com/en-us/windows/apps/design/accessibility/designing-inclusive-software
- W3C Audio EQ Cookbook (filter math): https://www.w3.org/TR/audio-eq-cookbook
- Microsoft WASAPI exclusive mode: https://learn.microsoft.com/en-us/windows/win32/coreaudio/exclusive-mode-streams
- Demucs upstream project (archived, reference backend): https://github.com/facebookresearch/demucs
- Demucs code license: https://github.com/facebookresearch/demucs/blob/main/LICENSE
- ONNX Runtime Windows path and current WinML guidance: https://onnxruntime.ai/docs/get-started/with-windows.html
- Windows ML execution providers: https://learn.microsoft.com/en-us/windows/ai/new-windows-ml/supported-execution-providers

**Non-negotiable:** A beautiful player that does not play actual FLAC or a visually convincing EQ/vocal slider with no real processing behind it fails this specification.
