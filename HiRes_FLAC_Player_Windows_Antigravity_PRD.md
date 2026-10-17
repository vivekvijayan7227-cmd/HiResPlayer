# PRODUCT REQUIREMENTS DOCUMENT (PRD)
# HiResPlayer — Native Windows Lossless FLAC Music Player

**Document version:** 1.0  
**Prepared:** 13 October 2026  
**Status:** Build-ready implementation specification  
**Intended implementation agent:** Google Antigravity (Planning mode)  
**Platforms:** Windows 11 x64 (primary); Windows 10 x64 only as a separately validated compatibility target  
**Audio connections:** PC built-in 3.5 mm headphone/line output and external USB DAC with analog AUX output  
**Delivery objective:** A real, buildable, locally running Windows desktop application, not a prototype or static UI.

---

## 0. READ FIRST — EXECUTION CONTRACT FOR ANTIGRAVITY

**Your assignment:** Implement the Windows desktop product described in this PRD in the open workspace, proceeding incrementally from working audio to a polished application. This document is the primary source of requirements. Do not stop after producing mockups, plans, boilerplate, or a UI that pretends to play audio.

### Operating rules

1. **Inspect the environment first.** Determine the operating system, installed .NET SDK, MSVC/C++ tools, CMake, package manager, available sound devices, and existing repository structure. Document any missing toolchain or hardware; do not falsely report validation.
2. **Use Planning mode.** Produce an Antigravity implementation plan and task list mapped to the milestone IDs in §15. Review technical ambiguities against the official references in §20, then implement. Antigravity's review policy may require user approval of its generated implementation plan; honor that policy without repeatedly asking the user to reconfirm requirements already settled in this PRD.
3. **Deliver working vertical slices.** The first runnable milestone must audibly render a known PCM signal through WASAPI. Then decode real FLAC and render it. Only then invest in the full library interface.
4. **Make real builds.** Run actual build commands, automated tests, and available runtime tests. Fix errors. Check in project configuration, dependency manifests, and reproducible setup scripts. Never mark an implementation complete merely because the files exist.
5. **No fake integrations.** Do not hardcode fabricated DAC names, source sample rates, supported formats, bit-perfect claims, album metadata, or successful playback states into the production application. Illustrative fixtures are allowed only in explicitly named tests/design previews.
6. **No silent audio degradation.** Exclusive/bit-perfect requests must either open the exact supported PCM output path with DSP disabled or provide an explicit and visible error or opt-in fallback. Never silently resample while claiming bit-perfect output.
7. **No unsafe repository actions.** Do not delete user files, rewrite an unrelated existing repository, overwrite existing work without understanding changes, or commit secrets. Do not require cloud accounts, network access, or administrator privilege to play local files.
8. **Offline-first.** Actual audio playback and local library access must work without the internet. Network access may be used to obtain approved build dependencies, not to stream music or send user telemetry.
9. **Be precise about unverified hardware.** If no physical Windows audio endpoint, USB DAC, or GUI is available in the execution environment, implement and test all hardware-independent aspects, explicitly mark hardware QA **NOT VERIFIED**, and provide exact manual test steps. Never claim to have heard playback without performing that test.
10. **At each milestone:** update `docs/IMPLEMENTATION_STATUS.md` (done, pending, blocked), run relevant tests, and keep a short implementation walkthrough. Finish with `docs/VERIFICATION_REPORT.md`, actual build artifacts if feasible, known limitations, and usage instructions. Use Antigravity implementation-plan and walkthrough artifacts for review.

### Definition of delivered product

A successful delivery contains a compilable Windows solution; a functioning native FLAC/WASAPI engine; a responsive Windows UI; the local library and player experience; build/test scripts; third-party license notices; an app build (`.exe` and dependencies, or published folder); and honest verification evidence. A static page, audio simulation, source-only tree that has never compiled, or aspirational architecture alone is **not** acceptable.

### Suggested initial prompt to use alongside this file

> Read `HiRes_FLAC_Player_Windows_Antigravity_PRD.md` completely and implement it as the project specification. Begin with toolchain discovery and an implementation-plan artifact, then execute the milestones in order. Prioritize real WASAPI audio and real FLAC decoding over UI embellishment. Build, test, and verify each vertical slice. Record exact results and blockers in `docs/IMPLEMENTATION_STATUS.md` and `docs/VERIFICATION_REPORT.md`. Do not claim bit-perfect output without proving the application-side conditions, and do not claim physical DAC verification without appropriate hardware measurements.

---

## 1. Product overview

### 1.1 Product vision

**HiResPlayer** is a premium-feeling, fully offline Windows music player designed for users who own local high-resolution FLAC recordings and listen through an analog AUX cable connected either to their PC's built-in audio jack or to an external USB DAC.

Its distinguishing promise is **transparent playback-chain control**: users can see the source format, choose the real Windows output endpoint, select shared or exclusive playback, and know when the original PCM is being preserved at the application's output boundary.

### 1.2 Problem statement

Conventional media players often use the Windows shared audio mixer, whose configuration can differ from the FLAC source rate. Users may not know if audio is being resampled, whether a USB DAC is actually selected, or why another application is interfering with exclusive playback. HiResPlayer removes those ambiguities while remaining simple enough for everyday listening.

### 1.3 Primary user stories

- **US-01:** As a music listener, I want to select folders containing `.flac` files so my music library can be indexed automatically.
- **US-02:** As a listener, I want reliable play, pause, stop, seek, next, previous, queue, shuffle and repeat controls.
- **US-03:** As a headphone/AUX user, I want to choose the PC's built-in endpoint as the output device.
- **US-04:** As an external DAC user, I want to select my actual USB DAC endpoint instead of Windows' default endpoint.
- **US-05:** As an audiophile, I want an exclusive-mode option that plays supported FLAC audio at the original PCM rate and precision without software DSP.
- **US-06:** As a listener, I want to see the source and negotiated output formats, playback mode, and any fallback or failure.
- **US-07:** As a laptop user, I want smooth playback when the screen changes state or another application is busy.
- **US-08:** As a device-switching user, I want a controlled response when a DAC is disconnected and an understandable way to resume.
- **US-09:** As a privacy-conscious user, I want local playback without an account, internet connection, or telemetry.

### 1.4 Explicitly out of scope for v1

Music streaming services; account registration; cloud storage/sync; mobile apps; DSD/DoP; proprietary ASIO drivers; DRM decoding; CD ripping; lyrics fetching; AI recommendations; advanced DSP/equalizers; multi-device synchronized playback; network audio endpoints; remote control apps; automatic volume normalization; crossfade. These must not delay the MVP.

---

## 2. Scope and release tiers

### P0 — Must ship in v1.0

- Windows 11 x64 graphical application; reproducible clean build and packaging.
- Open individual FLAC files and index one or more local folders recursively.
- Parse title, artist, album, track number, duration, sample rate, precision, channel count and embedded artwork when present.
- Library views: Tracks, Albums, Artists; search and file location.
- Playback: play, pause, stop, seek, next/previous, ordered queue, shuffle, repeat off/one/all.
- Actual FLAC decoding with libFLAC; real stereo PCM audio rendering.
- WASAPI shared-mode rendering with compatibility-oriented format conversion where needed.
- WASAPI exclusive-mode rendering when the exact compatible source format is accepted by the endpoint.
- A selector listing real Windows render endpoints, including built-in and USB DACs where present.
- Output diagnostics: source codec and PCM format, device identity, selected playback mode, requested/actual stream format, underrun count, failure reason.
- A visible distinction between **Shared**, **Exclusive / bit-perfect eligible**, **Format converted**, and **Unavailable**.
- Proper error-handling for file errors, device busy/removed, unsupported formats, decoder errors, sleep/resume.
- Local SQLite database, persistent folder locations, queue and preferences.
- No online dependency for daily playback.
- Automated unit tests and hardware QA checklist; complete build/developer README.

### P1 — Required before declaring v1.0 polished, but after working P0 audio

- Gapless playback for consecutive compatible files with identical output format.
- Media keyboard controls and standard Windows media integration if feasible without compromising stability.
- Fast incremental library rescans and missing-drive handling.
- Strong keyboard navigation, accessibility names/focus handling, high-DPI scaling, dark theme and clean empty/error states.
- Portable published release bundle and third-party notices.

### P2 — Later optional releases

- Enhanced waveform/spectrum analysis (only when not on the real-time audio thread).
- ReplayGain or optional EQ in non-bit-perfect mode.
- ReplayGain-aware queue options, richer sorting, playlists import/export, alternative skins.
- Wider formats such as WAV/AIFF/ALAC and ASIO where a genuine use case arises.

---

## 3. Target platform, dependencies and constraints

### 3.1 Reference stack

| Layer | Technology | Reason |
|---|---|---|
| Desktop application | C#  / WPF on .NET 10 LTS | Mature native Windows UI, data binding, theming and deployment |
| UI pattern | MVVM | Testable separation from audio/OS APIs |
| Native audio engine | C++20 DLL, MSVC toolchain | Direct control over COM, audio buffers and threads |
| Native build | CMake with committed dependency manifest | Reproducible dependency handling |
| Codec | libFLAC, Xiph reference library | Lossless decoder with metadata and seeking |
| Output | Windows WASAPI / Core Audio COM | Selectable audio endpoint, shared/exclusive streams |
| Device notifications | MMDevice / IMMNotificationClient | Hotplug/default endpoint detection |
| Local persistence | SQLite | File library, playlists/queue and settings |
| Interop | `extern "C"` native exports + P/Invoke | Stable versioned boundary between native and managed layers |
| Tests | C++ tests + .NET tests + Windows hardware smoke tests | Separate correctness from hardware-dependent results |
| Packaging | Windows x64 portable publish initially | Allows testing before optional installer/signing |

Use **Visual Studio 2026** with C++ Desktop and .NET Desktop workloads (or equivalent compatible SDK/build tools) and the Windows SDK. When using command-line builds, detect tools instead of assuming a hardcoded installation path. Document exact versions tested. Do not arbitrarily replace the stack with Electron, a browser audio element or a third-party player wrapper.

### 3.2 Audio support boundaries

- Initial supported sources: local **stereo FLAC**, integer PCM samples at **16-bit and 24-bit** precision, with common sample rates **44.1, 48, 88.2, 96, 176.4 and 192 kHz**.
- Mono/multichannel tracks or other bit depths may be shown in the library but should produce an explicit understandable unsupported-format message if not implemented correctly.
- Hardware must individually support requested exclusive PCM combinations; the app cannot force a DAC to operate at 192 kHz.
- AUX is **analog output**. FLAC/PCM are digital before the chosen DAC. Do not call the AUX cable a digital or FLAC interface.
- Windows detects output endpoints. It typically **cannot prove that a physical AUX cable is inserted** into a separate USB DAC, and internal speakers/headphone sockets may share a logical endpoint.
- A consumer PC may advertise driver capabilities that do not establish true DAC quality. Report what was negotiated, not unobservable claims.

### 3.3 Windows 10 note

The previous concept mentioned Windows 10/11. Implement and test **Windows 11 x64 first**. Do not declare Windows 10 support until it has been explicitly compiled and exercised there. The product must not rely on unsupported Windows versions for its validation story.

---

## 4. User experience and product design

### 4.1 Design principles

- Clean dark-mode aesthetic; legible typography; strong contrast; responsive window resizing; minimal decoration.
- Playback status and output routing are more important than flashy animated visualizers.
- App remains usable offline on small laptop screens and scales with Windows DPI settings.
- No unexplained status icons; all failures include specific actionable text.
- Artwork is optional; use a tasteful placeholder for files without embedded artwork.

### 4.2 App navigation

**Left sidebar:** Library, Albums, Artists, Folders, Playlists (if completed), Settings.  
**Main area:** Search, filtered track/album grid or table.  
**Bottom player:** Artwork thumbnail, track title/artist, elapsed/total time, seek bar, previous/play-pause/next, shuffle/repeat, current output device, quick diagnostics link.  
**Now Playing view:** Large artwork, queue, complete file properties, output signal details.  
**Settings / Audio Output:** Enumerated devices and modes, format/fallback policy, real-time diagnostics, device capabilities.

### 4.3 Key screens and behavior

**Library screen**
- User clicks **Add Music Folder** and grants access to an actual local filesystem folder.
- Scanner recursively identifies `.flac` files; update progress and allow cancellation.
- Library stores track path and metadata, not the complete audio file.
- Search matches title, artist, album and filename.
- Removed/unavailable drives show unavailable files without destroying saved queue/library entries.

**Now Playing screen**
- Play/pause/stop/seek controls act on native engine state, not a simulated timer.
- Show source `FLAC · 24-bit · 96 kHz` only when extracted from the actual file.
- Show actual active endpoint name and actual initialized WASAPI stream format.
- Display `Exclusive / bit-perfect eligible` **only when** all software-side bit-perfect conditions pass (§8); do not simply equate selected radio button with successful exclusive playback.
- Playback progress reflects the actual rendered position, including seeks and pauses.

**Audio settings**
- Device dropdown built from Windows endpoint enumeration; shows friendly name and active/unavailable state.
- Mode: `Shared (compatible)` or `Exclusive (source-preserving when supported)`.
- Default selected mode: **Shared**, to avoid surprise endpoint monopolization.
- Default fallback policy for strict exclusive requests: **Ask / report error**, not silently convert.
- Expose source rate, initialized output rate/bit configuration, DSP state and device event/error status.
- Offer **Switch Device** and **Retry** after USB removal or exclusive conflict.
- In strict bit-perfect mode, no software volume slider that scales PCM samples; show explanatory text that analog amplifier/DAC volume is preferred.

### 4.4 UI states

`Empty Library`, `Scanning`, `Ready`, `Loading Track`, `Buffering`, `Playing`, `Paused`, `Seeking`, `Switching Device`, `Playback Complete`, `Recoverable Error`, `Unsupported Format`, `Output Disconnected`. Clearly differentiate file errors and device errors.

---

## 5. Functional requirements with IDs and acceptance criteria

### FR-01 — Local library scanning (P0)
**Behavior:** Add/remove folders, recursive scan, duplicate-path suppression, cancellation, metadata indexing, incremental rescan on launch and on request.  
**Acceptance:** A folder with 1,000 mixed audio/non-audio files indexes `.flac` tracks without UI freezes; already indexed tracks are not duplicated; inaccessible files are reported and skipped safely.

### FR-02 — FLAC metadata (P0)
**Behavior:** Read STREAMINFO and common Vorbis comment tags, artwork if present, channel count, valid bits, sample rate, exact duration from total samples if available.  
**Acceptance:** The sample rate/precision shown for supplied reference files matches reference metadata; missing tags fall back to safe filename/unknown placeholders; malformed metadata cannot crash the app.

### FR-03 — Playback controls (P0)
**Behavior:** Play, pause, stop, next, previous, seek; shuffle and repeat modes; queue with deterministic advance.  
**Acceptance:** State and rendered audio stay aligned; seeking flushes stale buffers and resumes near requested sample position; no old audio is replayed after a seek.

### FR-04 — FLAC decoder (P0)
**Behavior:** Decode via official libFLAC streaming callbacks; preserve source PCM values; support proper file closure and cancellation.  
**Acceptance:** Reference FLAC files decode to expected PCM bytes/sample values after canonicalization; errors are surfaced; rapid track changes don't leak handles.

### FR-05 — WASAPI shared output (P0)
**Behavior:** Render to selected endpoint using shared-mode WASAPI, selecting compatible mix format and converting/resampling only as needed.  
**Acceptance:** Audible output on a real Windows endpoint in a smoke test; mismatched source/mix rates are handled and truthfully labeled `Format converted`; UI remains responsive.

### FR-06 — WASAPI exclusive output (P0)
**Behavior:** Query exact PCM configurations and initialize an exclusive event-driven stream when supported.  
**Acceptance:** Compatible sample-rate/precision is preserved without software effects; unsupported/busy endpoints produce informative errors; no false bit-perfect status.

### FR-07 — Built-in output and external USB DAC (P0)
**Behavior:** Enumerate all render endpoints and allow manual selection; remember preferred endpoint ID when possible.  
**Acceptance:** Endpoint list represents actual Windows devices; changing selected endpoint recreates stream safely; if the preferred endpoint vanishes, the app does not silently change output without explanation.

### FR-08 — Device interruptions (P0)
**Behavior:** Respond to device removal, state changes, default-device changes, sleep/resume, and exclusive conflicts.  
**Acceptance:** No process crash; current queue and position are retained where possible; an explicit restart/switch option is provided.

### FR-09 — Audio diagnostics (P0)
**Behavior:** Report codec, bit depth, source sample rate, initialized stream format, endpoint identifier/name, exclusive/shared status, number of underruns, last error.  
**Acceptance:** Values come from decoder and OS APIs, are updated when tracks/devices change, and never claim hardware bit-perfect proof.

### FR-10 — Local preferences and queue (P0)
**Behavior:** Save indexed folders, selected mode, optional endpoint preference, queue position and sorting preferences locally.  
**Acceptance:** Safe application restart restores usable preferences and library; corrupt or missing database is handled gracefully with backup/recovery instructions.

### FR-11 — Same-format gapless playback (P1)
**Behavior:** Predecode upcoming queue track and render contiguous PCM for compatible consecutive tracks at the same output format, without inserted silence.  
**Acceptance:** A generated two-file continuous test signal crosses the track boundary without an inserted gap or repeated samples. Different-format tracks may require a stream restart; document the resulting limitation.

### FR-12 — Keyboard, accessibility and polish (P1)
**Behavior:** Space for play/pause when focus allows; obvious keyboard navigation; labels for all important controls; responsive scaling, functional missing-artwork fallback.  
**Acceptance:** Core tasks can be performed with keyboard navigation, and the UI remains usable at 125% and 150% DPI and compact window size.

---

## 6. System architecture

### 6.1 Logical pipeline

```text
[WPF Desktop UI]
  -> [MVVM / Application Services]
  -> [Managed AudioFacade]
  -> [P/Invoke / stable C ABI]
  -> [Native C++ Playback Coordinator]
  -> [libFLAC Reader + Decoder]
  -> [PCM Converter + Preallocated Ring Buffer]
  -> [Format Negotiation + Optional Shared-Mode Resampler]
  -> [WASAPI Renderer]
  -> [Chosen Windows Render Endpoint]
       -> Internal DAC -> 3.5 mm AUX -> amplifier/speakers
       OR
       -> USB audio driver -> External DAC -> 3.5 mm AUX -> amplifier/speakers
```

Independent paths:

- `FolderScanner -> MetadataExtractor -> SQLite -> LibraryViewModel`.
- `IMMDeviceEnumerator + IMMNotificationClient -> DeviceManager -> OutputViewModel`.
- `Native Engine Status/Event Queue -> Managed Dispatcher -> PlaybackViewModel`.

### 6.2 Managed modules

| Module | Responsibility |
|---|---|
| `HiResPlayer.Desktop` | WPF windows, controls, themes, navigation, keyboard actions |
| `HiResPlayer.Application` | Playback commands, queue, settings, library services, error mapping |
| `HiResPlayer.Infrastructure` | File scanning, SQLite, metadata ingestion, native interop |
| `HiResAudioEngine` | C++ DLL for decoder, PCM, endpoint selection, WASAPI and diagnostics |
| `HiResPlayer.Tests` | Application tests, fake audio facade tests |
| `HiResAudioEngine.Tests` | Decoder, PCM conversion, ring-buffer and engine tests |

### 6.3 Required core C++ classes

- `FlacDecoder`: streaming `libFLAC`, metadata, sample callbacks, accurate sample-based seek.
- `PcmFormat`: rate, channel count, sample precision, container bits, channel mask, frame layout.
- `PcmSamplePacker`: interleave channels and pack 16-bit or 24-bit PCM into device-required integer layout.
- `PcmRingBuffer`: bounded single-producer/single-consumer preallocated buffer; expose occupancy, underflow stats and reset generations.
- `AudioFormatNegotiator`: choose validated shared mix or exclusive exact formats; produce a format result and reasons.
- `AudioResampler`: **only for non-bit-perfect shared-mode playback** where needed; use a tested implementation with explicit licenses; never attempt naive frame dropping/duplicating as the production resampler.
- `WasapiRenderer`: open stream, configure event, acquire render service, write endpoint buffer, start/stop/release.
- `AudioDeviceManager`: endpoint list, IDs, hotplug callbacks and capability queries.
- `PlaybackController`: commands, track/queue transitions, seek/restart, state machine.
- `AudioDiagnostics`: actual output characteristics, error codes, underflow counters and stream state.

### 6.4 Dependency direction

UI depends on application interfaces; application depends on infrastructure abstractions; native engine is loaded through a single interop facade. **Do not** call WASAPI from WPF controls or let native rendering depend on database/UI code.

---

## 7. Windows audio engineering requirements

### 7.1 Playback modes

**Shared:** Use WASAPI shared, usually endpoint mix format from `IAudioClient::GetMixFormat`; resampling/sample conversion may occur. Windows may mix/modify output. Report non-bit-perfect status unless proven otherwise, and do not imply source sample rate necessarily equals endpoint mix rate.

**Exclusive:** Use `IAudioClient::IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, ...)` for source-rate stereo integer PCM alternatives, open only a supported exact logical format, and bypass app-level resampling/DSP. The endpoint may be monopolized; device drivers vary.

### 7.2 Format negotiation

1. Read FLAC STREAMINFO (sample rate, valid bits, channels).
2. Determine selected endpoint and requested mode.
3. For shared playback, get mix format and select a supported conversion path.
4. For exclusive playback, generate valid candidate `WAVEFORMATEX`/`WAVEFORMATEXTENSIBLE` configurations matching **same source sample rate and valid PCM precision**. Driver compatibility varies: for 1–2 channel PCM, Microsoft advises trying both descriptors when appropriate.
5. Support 24-bit samples in a 24-bit packed container **or** correctly represented as 24 valid bits in a 32-bit integer container. Pack/alignment and zero padding must be verified by unit vectors; valid bits and container bits are distinct. Do not claim a 32-bit output container creates 32 bits of source precision.
6. Call `IsFormatSupported`, initialize the chosen stream and read/record actual configuration and errors.
7. If exact exclusive output cannot initialize, return a structured error and opt-in fallback option; do not automatically relabel a resampled shared stream as exclusive.
8. Re-negotiate and recreate the stream when track format/device changes.

### 7.3 Event-driven renderer

- Use `IAudioClient` and `IAudioRenderClient`; configure `AUDCLNT_STREAMFLAGS_EVENTCALLBACK` and `SetEventHandle` where applicable.
- Run render thread with Windows MMCSS priority (`AvSetMmThreadCharacteristics`), but use a sensible audio task category and preserve system stability.
- `WaitForMultipleObjects` (or a robust equivalent) should react to render-ready and shutdown/control signals.
- Use correct buffer/padding arithmetic for the actual WASAPI mode, and honor exclusive event-driven constraints.
- Handle `AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED` according to the Microsoft initialization guidance by using the aligned buffer size and recreating the client as required.
- Ensure COM initialization/lifetime is correct on each thread that uses COM interfaces.
- Avoid heap allocations, I/O, blocking locks, database calls and UI callbacks inside real-time render operations.
- Protect against integer overflow in byte calculations: bytes per frame = channels * container bytes per sample.

### 7.4 Decoder and buffering threads

- Decode on worker thread; render from a preallocated PCM queue on separate MMCSS thread.
- Start with about **500–1000 ms** of decoded read-ahead; actual endpoint buffer sizes must be negotiated, not hardcoded.
- Surface all underflows as counters, and recover without busy-spinning or crashing.
- For seeking, bump a buffer-generation counter, flush queued stale data, reset/reposition decoder and render timeline, then re-prime before resuming.
- Avoid race conditions between `Seek`, `Stop`, next track, and switching output devices. Serialize lifecycle commands on a control queue.
- Represent playback progress from accepted/rendered frame position and, where useful, `IAudioClock` with `GetFrequency`; never assume its position unit is always samples.
- Stop/close all resources deterministically; no lingering exclusive lock when paused/stopped longer than necessary per the chosen policy.

### 7.5 Device manager

- Use `IMMDeviceEnumerator::EnumAudioEndpoints(eRender, ...)` and friendly endpoint properties.
- Register an `IMMNotificationClient`; keep callbacks short and marshal notifications to the control thread.
- Register/unregister callbacks outside their callback implementations; avoid releasing final device object references in callbacks.
- Track explicit endpoint IDs and `active`, `disabled`, `unplugged`, `not present` states.
- A USB DAC unplug during playback triggers pause/error and a choice to switch or retry; don't pretend playback continues.
- Remember a manually selected DAC, but gracefully handle changed endpoint identifiers when Windows reconnects it.
- First release supports one active output endpoint. **Do not** try to keep two DAC clocks synchronized.

---

## 8. Bit-perfect integrity and status policy

**Definition used in the app:** `Bit-perfect eligible` means the program decoded losslessly and passed logically equivalent source PCM samples to a successfully initialized exclusive WASAPI output stream at the original sample rate/channels/precision, with sample-changing DSP disabled. It **does not assert** that the output DAC physically emitted an unchanged digital bitstream or that the analog waveform is perfect.

All the following conditions are necessary for `Bit-perfect eligible` status:

- FLAC frames decoded correctly with no concealment or decoder error.
- Chosen output sample rate equals source sample rate.
- Chosen output sample precision equals source valid precision; representation conversions are lossless and unit-tested.
- Same intended channel mapping, no channel mix/downmix.
- Exclusive WASAPI mode successfully initialized and rendering (not merely selected in UI).
- Resampler, software volume multiplication, ReplayGain, EQ, normalizer, crossfade and dithering disabled.
- App passes the expected PCM samples into `IAudioRenderClient`, modulo explicitly specified lossless container packing.

Status messages must be one of:

- `Shared mode — Windows mixer in use`
- `Exclusive — source format preserved (bit-perfect eligible)`
- `Exclusive unavailable — unsupported format`
- `Exclusive unavailable — device in use / access denied`
- `Playback using converted format — not bit-perfect`
- `Device disconnected / stream interrupted`

**Never display** unqualified `BIT PERFECT VERIFIED` without an independent digital path verification. Log the exact Windows HRESULT and translate it into a user-friendly message. User-facing terms should not confuse file losslessness with physical DAC fidelity.

---

## 9. Native C ABI and managed interop contract

C++ exports should use `extern "C"`, explicit calling convention, fixed-width integers, opaque handles, UTF-16 Windows strings, and versioned structures to avoid ABI ambiguity.

**Proposed interface (refine names during implementation, preserve semantics):**

```cpp
#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define HP_API __declspec(dllexport)
#define HP_CALL __cdecl
#endif

typedef void* HP_Handle;

typedef enum HP_Result {
    HP_OK = 0,
    HP_INVALID_ARGUMENT = 1,
    HP_NOT_READY = 2,
    HP_FILE_ERROR = 3,
    HP_UNSUPPORTED_SOURCE = 4,
    HP_UNSUPPORTED_OUTPUT_FORMAT = 5,
    HP_DEVICE_UNAVAILABLE = 6,
    HP_DEVICE_BUSY = 7,
    HP_DECODER_ERROR = 8,
    HP_INTERNAL_ERROR = 9
} HP_Result;

typedef enum HP_PlaybackMode {
    HP_SHARED = 0,
    HP_EXCLUSIVE = 1
} HP_PlaybackMode;

typedef struct HP_StatusV1 {
    uint32_t structure_size;
    uint32_t state;
    uint32_t source_rate;
    uint32_t source_valid_bits;
    uint32_t source_channels;
    uint32_t output_rate;
    uint32_t output_valid_bits;
    uint32_t output_container_bits;
    uint32_t output_channels;
    uint32_t actual_mode;
    uint32_t bitperfect_eligible;
    uint64_t rendered_source_samples;
    uint64_t underrun_count;
    int32_t  last_hresult;
} HP_StatusV1;

HP_API uint32_t HP_CALL hp_api_version(void);
HP_API HP_Handle HP_CALL hp_create(void);
HP_API HP_Result HP_CALL hp_open_flac(HP_Handle, const wchar_t* path);
HP_API HP_Result HP_CALL hp_select_device(HP_Handle, const wchar_t* endpoint_id);
HP_API HP_Result HP_CALL hp_set_mode(HP_Handle, HP_PlaybackMode);
HP_API HP_Result HP_CALL hp_play(HP_Handle);
HP_API HP_Result HP_CALL hp_pause(HP_Handle);
HP_API HP_Result HP_CALL hp_stop(HP_Handle);
HP_API HP_Result HP_CALL hp_seek_sample(HP_Handle, uint64_t position);
HP_API HP_Result HP_CALL hp_get_status(HP_Handle, HP_StatusV1*);
HP_API void HP_CALL hp_destroy(HP_Handle);

#ifdef __cplusplus
}
#endif
```

### API expectations

- Add device enumeration via explicit native functions with **caller-allocated buffers or query-length/two-pass calls**; define memory ownership clearly. Returning pointers to temporary native memory is prohibited.
- `hp_api_version()` allows the managed app to reject incompatible DLL versions.
- All public calls return deterministic status codes. Native exceptions must never cross the C ABI.
- Dispatch hardware/device events using a dedicated event queue or safe callback channel; don't invoke WPF view-model code from render callbacks.
- Build x64 managed and native binaries; detect architecture/DLL load mismatch and display actionable errors.
- For P/Invoke, use explicit `[LibraryImport]`/`[DllImport]` with `CallingConvention.Cdecl`, Unicode marshaling and exact struct layout. Verify ABI with interop tests.
- The C++ header is the contract of record; keep matching managed structs/tests synchronized.

### Playback state machine

```text
Uninitialized -> Ready -> Loading -> Primed -> Playing
                                     <-> Paused
Playing/Paused -> Seeking -> Primed/Playing-or-Paused
Playing/Paused -> SwitchingDevice -> Primed/Paused-or-Playing
Any active state -> RecoverableError -> Ready/Primed
Any active state -> Stopped -> Ready
Any state -> Disposed (terminal)
```

Every command must specify legal transitions and return an error for an illegal state. Use a single serialized controller to prevent concurrent WASAPI reinitializations.

---

## 10. Data and persistence design

### Suggested SQLite tables

**`LibraryFolders`**: `Id`, `Path`, `AddedUtc`, `LastScannedUtc`, `IsAvailable`.  
**`Tracks`**: `Id`, `FilePath` (unique normalized path), `FileSize`, `ModifiedUtc`, `Title`, `Artist`, `AlbumArtist`, `Album`, `TrackNumber`, `DurationSamples`, `SampleRate`, `BitsPerSample`, `Channels`, `ArtworkCachePath`, `Unavailable`.  
**`Albums`**: Optional normalized album group derived from tags, with stable artist+album handling.  
**`Playlists`** and **`PlaylistTracks`**: If playlists are implemented for v1.  
**`QueueEntries`**: `Position`, `TrackId`.  
**`Preferences`**: key/value for selected mode, endpoint ID, repeat/shuffle and general UI settings.

### Rules

- Use parameterized SQL queries; prevent malformed tags from being treated as SQL.
- Keep user audio files unchanged; scanning and playback are read-only.
- Use transactions for batch index updates and migrations.
- Maintain DB migration version with tests.
- Detect changes using path + file metadata; avoid expensive re-decoding on every launch.
- Never block the audio-rendering thread for artwork decoding or DB access.
- Preserve queue references when removable storage is temporarily unavailable.
- Cache thumbnails locally with bounded cache size and graceful cleanup.

---

## 11. Errors and recovery requirements

| Condition | Engine behavior | User-visible behavior |
|---|---|---|
| Missing FLAC file | Fail file open; preserve queue | `File not found`, locate/skip options |
| Corrupted FLAC | Stop affected track; free decoder | `Could not decode file`, skip option |
| Unsupported FLAC format | No incorrect coercion | Display exact source properties and limitation |
| Exclusive endpoint busy | Preserve playback state | `Device is in use; switch to Shared or retry` |
| Unsupported exact rate/precision | No silent resampling in strict mode | `DAC does not support this exact format` |
| USB DAC unplugged | Stop rendering, release COM objects | `Audio device disconnected`; retry/select |
| WASAPI service reset | Recover or return structured failure | Brief notice with retry, no crash |
| Sleep and resume | Re-enumerate/reopen safely | Resume action if automatic recovery unsafe |
| Long scan or inaccessible folder | Background task, cancellable | Progress and skipped-file count |
| Metadata/artwork failure | Keep playable track | Filename placeholder; no crash |
| DLL missing or wrong CPU architecture | Fail startup with explanation | Show exact dependency/build guidance |

Every error must have an internal code and a useful, human-readable message; don't show raw HRESULT as the *only* explanation.

---

## 12. Nonfunctional requirements

### Performance

- UI interaction remains responsive during library scanning, file decoding and output reconfiguration; no synchronous disk scans on the UI dispatcher.
- Playback startup target: **under 2 seconds after choosing a local file on a normal SSD**, excluding first-use library scanning and unusual device initialization delays; measure on a recorded reference machine.
- Decode buffer target: **0.5–1.0 seconds** of ahead-of-playback PCM, with adjustable implementation tuning.
- Rendering must be event-driven, avoid polling/busy loops, and normally achieve **zero software buffer underruns in a 60-minute sustained playback test** on a reference PC with healthy local storage.
- With 10,000 indexed tracks, search and scroll must remain responsive through virtualization and indexed DB access.
- No unbounded growth in queued PCM, waveform/artwork caches, native handles or COM references across repeated playback changes.

### Quality and accessibility

- Correct artist/album typography and accessible contrast in dark UI.
- Progress bar, focus state and keyboard access available at 100%, 125% and 150% Windows DPI scaling.
- No audio glitches caused by album artwork, animations, logging or database activity.
- Logging is structured and rate-limited; avoid full filesystem paths in diagnostic exports unless the user opts in.

### Privacy and security

- No sign-in or telemetry; no sending of music tags, playlists or file paths to external endpoints.
- Source files opened read-only.
- Handle arbitrary malformed FLAC metadata using supported library APIs; avoid unsafe string handling/length arithmetic.
- Validate DLL/package integrity and document third-party licenses and versions.
- The built app must not require administrator rights for normal playback.

### Reliability

- Errors should not permanently hold endpoint exclusive locks.
- Repeated play/pause/seek/device changes are race-free and recover without process restart.
- Gracefully close native and managed resources on exit.

---

## 13. Testing and validation plan

### 13.1 Automated tests (required)

**Codec/PCM tests:**
- Decode known 16/44.1, 24/48, 24/96 and 24/192 stereo FLAC fixtures where available; compare canonical samples against reference PCM output.
- Validate interleaving, signed sample handling, endianness, 24-bit packed PCM, and 24 valid bits in a 32-bit container with known test vectors (including negative/full-scale/min samples).
- Verify source sample rate and precision are never mislabeled as the output format.
- Test truncated/malformed files, missing tags, empty metadata, seek to sample 0/middle/end.

**Audio buffer/engine tests:**
- SPSC buffer wraparound, full/empty conditions, reset-generation behavior, concurrent producer/consumer stress, underrun count and cleanup.
- State-machine legal/illegal transitions; concurrent command serialization.
- Format negotiator using mocked endpoint capability results: exact supported, unsupported, exclusive busy, shared fallback approval.
- Gapless (P1): split continuous PCM into two source files and verify no extra frames inserted for same-format playback.

**Managed/UI tests:**
- DB migrations, metadata/path deduplication, folder rescans, missing drive behavior, queue, repeat/shuffle, error messages and testable view models.
- ABI layout and version checks; missing native DLL reporting.
- Basic keyboard and binding behavior.

### 13.2 Real Windows hardware tests (required for verified release)

**Hardware matrix:**
1. Windows 11 reference PC, internal audio endpoint + wired headphones/speakers or 3.5 mm AUX to an amplifier.
2. Windows 11 reference PC, external USB DAC + analog 3.5 mm AUX to the same amplifier, if physical hardware is available.
3. At least one DAC with independently documented capability for 24-bit/96 kHz; 24/192 if available.

**Scenarios:**
- Shared playback audible for 16/44.1.
- Exact exclusive configuration accepted for a supported high-resolution file; verify actual initialized stream parameters.
- Unsupported rate and busy endpoint display correct error and do not claim bit-perfect.
- Switch internal-to-USB and USB-to-internal while stopped and while playing.
- Unplug/reconnect USB during playback; verify controlled recovery and no crash.
- Seek many times; play 60 minutes; pause/resume; play queue to end; sleep/resume.
- Compare recorded source metadata with displayed device/output state.
- If genuine bit-perfect hardware proof is required, use a suitable digital loopback/capture or vendor-specific diagnostic path. **Do not** claim physical bit-perfect verification based solely on analog AUX recording, a UI label or a successful HRESULT.

### 13.3 Acceptance matrix

| Test ID | Test | Passing result |
|---|---|---|
| QA-01 | Build from clean checkout on Windows | Native DLL and WPF executable build with documented commands |
| QA-02 | FLAC decode integrity | Canonical PCM matches reference vectors |
| QA-03 | Shared output | Real selected device renders music with no crash |
| QA-04 | Exclusive 16/44.1 | Exact supported stream opens, no software DSP |
| QA-05 | Exclusive 24/96 | Exact stream opens when device supports it |
| QA-06 | Exclusive 24/192 | Exact stream opens when device supports it; otherwise clear error |
| QA-07 | Internal output selection | Audio routed to real selected internal endpoint |
| QA-08 | USB DAC selection | Audio routed to real selected external endpoint |
| QA-09 | Device unplug | Controlled transition and error; no crash |
| QA-10 | Unsupported/busy exclusive | No silent shared fallback or false label |
| QA-11 | Seek/next/previous | Output and progress follow expected source position |
| QA-12 | Library scanner | Duplicate-free responsive indexing and metadata correctness |
| QA-13 | Playback stability | 60 min without unexpected underrun on reference hardware |
| QA-14 | Offline use | App plays local FLAC with networking disabled |
| QA-15 | Packaged app | Exe and dependencies launch from published folder |
| QA-16 | Same-format gapless (P1) | No inserted/repeated PCM frames at boundary |

Mark results `PASS`, `FAIL`, `BLOCKED`, or `NOT RUN`. Include date, machine/environment and evidence. Never substitute automated mock tests for real hardware results.

---

## 14. Proposed repository and output layout

```text
HiResPlayer/
|-- HiResPlayer.sln
|-- README.md
|-- LICENSE-or-PROJECT-LICENSE.md
|-- THIRD_PARTY_NOTICES.md
|-- CMakePresets.json
|-- vcpkg.json                     # if vcpkg chosen
|-- src/
|   |-- HiResPlayer.Desktop/
|   |   |-- App.xaml
|   |   |-- MainWindow.xaml
|   |   |-- Views/
|   |   |-- ViewModels/
|   |   |-- Controls/
|   |   `-- Styles/
|   |-- HiResPlayer.Application/
|   |   |-- Playback/
|   |   |-- Library/
|   |   |-- Queue/
|   |   `-- Interfaces/
|   |-- HiResPlayer.Infrastructure/
|   |   |-- Persistence/
|   |   |-- Scanning/
|   |   |-- Metadata/
|   |   `-- NativeInterop/
|   `-- HiResAudioEngine/
|       |-- include/hires_audio_api.h
|       |-- src/FlacDecoder.cpp
|       |-- src/PcmSamplePacker.cpp
|       |-- src/PcmRingBuffer.cpp
|       |-- src/AudioFormatNegotiator.cpp
|       |-- src/WasapiRenderer.cpp
|       |-- src/AudioDeviceManager.cpp
|       |-- src/PlaybackController.cpp
|       |-- src/AudioDiagnostics.cpp
|       `-- CMakeLists.txt
|-- tests/
|   |-- NativeTests/
|   |-- ManagedTests/
|   `-- Fixtures/
|-- scripts/
|   |-- check-prerequisites.ps1
|   |-- build.ps1
|   |-- test.ps1
|   `-- publish.ps1
|-- docs/
|   |-- ARCHITECTURE.md
|   |-- IMPLEMENTATION_STATUS.md
|   |-- VERIFICATION_REPORT.md
|   |-- MANUAL_HARDWARE_QA.md
|   `-- KNOWN_LIMITATIONS.md
`-- dist/                          # generated; excluded from source control
```

Avoid committing generated compilation outputs, large copyrighted music tracks or absolute user file paths. Generate small original/test-tone FLAC fixtures using a reproducible script or a properly licensed fixture set and document provenance.

---

## 15. Antigravity implementation milestones

### M0 — Environment inspection and plan
**Work:** Check toolchain; inspect repo; record baseline; choose dependency versions; generate implementation-plan artifact and task list; create skeleton docs and build scripts.  
**Deliverables:** `docs/ARCHITECTURE.md`, `docs/IMPLEMENTATION_STATUS.md`, baseline build configuration.  
**Exit criteria:** All dependencies known and documented; build toolchain either confirmed or listed as blocker.  
**Never claim:** application already works before audio renderer is tested.

### M1 — Native WASAPI proof of concept
**Work:** Enumerate endpoints, choose real output, render a generated stereo PCM sine-wave/test tone through shared WASAPI, then test exclusive path where available. Apply event-driven buffering, MMCSS and robust COM cleanup.  
**Deliverables:** Compilable native engine library and a small command-line smoke-test harness.  
**Exit criteria:** Working shared-mode stream or, if no Windows output device is available, tested format-setup logic and explicitly blocked physical test.  
**Priority:** Highest; do before comprehensive WPF styling.

### M2 — Real libFLAC decoding and sample correctness
**Work:** Integrate Xiph libFLAC, implement metadata and seek, PCM packing, ring buffer, source-to-output pipeline and automated sample vectors.  
**Deliverables:** Actual local FLAC playback from native smoke-test harness.  
**Exit criteria:** Correct PCM reference output and working song playback on a real endpoint when available.

### M3 — Playback coordinator and managed bridge
**Work:** Opaque C ABI, managed interop, serialized commands, progress/events, pause/seek/stop, safe release.  
**Deliverables:** Unit-tested native/managed contract and command-line or minimal WPF controller.  
**Exit criteria:** Managed play/pause/seek results correspond to native playback, without races in stress tests.

### M4 — WPF desktop experience and library
**Work:** WPF MVVM shell, dark UI, scanner, SQLite, library pages, queue/controls, artwork, player status, settings and preferences.  
**Deliverables:** Usable desktop `.exe` for basic local FLAC listening.  
**Exit criteria:** A user can select a folder, view songs, choose one and hear/play/seek it when hardware is available.

### M5 — Hi-Res, strict exclusive and dual-output management
**Work:** Exact-format negotiation, bit-perfect eligibility gate, USB DAC hotplug, output switching and diagnostics. Test 16/44.1, 24/96 and 24/192 device-dependent cases.  
**Deliverables:** Complete output-selection settings and failure handling.  
**Exit criteria:** App selects either type of endpoint and truthfully handles accepted/rejected exclusive formats and hotplug.

### M6 — Gapless, packaging and verification
**Work:** Same-format track predecode, UI accessibility/polish, stress/regression tests, publish portable x64 app and license notices.  
**Deliverables:** Windows release folder; `README.md`; `docs/VERIFICATION_REPORT.md`, `docs/MANUAL_HARDWARE_QA.md` and Antigravity walkthrough artifact.  
**Exit criteria:** QA matrix completed with evidence and explicit blockers; no unresolved P0 blocking test failures in the validated target environment.

### Implementation sequencing guidance

Complete M0 through M3 before spending significant effort on visual effects. After M3, alternate UI and device-integrity vertical slices until the application is fully usable. If environment lacks Windows/USB hardware, do not pretend to pass M1/M5 physical acceptance tests; finish all testable code, provide a buildable Windows handoff, and mark physical verification pending.

---

## 16. Agent checkpoint behavior and completion reporting

For every milestone, Antigravity should provide:

- Exactly what was built and which files changed.
- Exact commands run, meaningful output and test results.
- Current unresolved failures/blockers.
- What the next step is and why.

`docs/IMPLEMENTATION_STATUS.md` must track:

```text
Milestone: M2
Status: PASS | IN PROGRESS | BLOCKED
Completed work: ...
Build command and result: ...
Test command and result: ...
Hardware tested: yes/no; details ...
Remaining issues: ...
```

The final `docs/VERIFICATION_REPORT.md` must contain:

1. Windows version, CPU architecture, .NET SDK, compiler and dependency versions.
2. Native DLL build result and managed UI build result.
3. Automated tests, count passed/failed/skipped and evidence.
4. Physical device(s) actually used; which sample formats were tested.
5. QA-01 through QA-16 status matrix.
6. Playback screenshots if a GUI is genuinely available; don't invent screenshots.
7. Executable/release folder path (only if created).
8. Known unsupported device cases, unresolved errors and candid limitations.

### Mandatory release gate

The agent must NOT conclude `production ready` until P0 requirements are implemented, the executable builds, tests pass, real playback is hardware-verified in the stated environment, and no known severity-1 playback issues remain. If hardware validation is unavailable, the truthful result is **implementation complete pending Windows/hardware validation**, not a certified finished release.

---

## 17. Sample build and run workflow

The agent must verify commands against the generated actual paths; below is the intended developer experience, not a guarantee that empty scaffolding will build.

```powershell
# On a correctly provisioned Windows development PC
pwsh -File .\scripts\check-prerequisites.ps1
pwsh -File .\scripts\build.ps1 -Configuration Release
pwsh -File .\scripts\test.ps1 -Configuration Release
pwsh -File .\scripts\publish.ps1 -Configuration Release
```

Scripts should:

- Configure native dependencies using a committed manifest/lock convention, avoiding undocumented machine-specific library paths.
- Build native x64 DLL and managed `net10.0-windows` WPF solution.
- Copy required runtime DLLs into the app publish output.
- Run the native and managed test suites and return failing exit codes properly.
- Publish into `dist/HiResPlayer-win-x64/`, containing a working executable and necessary dependencies.
- Record any hardware-only tests separately rather than making automated test success depend on a physically connected DAC.

Document `dotnet` and CMake alternatives in README for maintainers who do not use PowerShell scripts.

---

## 18. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Windows driver rejects high-res exact format | Validate with `IsFormatSupported`; communicate limitations; optional explicit shared fallback |
| Driver reports format but internally processes PCM | Report only app-side bit-perfect eligibility; do not promise DAC measurement |
| 24-bit sample packing is wrong | Golden vector tests with negative and full-scale samples; valid-vs-container precision explicit |
| Exclusive buffer alignment or event issue | Follow Microsoft WASAPI sample and error contracts; integration test different endpoints |
| System sounds or another app occupies endpoint | Clear device-busy error and user-directed shared-mode option |
| USB hot-unplug race | Serialize lifecycle, short MMDevice callbacks, no UI/thread violations |
| FLAC parser consumes corrupt metadata | Use maintained libFLAC, bounds checks and malicious-input tests |
| UI controls block audio thread | Separate render thread and command/event queues |
| Too much UI work before working playback | Milestone M1/M2 exit gates enforce actual audio first |
| Dependency license mistakes | libFLAC is BSD-like; include proper notices; check all other redistributable dependencies |
| Agent runs without Windows hardware | Run hardware-independent tests and explicitly identify physical QA as pending |
| Windows 10 support assumption | Windows 11 primary; declare Win10 support only after separate compatibility tests |

---

## 19. Product success and definition of done

### Functional success

A nontechnical Windows 11 user can launch the installed/portable application, point it to a local FLAC folder, select an actual internal audio endpoint or USB DAC, play songs through the selected physical audio path, seek and browse the library, and review source/output format data. On a supporting device they can enable WASAPI exclusive and play at the original PCM format without application-side DSP, while getting honest errors when that is not possible.

### Engineering success

- Source code and dependency management are reproducible.
- No placeholder audio or simulated status in the production player.
- Real renderer + libFLAC + WPF integrate end-to-end.
- Automated sample-format tests pass, including correct 24-bit mapping.
- Native resources are freed correctly, and interruption paths are tested.
- Included verification report proves what was actually exercised.

### User-facing success

- The application is approachable for casual listening and informative for advanced listeners.
- No misleading marketing claims such as `all AUX is Hi-Res`, `exclusive always bit-perfect at the DAC`, or `USB DAC always higher quality`.
- Clear audio path visibility: `FLAC file -> decoded PCM -> WASAPI -> chosen DAC -> analog AUX`.

---

## 20. Primary technical references and authority order

Prefer official vendor API specifications over informal articles; use current documentation when implementations expose device-specific behavior.

1. Microsoft — [Exclusive-Mode Streams](https://learn.microsoft.com/en-us/windows/win32/coreaudio/exclusive-mode-streams). Correct WASAPI exclusive initialization and buffer alignment.
2. Microsoft — [`IAudioClient::IsFormatSupported`](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-isformatsupported). Exact format support semantics, shared/exclusive `ppClosestMatch` rules.
3. Microsoft — [Device Formats](https://learn.microsoft.com/en-us/windows/win32/coreaudio/device-formats). Shared mix format and special compatibility considerations for mono/stereo descriptor alternatives.
4. Microsoft — [`IAudioClient` interface](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nn-audioclient-iaudioclient). Stream management API surface.
5. Microsoft — [`IMMNotificationClient`](https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nn-mmdeviceapi-immnotificationclient). Endpoint notifications and callback safety restrictions.
6. Microsoft — [Extensible Wave-Format Descriptors](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/extensible-wave-format-descriptors). 24-bit samples, valid bits and container size.
7. Microsoft — [Shared Event-Driven Rendering Sample](https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendersharedeventdriven). Shared event-driven output pattern.
8. Xiph — [libFLAC Decoder API](https://xiph.org/flac/api/group__flac__decoder.html). Streaming decoder, metadata/error callbacks and seeking.
9. Xiph — [FLAC Licensing](https://xiph.org/flac/license.html). libFLAC redistributable license and attribution.
10. Microsoft — [What's New in WPF for .NET 10](https://learn.microsoft.com/en-us/dotnet/desktop/wpf/whats-new/net100). Framework baseline.
11. Google Antigravity — [Agent Modes and Settings](https://antigravity.google/docs/agent-modes-settings). Planning mode and review behavior.
12. Google Antigravity — [Implementation Plan](https://antigravity.google/docs/implementation-plan/). Plan artifact review and execution handoff.
13. Google Antigravity — [Walkthrough](https://antigravity.google/docs/walkthrough). Implementation completion reporting.

---

## 21. Final instruction for the implementing agent

**Build the application, not another PRD.** Start by proving actual Windows WASAPI output. Implement a real libFLAC decoding pipeline; expose it through a safe C ABI to WPF; add the music library, controls, endpoint selection and strict exclusive-mode behavior; then test and package it. Prioritize audio integrity, reliability, transparent diagnostics and a usable finished executable. Keep all assertions about playback, bit-perfect operation and hardware testing strictly grounded in the tests actually performed.

**End of PRD.**
