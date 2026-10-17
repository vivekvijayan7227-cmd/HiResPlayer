# HiResPlayer | Premium Offline Audio Player

## Independent v0 Engineering Audit + Final Product Requirements

**Document status:** Consolidated engineering report and proposed product acceptance specification, Version 2.0 — 15 October 2026.  
**Source basis:** `HiResPlayer_Source_Review.zip` (83 entries), C++20 native engine / .NET WPF shell. All file:line locations refer to that v0 source snapshot.  
**Target:** A genuinely premium Windows desktop music player, delivering transparent high-fidelity output, exceptional control, accurate audio monitoring, and dedicated non-AI Vocal Focus / Instrumental Focus / Music Analysis modes.  
**Non-negotiable boundary:** 100% local operation; no AI/ML inference, ONNX, cloud services, model downloads, remote analysis or internet requirement. Optional updates/network features must not be required to play local music.  
**Evidence:** Static source review plus previously executed portable DSP tests. Windows integration, DAC capture, acoustic measurements, blind listening results and final-device performance have **not** been validated in the supplied environment.

## Executive decision and premium-product charter

**Status of v0: not release-ready.** The v0 native audio architecture is a workable starting point, but existing bugs in PCM tail handling, WASAPI format conversion, channel layout assumptions, realtime DSP parameter updates, sample-rate conversion and EQ activation must be corrected before building a premium experience on top of it.

**Product ambition:** Make HiResPlayer meaningfully superior to an ordinary file player *in the overall listening experience*: more faithful default output, more reliable track/device behavior, musically useful optional processing, transparent and honest controls, deeper analysis, meaningful A/B comparison, and meticulously engineered interaction quality.

**Engineering truth:** Software cannot reconstruct information that is absent from the source recording merely by selecting a higher sample rate or applying enhancement. A completely accurate reference path **cannot improve on the source samples**; its goal is not to degrade them. Premium differentiation comes from preserving the master when requested, accurate output choices, useful reversible DSP, focused exploration and professional verification. 'More expensive sounding', 'always richer', or 'better than the mastering engineer' are not measurable product requirements.

### The final application must deliver

| Outcome | Required user experience | Engineering standard |
|---|---|---|
| Studio-faithful original playback | One-touch Original mode with trustworthy current signal-path status | PCM unchanged where a supported bit-exact route is demonstrably available; transparent conversion otherwise |
| Premium perceived sound when requested | Clearly optional, tasteful EQ, vocal focus and instrument focus | Level-matched, reversible DSP; no unexplained distortion or hidden enhancement |
| Voice/instrument exploration without AI | Fast focus modes plus deeper *estimated* foreground/background analysis | Classic DSP methods; artifact-aware; never promise perfect separation |
| Dependable all-day music listening | Instant coherent playback controls, stable devices and truthful progress | No missing track tails, race conditions, silent failures or hung device states |
| Expert-grade information | Real FFT/spectrogram, loudness, true-peak, stereo correlation and negotiated output format | Measurements derived from samples; no fabricated spectrum or quality badges |
| Premium interface | Uncluttered, responsive listening-first design | Accessible keyboard controls, clear labels, no control that only changes visuals |

### Priority categories

- **P0 / Release blocker:** Playback safety, correct output formats, reliable end-of-track, data-race elimination, clear reference path, no AI runtime dependency.
- **P1 / Premium beta blocker:** Proven SRC quality, EQ correctness, loudness/true-peak metering, musically safe DSP modes, real-time responsiveness and audio-backed analysis.
- **P2 / Product refinement:** Advanced non-AI foreground/background estimation, stem import, music library refinements, analyzer customization, premium polish and broader format support.
- **Research / conditional:** Any claim of high-quality approximate vocal/accompaniment separation from arbitrary stereo masters; must pass listening and artifact validation before being marketed as a general solution.

## Existing-v0 scope correction: eliminate AI, retain the musical ambition

| Present component | v0 evidence | Product decision |
|---|---|---|
| `src/HiResPlayer.SeparationWorker/` | `Program.cs:97–130` generates a sine input tensor instead of using the selected file | Remove from ship solution and packaging; do not complete with a model |
| `tests/generate_onnx_model.py` | Toy neural-network test generator | Remove from production build/test dependencies |
| `HiResPlayer.Infrastructure/Separation/` | Model asset/cache orchestration | Remove model loading/downloading; introduce separate *conventional DSP analysis cache* only if required |
| `MainWindow.xaml.cs:1363–1424` | Inference task UI is not connected to playable actual vocal/instrument stems | Replace with local DSP-mode controls and optionally offline analysis-job status |
| `DspGraph.h:80–87` + `CenterChannelProcessor.h:51–70` | Current stem sliders modulate mid and side of the original mix | Rename as focus/intensity processing; no 'true stem' claim unless independently supplied stems exist |
| Release documentation | AI isolation and high-resolution claims exceed demonstrated product behavior | Replace with evidence-based claims and actual decoded/negotiated format readouts |

**Terminology policy:** 'Vocal Focus', 'Instrumental Focus', 'Center Attenuation', 'Estimated Vocal Foreground' and 'Estimated Accompaniment' are valid product terms when the processing supports them. 'Isolated studio vocals', 'vocal removed', 'independent instruments' or 'perfect separation' are not valid descriptions of ordinary stereo DSP outputs. The label 'original stems' may be used only for genuine user-provided stems.

## Premium listening modes: required behavior

| Mode | Processing intent | Controls | Explicit limitations |
|---|---|---|---|
| **Original / Reference** | Decode and deliver source with no EQ, focus, widening, normalization or limiter | Output device, shared/exclusive mode, format readout, gain where appropriate | Bit-exact only for a verified eligible digital path |
| **Vocal Focus** | Increase perceived lead vocal presence and reduce competing elements | Intensity, tonal shaping, center width, artifact safeguard, A/B | Background instruments sharing same time-frequency region will be affected |
| **Instrumental Focus** | Lower dominance of centered foreground voice, expose backing arrangement | Intensity, center suppression, stereo-preservation option, A/B | Also suppresses centered non-vocal instruments; residual vocals remain |
| **Music Analysis** | Explain *measured* energy, dynamics and image in the actual recording | FFT/spectrogram, waveform, channel correlation, LUFS, true-peak, frequency ranges | Frequency and harmonic estimates do not prove instrument identity |
| **Enhanced Listening (optional)** | Purposeful user-chosen tonality/dynamics and safe speaker/headphone playback | Parametric EQ, channel balance, optional crossfeed, optional loudness normalization | Never silently active in Original mode |
| **Original-stem explorer (optional)** | Independent level, mute, solo, pan of supplied vocal/instrument multitrack material | Import aligned stems, precise sync, blend, audition | Works only with real user-provided synchronized stems |

### Core interaction rules

1. Opening a track defaults to **Original** or an explicitly remembered user preference; do not secretly enable gain boosts, resampling gimmicks or spatial effects.
2. Mode changes, A/B bypass, seek and intensity adjustments must use smoothed gain/parameter transitions to prevent clicks. A/B comparison offers optional matched perceived loudness so a louder version does not automatically 'win'.
3. A mode readout should show **signal class** ('Original PCM', 'Processed DSP', 'Estimated Foreground', 'Imported Stems') and disclose active SRC and output conversion.
4. The user can always restore Original at a single action without changing the song position or volume unexpectedly. Playback should remain responsive while optional offline analysis runs.
5. If a DSP focus algorithm cannot reliably interpret a track (mono source, fully centered mix, unusual spatial placement), provide graceful fallback and straightforward caveats instead of false certainty.
6. Offer sensible presets ('Natural', 'Balanced', 'Strong') with a continuous 0–100 intensity control; **0 means audibly identical to bypass**, with test coverage. Presets must be editable and recallable per track only with opt-in.

## Non-AI Vocal/Instrument Focus Engine: technical design

**Do not conflate processing with semantic source separation.** A stereo master contains overlapping sources; a non-AI algorithm can emphasize or estimate some components under favorable conditions, but cannot robustly identify 'the singer' or 'the guitar' from arbitrary audio without additional information. The product should offer two complementary implementations.

### Tier A — Real-time focus (required, release-grade)

- Work from native decoded stereo PCM at the actual processing sample rate. Include a **guarded mid/side transform**, optionally frequency-dependent center attenuation; explicitly test mono compatibility.
- Add carefully band-limited voice-presence EQ / dynamic EQ when helpful, not as a claim of isolated singing. DSP controls use coefficients and parameter ramps owned by the audio thread.
- Allow Instrumental Focus to attenuate selected center bands without automatically adding the current `+20%` side gain; stereo width is a *separate, opt-in* control.
- Provide peak headroom, finite-value protections and optional transparent downstream protection; don't substitute hard clipping, `tanh` saturation or a misleading true-peak limiter label.
- Measured analysis runs on a copy/tap stream off the realtime playback callback. DSP processing must never wait for UI meters.

### Tier B — Offline conventional spectral estimation (optional advanced mode)

Evaluate classical, non-trained signal decomposition techniques: STFT and overlap-add reconstruction; repeating-pattern accompaniment estimation (REPET family); harmonic-percussive decomposition (HPSS); stereo coherence / center estimates; and spectral masks with user-adjustable aggressiveness. These methods can help listeners examine accompaniment or foreground traits. **HPSS identifies harmonic and percussive structure, not 'vocals' versus 'instruments'.** REPET assumes repeating backing material and may fail for non-repeating compositions, vocal harmonies or sparse mixes.

Suggested processing flow:

```text
Selected local audio file
    |-- Decode PCM + checksum/metadata + channel-layout validation
    |-- Create full-rate reference playback handle (unaltered)
    '-- Analysis worker, NOT on WASAPI render callback
         |-- Analysis-rate copy / carefully filtered resampling
         |-- STFT per channel + time/frequency coherence measurements
         |-- Classical separation estimates (REPET, optional HPSS, M/S)
         |-- Artifact-safe masks + consistent interchannel processing
         |-- Inverse STFT / overlap-add + reconstruction verification
         |-- Align for precise timing; high-quality return SRC if needed
         '-- Cache *derived* foreground/background estimates locally
                  |
                  '-- Audition/Blend: Original | Estimated foreground |
                                   Estimated accompaniment | Residual
```

Quality rules for Tier B:

- **No pre-trained model, neural inference, ONNX runtime, telemetry upload or remote computation.** Support no-internet operation during file import, full analysis, audition and playback.
- Keep analysis-rate signals separate from the reference playback path. Cache invalidation must follow file hash, algorithm version and settings; never mutate the source file.
- Use windows and overlap satisfying reconstruction conditions; preserve phase relations, account for analysis delay, and maintain gapless aligned switching. Validate stereo and mono behavior.
- If creating complementary estimates, verify `foreground + background ≈ processed-analysis source` numerically at matched gain/latency. This does **not** prove either estimate corresponds perfectly to vocals or instruments.
- Show 'processing', 'available', 'fallback' and 'artifact warning' states honestly. Analyze asynchronously with cancellation and no playback dropouts.
- Permit A/B comparison against Original, estimated vocal foreground and accompaniment at fair listening levels; include a Residual view for diagnostics.
- Treat per-instrument labels as **frequency/harmonic characteristics** unless supplied real multi-track stems. Do not imply that the display recognizes piano, guitar or bass with certainty.
- Deliver Tier B only if objective artifact tests and human listening show a meaningful gain over Tier A on a documented evaluation set. Otherwise ship Tier A without making an unsupported separation promise.

### Quantitative enhancement controls (implementation contract)

| Control | Invariant / validation |
|---|---|
| Intensity 0–100 | At 0, DSP focus path nulls to original within numeric precision in matching float pipeline |
| Center amount | Bounded gain; instrument mode never changes side gain unless stereo width is separately enabled |
| A/B gain matching | Optional, annotated; match integrated or short-term loudness within design tolerance without altering saved master |
| Stereo width | Separate from source focus; meter L/R correlation and mono fold-down delta |
| Headroom | Preamp auto-headroom preview and clipping warning when boosts increase levels; reference path remains unaltered |
| Derived estimates | Deterministic offline render for fixed input/version/settings; cache purged on demand |
| Time alignment | Switching original/estimate does not drift across long tracks or accumulate look-ahead offset |

## Reference-grade signal architecture and actual premium sound

```text
LOCAL MEDIA -- file integrity, metadata, safe decoder, accurate channel map
                             |
                             v
                  PCM SOURCE / TIMEBASE
                      /              \
                     /                \
       ORIGINAL / REFERENCE        DSP / ENHANCED PATH
       no hidden processing        floating point + explicit headroom
       direct compatible PCM       -> measured SRC only if needed
       -> optional exclusive       -> EQ / focus / optional crossfeed
       -> validated device fmt     -> optional oversampled peak safety
                    \                /
                     \              /
                    DEVICE FORMAT & CHANNEL NEGOTIATION
                                    |
                        WASAPI shared / exclusive renderer
                                    |
                  device driver / DAC / speakers / headphones

    independent taps -> meters, FFT, waveform, spectrogram
    local offline worker -> classical signal estimates and cache
    UI parameter changes -> realtime-safe snapshots / ramps
```

**Original path:** Use WASAPI exclusive when supported and requested, with explicit `IsFormatSupported` negotiation. Shared mode is a legitimate default for compatibility but can require format conversion or Windows mixing. Neither an exclusive-mode label nor a 24-bit display proves bit-perfect audio; validate digital output with controlled tests. Avoid unnecessary conversion where source/device formats match.

**Enhanced path:** Perform conventional DSP in suitable floating-point precision, consistently manage preamp/headroom, process L/R or multichannel using explicit channel mapping, and only convert at verified boundaries. A fancy resampler is not an enhancement in itself; it is there to prevent quality loss when conversion is necessary.

**No hidden changes:** Up-sampling a 44.1-kHz track to 192 kHz does not reconstruct the original recording's absent ultrasonic data. EQ, stereo widening, dynamic range compression, crossfeed and normalization remain opt-in. A system can be perceptually preferred on some tracks without being measurably more faithful to the source.

## Features that actually differentiate HiResPlayer from an ordinary player

| Premium differentiator | Why listeners benefit | Dependency / honest boundary |
|---|---|---|
| Transparent original path + signal-path inspector | Trusts what format is decoded, processed and sent to the driver | Claims about physical DAC output require capture/driver verification |
| Musical Vocal and Instrumental Focus | Lets listeners examine a performance and hear background texture | Useful emphasis, not guaranteed independent stems |
| Audio-backed real-time spectrogram + stereo phase meter | Reveals actual frequency distribution and channel behavior | Displays characteristics, not semantic instrument IDs |
| Excellent PEQ with auto-headroom preview | Tailors playback to listening system without unexpected overdrive | Default untouched; presets optional and reversible |
| Loudness-matched instantaneous A/B | Makes focus/EQ judgments fairer | Matching only enabled when comparing, not the reference baseline |
| Reliable output-device switching + diagnostics | Prevents surprise silence, clicks or device conflicts | Explicit shared/exclusive support and deterministic fallback |
| Gapless tracks and trustworthy seek/progress | Preserves album transitions and playback confidence | Requires tail drain and sample clock verification |
| Local-first analysis and non-AI derived preview | Useful offline exploratory capabilities | Only ship if artifacts and compute cost are acceptable |
| Carefully designed desktop interaction | Less effort, fewer misleading controls, fewer crashes | UI should display verified technical facts, not pseudo-meters |

**Optional later enhancements:** user-measured room/headphone correction (requires measurement data and clear bypass); high-quality headphone crossfeed; user-supplied multitrack stems; album ReplayGain-style listening normalization; expanded local decoders for WAV/AIFF/ALAC. Defer these if they threaten the reference path or core playback stability.

## Measurable acceptance criteria — proposed engineering targets, NOT achieved results

The following targets define tests the future implementation should pass. They do not assert current compliance and may be tightened after a formal reference-device benchmark. All measurements must include the original test file, measured output, test procedure, hardware/software versions and pass/fail result.

| Test ID / priority | Target acceptance test | Pass criteria / documentation |
|---|---|---|
| **QA-01 P0** | Exact supported PCM source -> compatible exclusive endpoint | Digital sample comparison matches reference, except explicitly documented conversions; independently confirm configured path |
| **QA-02 P0** | EOF, partial buffer, short tracks, gapless album sequence | Every input sample accounted for; no chopped tail; one and only one end event; no incorrect inserted silence |
| **QA-03 P0** | Audio control stress/device churn | No undefined-thread access, crashes, hang or unhandled device errors; fallback is visible and recoverable |
| **QA-04 P0** | Sample formats & layout: mono/stereo/24-bit integer/float where supported | Correct frame size, clipping behavior, channel identification and format negotiation; unsupported layouts reject clearly |
| **QA-05 P1** | SRC: 44.1↔48, 96→48, 192→48 | Proposed target: passband ripple ≤0.05 dB in defined passband and alias/stopband rejection ≥100 dB for qualifying test vectors; publish actual filter transition band and latency |
| **QA-06 P1** | EQ and focus at zero | DSP bypass returns original float samples within defined machine-tolerance; enabled LP/HP/notch work at 0-dB 'gain' setting |
| **QA-07 P1** | Click-free parameter and mode changes | No discontinuity beyond a pre-agreed synthetic fixture threshold; waveform/spectrogram inspection plus panel listening |
| **QA-08 P1** | Loudness-matched comparison | Compare candidate and reference at ±0.3 LU integrated loudness *where measurement is meaningful*, or matched short-term method disclosed |
| **QA-09 P1** | True-peak and loudness meter verification | Agree with vetted BS.1770-5 reference signals/meters within documented implementation tolerance; show input/output path positions |
| **QA-10 P1** | Vocal and instrument modes | Published blind level-matched tests on male/female, mono, centered instruments, acoustic and heavily compressed mixes; preserve a user-controlled natural preset |
| **QA-11 P1** | Stereo/mono fold-down | Report correlation, side-gain change, summed mono difference and artifact cases; no undocumented side boost in instrumental mode |
| **QA-12 P1** | Overnight reliability | 8-hour session with heavy UI use, seeking and device changes: no silent stall, render underruns attributable to DSP or monotonically growing memory |
| **QA-13 P1** | Spectrum analyzer truth | FFT bars and spectrogram track injected single- and dual-tone frequencies within defined resolution; bars do not animate for digital silence |
| **QA-14 P1** | Product transparency | Active effects, sample rate, sample format, shared/exclusive mode and conversions shown accurately; no bit-perfect badge without verified route |
| **QA-15 P2** | Local offline spectral estimation | Deterministic rendering; no source mutation; estimates aligned and approximately recombine; no severe recurring watery/phasing artifacts in evaluation set |
| **QA-16 P2** | Premium UX and accessibility | Keyboard-only essential operations, focus-visible controls, scalable type, high-contrast modes, responsive waveforms and stable state transitions |

### Listening acceptance protocol

- Compare using at least 20 diverse licensed/reference excerpts covering quiet acoustic recordings, close-mic vocals, dense rock, modern pop, jazz, orchestral, deep bass, wide electronic productions, mono and tracks with off-center singers.
- Test on at least three playback setups, including ordinary headphones, an external DAC/headphone chain and a speaker-based stereo system. Record hardware, audio driver, Windows settings, headphone/speaker chain and playback gain.
- Perform matched-loudness blind A/B/X or paired-preference listening. Separate **preferred musical presentation** from **faithful unprocessed reproduction**; each is a different test objective.
- Log artifacts by type (metallic sound, musical-noise flutter, leakage, phase/image shift, transient smearing, bass loss, limiter pumping), severity and song segment; link to setting and reproducible clip.
- Reject any preset that routinely performs worse than bypass on the 'Natural' listening target. High-intensity modes can remain available with clear trade-off labels.

## Build implementation map to current source structure

| Existing location | Recommended premium implementation work |
|---|---|
| `HiResAudioEngine/src/PlaybackController.cpp` | Explicit decode/playback/EOF/device state machine; reliable seek & tail drain; source/render timebase; stereo & channel correctness |
| `HiResAudioEngine/src/WasapiRenderer.cpp` | Robust device format negotiation, shared/exclusive setup, proper partial buffer handling, conversion, recovery and output telemetry |
| `HiResAudioEngine/src/PcmRingBuffer.h` | Complete-frame partial consume, invariant tests, underrun/EOF separation |
| `HiResAudioEngine/src/AudioResampler.h` | Replace Hermite path with benchmarked band-limited SRC; streaming histories, reset semantics and quality tests |
| `HiResAudioEngine/src/Equalizer.h` | Correct always-active filter categories, coefficient-smoothing and denormal/click handling |
| `HiResAudioEngine/src/DspGraph.h` | Split reference and enhanced paths, organized gain staging, optional focus processing, accurate before/after taps |
| `HiResAudioEngine/src/CenterChannelProcessor.h` | Real-time Tier A vocal/instrument focus; side boost behind explicit stereo-width control; mono-safe fallbacks |
| `HiResAudioEngine/src/StemMixer.h` | Use only for truly separate imported streams or real algorithmic estimates, properly sample-aligned; otherwise rename/remove misleading calls |
| `HiResAudioEngine/src/hires_audio_api.cpp` | Lock-free DSP parameter snapshots or audio-thread ownership, API diagnostics/negotiation readouts |
| `HiResPlayer.Desktop/MainWindow.xaml(.cs)` | Premium focus UI, actual audio-backed meters, status/diagnostic inspector, accessibility, honest labels, A/B matching |
| `HiResPlayer.Infrastructure/` | Remove AI model/cache service; optional non-AI async analysis jobs, reproducible derived-signal cache |
| `tests/` + Windows CI | Golden PCM fixtures, property/unit tests, device integration matrix, DSP benchmarks, long-run regression testing |

## Delivery roadmap and stop/go gates

**Gate 0 — Trustworthy core:** Stop product expansion until P0 issues (AUD-04–07) pass and the production installer has no ONNX/model dependencies. Test short-track EOF, channels and exclusive/shared formats. Pass Gate 0 before promising a premium sound experience.

**Gate 1 — Audible baseline:** Replace SRC; make EQ and parameter changes correct and click-free; add auditable device-format status; build true sample-peak/true-peak/FFT monitoring. Deliver a trustworthy reference mode and excellent EQ. Prioritize code over visual redesign.

**Gate 2 — Focus modes:** Ship conservative Tier A Vocal Focus and Instrumental Focus with headroom management, mono/stereo safety, audible bypass, level-matched A/B, and musician/engineer listening tests. Clearly state what each mode changes.

**Gate 3 — Advanced exploration (conditional):** Prototype Tier B classical spectral decomposition in a local background worker, measure reconstruction quality and artifacts, compare against Tier A, then either integrate as a labeled advanced preview or omit it.

**Gate 4 — Premium release:** Portable reproducible builds, Windows DAC/device regression, usability/accessibility, signed installer if product delivery requires it, crash logs kept local by default, documented offline/privacy behavior, polished controls, release notes and truthful feature claims.

### What the product must NOT do

- No AI or machine-learning models, inference workers, data uploads or compulsory network license checks.
- No auto-enabled 'magic enhancement', hidden EQ, artificial width, mandatory SRC or lossy recoding in Original mode.
- No invented spectrum/AI confidence indicators or marketing labels claiming master-quality restoration by upsampling.
- No automatic `tanh` coloration presented as transparent true-peak limiting.
- No vocal/stem semantics attached to simple center-channel gain without qualification.
- No blocking CPU-intensive analysis or disk access inside the real-time WASAPI render callback.

## Original code audit: findings and severity

**Code-evidence note:** Findings below were established during the preceding v0 audit and retain the original AUD references for traceability; this revision adds product requirements rather than asserting new implementation fixes. P0 = release-blocking; P1 = major quality/correctness; P2 = usability and maintainability. AI-only AUD-01–03, AUD-20–21 and AUD-32–33 are retired as *removal tasks*, not implementation requests.

## P0 — playback integrity and real-time correctness
### AUD-04 — Renderer cannot consume the final partial PCM buffer — P0 / Proven from control flow
**Files:** `src/HiResAudioEngine/src/PcmRingBuffer.h:50–71`; `src/HiResAudioEngine/src/WasapiRenderer.cpp:358–395`.
- `read()` returns **zero** when fewer than `bytes` are available, rather than supplying the available complete frames followed by silence. The consumer always requests its entire `frames_avail` chunk.
- At end-of-file, a residual tail smaller than the requested hardware buffer cannot be consumed. The consumer generates underruns while bytes remain stuck in the ring. Short sources may be especially vulnerable; depending on frame availability and device buffer size, even an entire short track may never be delivered.
- The code never emits `HP_EVT_TRACK_END` (defined but not used). The WPF UI polls rendered sample count for end-of-track (`MainWindow.xaml.cs:799–805`), so a stuck short tail can also prevent auto-advance.
- **Fix:** have the renderer consume `min(available, frames_avail)` complete frames, fill remaining frames with zero, account only for consumed source frames, and signal decoder-EOF + drained ring + hardware tail. Treat true underruns separately from normal EOF.
- **Gate:** 10 ms, 100 ms, 1.01 s and odd-duration tracks play completely and auto-advance exactly once.

### AUD-05 — Shared mix-format conversion is incomplete — P0 / Conditional, high-impact
**Files:** `src/HiResAudioEngine/src/WasapiRenderer.cpp:151–178`; `src/HiResAudioEngine/src/PlaybackController.cpp:390–431`.
- WASAPI shared mode takes the endpoint's **actual** mix format, but the decoder always packs **32-bit float**, using the **source's** channel count.
- There is no general interleaved channel remixing and no integer-format output conversion for an endpoint whose shared mix format is not stereo float32. The renderer consumes according to `nBlockAlign` of the destination, which can differ from the bytes that the decoder produces.
- **Impact:** playback pitch/channel mapping corruption, underruns, or incorrect PCM interpreted as another format on some endpoints.
- **Fix:** build a format-aware converter for `WAVEFORMATEXTENSIBLE` subformat, channel mask, container/valid bits, sample rate and block alignment. Either negotiate a tested stereo IEEE-float client format or convert explicitly to the actual stream format. Validate all cases before starting.
- **Gate:** stereo/mono/multichannel endpoint matrices with PCM16/PCM24/float32 combinations, validated with channel-identification WAV/FLAC test vectors.

### AUD-06 — Channel processing assumes exactly two channels — P0 / Proven from code
**Files:** `src/HiResAudioEngine/src/PlaybackController.cpp:23–95`; `src/HiResAudioEngine/src/AudioFormatNegotiator.cpp:17–40`.
- DSP indexes input as `f[i*2], f[i*2+1]` (and analogous integer accesses), regardless of `wfex.Format.nChannels`. On a mono endpoint this accesses beyond the current buffer; on multichannel endpoints it processes different channel pairs incorrectly.
- `FlacDecoder::Open` validates only `channels != 0`, so the decoder may accept mono or multichannel FLAC.
- **Fix:** create a frame layout dispatcher that honors destination channels and masks. Stereo-only modes must downmix intentionally or reject incompatible formats; never cast/stride assuming 2 channels without a guard.

### AUD-07 — DSP parameters are modified concurrently with the render thread — P0 / Proven concurrency defect
**Files:** `src/HiResAudioEngine/src/hires_audio_api.cpp:166–274`; `src/HiResAudioEngine/src/Equalizer.h:28–35,145–151`; `src/HiResAudioEngine/src/DspGraph.h:42–75,80–115`; `src/HiResAudioEngine/src/CenterChannelProcessor.h:31–35,141–165`; `src/HiResAudioEngine/src/PlaybackController.cpp:22–95`.
- C ABI setters modify ordinary `float`, `bool`, filter coefficients, and filter state from the UI/control thread while the WASAPI callback reads/writes the same objects. There is no audio-thread-safe parameter transfer or lock-free snapshot scheme. WPF timer debouncing **reduces call frequency**, not race risk.
- **Impact:** undefined behavior at the C++ level, transient coefficient combinations, clicks, and potential instability under rapid parameter changes.
- **Fix:** audio-thread-owned DSP instances; immutable parameter snapshots swapped atomically at buffer boundaries; control queue for parameter updates; coefficient interpolation or dual-filter crossfades for smooth changes; no allocations/locks within the real-time callback.
- **Gate:** ThreadSanitizer-style concurrency tests where supported, aggressive UI sweep testing, and long-running WASAPI underrun/click monitoring.

## P1 — audio quality and stability

### AUD-08 — Cubic resampler has no anti-alias low-pass on downsampling — P1 / Reproduced
**File:** `src/HiResAudioEngine/src/AudioResampler.h:31–103`.
- The converter performs cubic Hermite interpolation. It is **not** a band-limited sample-rate converter and does not suppress out-of-band components before decimation.
- Reproduction: 96 kHz source containing 30 kHz / 0.5-peak sine converted to 48 kHz produces ~0.3536 RMS output; this is a full-scale-relative aliased signal, not a rejected out-of-band component. A legitimate 1 kHz / 0.5-peak input measured ~0.3537 RMS.
- **Fix:** high-quality polyphase windowed-sinc FIR SRC with explicit stopband attenuation and state across blocks; QA with logarithmic sweep, null/residual spectra, SNR, passband ripple and alias suppression for all supported rates.

### AUD-09 — Parametric low-pass/high-pass/notch may be treated as disabled — P1 / Reproduced
**Files:** `src/HiResAudioEngine/src/Equalizer.h:158–163`; `src/HiResAudioEngine/src/DspGraph.h:70–85`.
- `HasActiveBands()` returns true only if an enabled band's `abs(gain_db) > 0.001`, but the low-pass, high-pass, notch filters function at 0 dB gain.
- Reproduction: enabled 1 kHz low-pass with gain 0 dB yields `IsDspActive() == false` while its theoretical response at 8 kHz is −37.8 dB. Thus the render callback bypasses the intended EQ when this is the only active processing.
- **Fix:** define active filter logic per filter type; peaking/shelves can be gain-zero bypass, low/high-pass and notch cannot. Align curve display and actual render route.

### AUD-10 — DSP test asserts output contrary to its own implemented widening — P1 / Reproduced
**Files:** `tests/test_dsp_fidelity.cpp:129–145`; `src/HiResAudioEngine/src/CenterChannelProcessor.h:109–113`.
- `CenterChannelProcessor` scales side by 1.2 at full instrumental focus; measured side gain is +1.58363 dB.
- The test asserts gain change is below 0.01 dB and fails at line 143 (`SIGABRT`, exit 134). Earlier test sections 1–3 passed on this Linux C++ compilation.
- **Fix:** decide intended sound and requirement: leave side unchanged for preservation or make deliberate widening a separately configurable feature with stereo/mono/phase safety validation. Test response against the intended design rather than contradictory language.

### AUD-11 — Spectrum analyzer is simulated, not obtained from music frequencies — P1 / Proven
**File:** `src/HiResPlayer.Desktop/MainWindow.xaml.cs:70–72,1516–1547`.
- The `32` spectrum values are generated from aggregate peaks, predicted EQ curve, and seeded random jitter. No real FFT signal is fed into these bars.
- **Fix:** explicitly call the UI a decorative animation, or implement actual audio observation via a tap FIFO and windowed FFT running off the audio/render thread. A signal analysis display should show calibrated frequency magnitudes of the real audio signal, not random variations.

### AUD-12 — Source-position accounting uses offered frames, not necessarily consumed frames — P1 / Proven
**File:** `src/HiResAudioEngine/src/WasapiRenderer.cpp:372–395`.
- On a successful read the position advances by all `frames_avail`. After implementing partial reads, this must be changed to `read / bytes_per_frame`, with resample-state-aware accounting and device queue latency if reporting true audible position.
- **Fix:** track submitted, buffered and presented frames separately; obtain presentation clock where possible; never count silence as consumed source audio.

### AUD-13 — No proper EOF lifecycle / final drain handling — P1 / Proven
**Files:** `src/HiResAudioEngine/src/PlaybackController.cpp:445–447`; `src/HiResAudioEngine/src/WasapiRenderer.cpp:319–402`; `src/HiResAudioEngine/include/hires_audio_api.h:75`.
- Decode completion result is ignored. No explicit EOF state is published to the renderer; `HP_EVT_TRACK_END` has no emit site. A decode failure can leave the renderer spinning on silence without transitioning to a recoverable error.
- **Fix:** design explicit states `decoding`, `source_eof`, `pcm_draining`, `device_draining`, `ended`, `error`. Fire events once; reset on seek/new source and cleanly end after last played frame.

### AUD-14 — DSP state persists across seeks and track boundaries — P1 / Proven by code path
**Files:** `src/HiResAudioEngine/src/PlaybackController.cpp:109–123,305–358`; `src/HiResAudioEngine/src/AudioResampler.h:16–25`; `src/HiResAudioEngine/src/DspGraph.h:35–40`.
- Seek resets the PCM ring and FLAC position but never calls `m_resampler.Reset`. Persistent sample histories can blend the preceding playback location into the first samples after seek.
- `SetSampleRate()` recomputes filter coefficients without resetting the biquad delay registers. Depending on settings, residual prior-track state can bleed into the next source or cause artifacts.
- **Fix:** define processor lifecycle events `ResetOnSeek`, `ResetOnNewTrack`, and optional click-free fade around discontinuities. Preserve continuity only for uninterrupted streams.

### AUD-15 — Peak meter is post-limiter sample peak, not true peak — P1 / Proven
**File:** `src/HiResAudioEngine/src/DspGraph.h:99–115`.
- Meters inspect sample magnitudes **after** soft limiting. The code does not expose unclipped input peaks, required gain reduction, preamp/EQ overload history, or intersample true-peak.
- **Fix:** report pre-limiter sample peak, post-limiter peak, limiter gain reduction, and oversampled true-peak (ITU-R BS.1770-5). Avoid claiming clipping-free analog/DAC output from a mere 0.9999f sample clamp.

### AUD-16 — DSP calculation on MMCSS render thread has unpredictable work — P1 / Risk, especially parameter changes
**Files:** `src/HiResAudioEngine/src/PlaybackController.cpp:22–95`; `src/HiResAudioEngine/src/WasapiRenderer.cpp:310–402`.
- The renderer executes a per-sample effect chain directly in the MMCSS thread. This is viable if bounded and lock-free, but concurrent filter configuration, callbacks into managed code on underruns, and render-buffer mutex contention can all cause timing variability.
- **Fix:** bounded real-time graph processing, preallocated buffers, lock-free control transfers, telemetry to a non-RT queue, CPU time distribution under device rates/buffer sizes.

### AUD-17 — Current bit-perfect user messaging overstates verified evidence — P1 / Proven
**Files:** `src/HiResAudioEngine/src/AudioDiagnostics.cpp:36–55`; `src/HiResPlayer.Desktop/MainWindow.xaml.cs:751–765,1434–1462`.
- Diagnostic code hardcodes `bool pcm_pass = true`, rather than verifying actual data identity. Passing format/mode/DSP gates means *eligible*, not independently **verified** bit identity at DAC output. The UI claims "100% Bit-Identical" and manually sets YES immediately after pressing the enable button before checking whether the stream restarted successfully.
- **Fix:** display "Exclusive, bit-perfect path eligible" only after actual mode and bypass are confirmed; reserve "bit-identical verified" for a digital loopback/test harness with exact sample comparison. Display precise reasons for any failed gate.

### AUD-18 — Mode-switch restart errors and playback errors are ignored in UI — P1 / Proven
**Files:** `src/HiResAudioEngine/src/PlaybackController.cpp:126–178`; `src/HiResPlayer.Desktop/MainWindow.xaml.cs:656–670,1156–1175,1473–1505`.
- `SelectDevice()` / `SetMode()` perform stop-restart internally but discard `Play()` return code. `PlayTrack()` sets `_isPlaying=true` without checking facade `Play()` success.
- **Fix:** propagate restart result and actual mode/output format to WPF. On error, show failure and restore consistent state; never report playing when no stream was started.

### AUD-19 — Conflicting seek handlers cause repeated seek operations — P1 / Proven
**File:** `src/HiResPlayer.Desktop/MainWindow.xaml.cs:106–114,166–173,1203–1215`.
- Drag-start/drag-complete events are registered twice. Additional mouse-up logic can submit the same seek again. In a decoder whose seek stops/joins and restarts a thread, redundant calls create audible interruptions and UI latency.
- **Fix:** keep one drag completion handler, one seek command, and cancel/coalesce stale asynchronous seeks; do not seek again on equivalent pointer-up.

### AUD-22 — Render thread uses raw COM audio-client interfaces across threads without documented apartment correctness — P1 / Hardware-dependent risk
**Files:** `src/HiResAudioEngine/src/AudioDeviceManager.cpp:15–27`; `src/HiResAudioEngine/src/WasapiRenderer.cpp:61–95,310–402`.
- `IAudioClient` is activated in the caller's context while renderer calls portions of the interface in a new MTA thread. `CoInitializeEx` result is not checked; when called from WPF's STA thread, an MTA request can fail with `RPC_E_CHANGED_MODE`. Correct COM and WASAPI interface-affinity requirements should be verified explicitly on Windows.
- **Fix:** initialize COM deliberately per thread; acquire/render client interfaces on the correct thread or marshal interfaces via COM rules; test on STA UI, device disconnection and multiple endpoint drivers.

## P2 — usability, credibility and maintainability

### AUD-23 — Endpoint hardware master-volume slider modifies global device volume
**Files:** `src/HiResAudioEngine/src/PlaybackController.cpp:361–379`; `src/HiResAudioEngine/src/AudioDeviceManager.cpp:112–145`.
- The slider adjusts `IAudioEndpointVolume`, not application-only software gain. It may affect other applications/users' audio from the same endpoint. This may be a reasonable choice for exclusive bit-perfect path, but it should be explicit in UI and tested per driver.
- **Fix:** distinguish hardware endpoint volume from application DSP volume; offer a safe and transparent selection.

### AUD-24 — Processing artifacts not honestly described in traditional instrumental mode
**File:** `src/HiResAudioEngine/src/CenterChannelProcessor.h:7–21,95–113`.
- The claim that the method preserves *all* stereo instruments, reverb and bass/air with "zero" phase issues cannot be established by mid/side algebra and 2nd-order LP/HP filters. Vocal and instrument spectra overlap; center-channel attenuation affects any sound occupying that region, while boosted side information can change image width, center stability and mono compatibility.
- **Fix:** rename modes "Center/Vocal Presence" and "Center Attenuation (Pseudo-Instrumental)" if they remain mid/side-only; avoid any claim of complete isolation without separately provided true stems. Evaluate with real multitracks and mono null tests.

### AUD-25 — The vocal mode's EQ is not source-aware
**File:** `src/HiResAudioEngine/src/CenterChannelProcessor.h:81–95,150–156`.
- A bell centered at 2.6 kHz, low-shelf cut, and side reduction raise some vocal presence but also alter snare, guitar, piano, cymbals, and reverberation. The sound could be pleasing, but it is tonal/image adjustment, not isolated vocals.
- **Fix:** present as enhancement control; design listening tests with vocal-only, instrumental-only, complex stereo and mono sources.

### AUD-26 — Limiter branding exceeds implementation scope
**File:** `src/HiResAudioEngine/src/DspGraph.h:12–26,99–115`.
- The soft-knee `tanh` stage is a memoryless saturator, not a lookahead true-peak limiter. It changes waveform shape above its threshold. Numerical sample clamps do not guarantee oversampled intersample headroom.
- **Fix:** state "soft clip / peak protection" accurately, make optional, or implement an oversampled/lookahead true-peak limiter with latency and gain-reduction metering.

### AUD-27 — Fixed source/playback format scope
**Files:** `src/HiResAudioEngine/src/PlaybackController.cpp:109–123`; `src/HiResAudioEngine/src/FlacDecoder.cpp:33–80`; WPF open dialogs.
- The playback pipeline explicitly opens FLAC. This may be intentional for v0 but cannot be represented as a universal hi-res music library. Formats such as WAV/AIFF/ALAC/DSD are not implemented in the inspected source.
- **Fix:** clearly state supported formats; add additional decoders only after stabilizing FLAC and WASAPI.

### AUD-28 — Build is coupled to one developer's absolute Windows paths
**Files:** `src/HiResAudioEngine/CMakeLists.txt:8–12,24–32`; `tests/test_resampler.cpp:4`.
- FLAC includes and binary are bound to `d:/AudioPlayer/flac-1.4.3`; some tests include absolute paths. This undermines reproducible builds.
- **Fix:** use CMake `find_package`/FetchContent/vcpkg or supplied dependencies with version checks and relative paths; CI build on a clean Windows runner.

### AUD-29 — Unsupported bit-perfect and high-resolution claims in UI
**File:** `src/HiResPlayer.Desktop/MainWindow.xaml.cs:1555–1562,1695–1726`.
- Readout describes "Original 24-bit PCM Wire" even where negotiated source could be 16-bit; bit-perfect path eligibility does not establish device output electrical/physical identity. Documentation mentions DSD/384 kHz, but inspected decoding accepts FLAC and does not implement DoP/native DSD.
- **Fix:** derive every format string from the current negotiated stream, not hard-coded promotional claims; keep hardware support as unverified until measured.

### AUD-30 — Existing automated testing does not verify end-to-end playback fidelity — P2 / Proven
**Files:** `tests/ManagedTests/AppUnitTests.cs:95–160,163–483`; `tests/test_dsp_fidelity.cpp:9–170`; `docs/VERIFICATION_REPORT.md`.
- Tests cover various unit-level behavior and format handling, but they do not independently establish sample-accurate device output, seamless seek/drain, rate-conversion quality, center-attentuation fidelity or robust output device switching. A portable native DSP test failed during this review.
- **Fix:** use known-source real PCM fixtures, finite-duration/EOF tests, impulse/step tests, digital loopback capture and corrected DSP acceptance requirements. Verify that each UI control has an audible signal-path effect and bypass restores the reference signal. No AI/model tests are required for the non-AI product.

### AUD-31 — Unsafe Unicode error-path helper buffer size
**File:** `src/HiResAudioEngine/src/FlacDecoder.cpp:12–18`.
- `WideCharToMultiByte(..., -1, ...)` returns the count **including** a NUL terminator. The code allocates `std::string(sz - 1, '\0')` and then passes output capacity `sz`, which may write one byte past the allocated string's length. This is risky even if it only occurs in an error-reporting branch.
- **Fix:** allocate `sz` bytes then remove trailing NUL, or convert using explicit source length and a properly sized output.

## Portable tests — previously recorded evidence

| Test | Prior observation |
|---|---|
| Native C++ DSP tests, GCC/C++20 | Compiled; original bypass, EQ 1-kHz boost and center-bypass passed |
| Instrument focus stereo-preservation assertion | **Failed:** +1.58363 dB unintended side boost at full strength where test expected 0 dB |
| LP active at 1 kHz with nominal 0-dB gain | **Failed:** DSP graph reported no active processing despite ≈−37.8 dB attenuation at 8 kHz in isolated filter response |
| 96 kHz → 48 kHz downsample of 30 kHz sine | **Failed:** ≈0.3536 RMS aliased output from a 0.5-peak source |
| Windows physical audio/actual DAC measurement | **Not performed**; all device-specific claims require future verification |

These results describe the v0 source already reviewed. Re-run the tests after fixes. A repaired test must demonstrate corrected behavior, not merely remove its assertion.

## Technical references and source material

1. [Microsoft Learn — Device Formats](https://learn.microsoft.com/en-us/windows/win32/coreaudio/device-formats): format matching and WASAPI shared/exclusive audio conversions.
2. [Microsoft Learn — IAudioClient::IsFormatSupported](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-isformatsupported): format negotiation, errors and fallback design.
3. [Microsoft Learn — Exclusive-Mode Streams](https://learn.microsoft.com/en-us/windows/desktop/coreaudio/exclusive-mode-streams): endpoint exclusivity and other-app compatibility.
4. [ITU-R BS.1770-5 (2023)](https://www.itu.int/rec/R-REC-BS.1770-5-202311-I): loudness and true-peak measurement algorithms.
5. [EBU R 128 (2023)](https://tech.ebu.ch/publications/r128): programme loudness standard; the broadcast target is **not** a mandated music-player default.
6. [Fitzgerald / librosa — Harmonic–Percussive Source Separation](https://librosa.org/doc/main/api/generated/librosa.decompose.hpss.html): non-neural median-filtering harmonic/percussive separation; not semantic vocal isolation.
7. [Rafii and Pardo — REpeating Pattern Extraction Technique (REPET)](https://github.com/faroit/reproducible-audio-research/blob/master/papers/rafii13.md): accompaniment/foreground estimation using repetitions, with track-dependent limitations.

## Confidence, exclusions and final decision

- **Existing audit evidence:** Code locations and portable probes are grounded in the supplied source snapshot and prior review; they are not proof of successful Windows playback, bit-exact device output or subjective superiority.
- **New additions:** Premium positioning, feature specification, DSP separation tiers, acceptance criteria and implementation roadmap are **forward-looking requirements**, not features already delivered by v0.
- **Experience promise:** A normal player should be surpassed through verifiable playback reliability, fidelity, optional tasteful focus/control, musical usefulness and polished interaction—not unmeasurable claims of improving the mastering of every song.
- **Release verdict:** Rebuild through Gate 0–4; do **not** market as professional-grade until the objective test matrix and listening evaluation have been completed and published internally.
