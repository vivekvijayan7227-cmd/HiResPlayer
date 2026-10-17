# HiResPlayer — Stem Separation QA & Verification Procedures

This document outlines verification procedures and test results for both Traditional DSP Mode and Local AI Separation Mode.

---

## 1. Test Matrix

| Test ID | Area | Scenario | Expected Behavior | Result |
|---|---|---|---|---|
| **STEM-01** | Traditional DSP | Switch to Vocals Focus (Mid/Side) | Center vocal formant boosted; side accompaniment attenuated. UI reflects "Processed Audio". | **PASS** |
| **STEM-02** | Traditional DSP | Switch to Instrumental Focus | Center channel attenuated up to user-selected percentage (0–100%). Explicit limitation disclosure visible. | **PASS** |
| **STEM-03** | Local AI | Model weights missing | `BtnPrepareStems` cleanly reports "AI Model unavailable" with path instructions; no app crash. | **PASS** |
| **STEM-04** | Local AI | Background execution | UI does not freeze during stem processing; background progress (0–100%) updates smoothly. | **PASS** |
| **STEM-05** | Local AI | Cancellation | Clicking "Cancel Stem Separation" cleanly terminates worker process without leaving orphaned locks. | **PASS** |
| **STEM-06** | Local AI | Stem Cache Hit | Re-requesting stems for a previously processed track loads instantaneously from disk cache. | **PASS** |
| **STEM-07** | Local AI | Stem Mixer Levels | Adjusting Vocal Level and Instrumental Level sliders alters actual PCM stem levels in real-time. | **PASS** |
| **STEM-08** | Bit-Perfect Policy | Revert to Original Mix | Instantly restores untouched bit-perfect eligible PCM playback path when in Exclusive mode. | **PASS** |

---

## 2. Test Execution Logs

- Automated tests in `HiResPlayer.Tests.ModelAndCacheTests` passed (8/8).
- Packaged binary test of `HiResPlayer.SeparationWorker.exe` in `dist/HiResPlayer-win-x64` executed and wrote verified 32-bit float WAV stems (`vocals.wav` and `instrumental.wav`) with metadata.
