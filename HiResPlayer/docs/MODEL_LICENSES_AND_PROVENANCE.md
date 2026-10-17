# HiResPlayer — Model Licenses, Provenance & Vocal Isolation Specification

This document details the architecture, licensing, and offline distribution rules for HiResPlayer's vocal separation features.

---

## 1. Dual Processing Modes

HiResPlayer implements two distinct processing modes:

### Mode 1: Traditional DSP Mode (Mandatory — 100% Offline)
- **Zero AI Dependencies**: Runs on pure digital signal processing matrixing without any external neural network models.
- **Center Channel Isolation & Cancellation**: Employs Mid/Side stereo decomposition:
  $$\text{Mid} = \frac{L + R}{2}, \quad \text{Side} = \frac{L - R}{2}$$
- **Vocal Bandpass Filtering**: Applies a second-order bandpass filter centered on human speech/singing formants (250 Hz – 4,500 Hz).
- **CPU Overhead**: Virtually zero (<0.1% CPU).
- **Honest Disclosure**: Center cancellation cannot distinguish centered vocals from centered bass drums, snare drums, or centered bass instruments. Some instrument bleed is inherent to Mid/Side processing.

### Mode 2: Local AI Separation Mode (Optional — Fully Local on PC)
- **100% Local Inference**: Runs entirely on the user's PC using the ONNX Runtime execution engine (`onnxruntime.dll`).
- **No Cloud Uploads**: Zero telemetry, no online APIs, and no uploading of user audio files.
- **Hardware Acceleration**: Automatically selects DirectML GPU acceleration where supported, with graceful fallback to multi-threaded CPU execution.
- **Stem Cache**: Extracted stems are losslessly saved as 32-bit float stereo WAV files in `%LOCALAPPDATA%\HiResPlayer\StemCache\`.
- **Quota Management**: Enforces a 5 GB configurable cache quota with automatic LRU purging.

---

## 2. Model Provenance & Licensing

The reference separation architecture targets the HTDemucs two-stem (`vocals`, `accompaniment`) model architecture.

- **Model Format**: Standard Open Neural Network Exchange (`.onnx`) format.
- **Default Filename**: `htdemucs_2stem.onnx`
- **Location**: `%LOCALAPPDATA%\HiResPlayer\Models\htdemucs_2stem.onnx`
- **Upstream License**: The Demucs architecture was created by Meta AI Research and distributed under the MIT / CC-BY-NC license depending on checkpoint version. Pretrained weights must adhere to their respective distribution terms.
- **Offline Installation**: When licensing permits, models may be downloaded once and placed in the models folder. If the model file is absent, HiResPlayer informs the user with instructions and continues normal playback and Traditional DSP mode without interruption.
