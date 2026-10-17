#pragma once
#include <cstdint>
#include <cstring>
#include <algorithm>

// PcmSamplePacker: converts libFLAC signed integer samples to wire PCM bytes.
// libFLAC delivers samples as int32_t with the value in the low valid_bits bits.
//
// Supported output formats:
//   - 16-bit packed little-endian  (container=16, valid=16)
//   - 24-bit packed little-endian  (container=24, valid=24)  -- 3 bytes per sample
//   - 24 valid bits in 32-bit LE   (container=32, valid=24)  -- sign-extended, zero-padded LSB
//   - 16 valid bits in 32-bit LE   (container=32, valid=16)  -- for USB DACs requiring 32-bit slot
//   - 32-bit IEEE float            (container=32, valid=32)  -- for shared-mode MF resampler bypass

struct PackParams {
    uint32_t valid_bits;      // source valid bits (16 or 24)
    uint32_t container_bits;  // output container (16, 24, or 32)
    bool     output_float;    // true = convert to float32 instead of integer
};

// Pack a single signed sample (value in low valid_bits) into the wire format.
// dst must have at least container_bits/8 bytes of space.
inline void pack_sample(uint8_t* dst, int32_t sample, const PackParams& p) noexcept {
    if (p.output_float) {
        // Scale to [-1.0, 1.0) using 64-bit divisor to prevent 32-bit integer overflow
        float f = static_cast<float>(sample) / static_cast<float>(1ULL << (p.valid_bits - 1));
        std::memcpy(dst, &f, 4);
        return;
    }
    switch (p.container_bits) {
    case 16: {
        int32_t clamped = std::clamp(sample, -32768, 32767);
        int16_t s16 = static_cast<int16_t>(clamped);
        dst[0] = static_cast<uint8_t>(s16 & 0xFF);
        dst[1] = static_cast<uint8_t>((s16 >> 8) & 0xFF);
        break;
    }
    case 24: {
        int32_t clamped = std::clamp(sample, -8388608, 8388607);
        dst[0] = static_cast<uint8_t>(clamped & 0xFF);
        dst[1] = static_cast<uint8_t>((clamped >> 8) & 0xFF);
        dst[2] = static_cast<uint8_t>((clamped >> 16) & 0xFF);
        break;
    }
    case 32: {
        int32_t s32;
        if (p.valid_bits == 24) {
            int32_t clamped = std::clamp(sample, -8388608, 8388607);
            s32 = clamped << 8;
        } else if (p.valid_bits == 16) {
            int32_t clamped = std::clamp(sample, -32768, 32767);
            s32 = clamped << 16;
        } else {
            s32 = sample;
        }
        dst[0] = static_cast<uint8_t>(s32 & 0xFF);
        dst[1] = static_cast<uint8_t>((s32 >> 8) & 0xFF);
        dst[2] = static_cast<uint8_t>((s32 >> 16) & 0xFF);
        dst[3] = static_cast<uint8_t>((s32 >> 24) & 0xFF);
        break;
    }
    }
}

// Pack an interleaved frame (channels * samples_per_ch samples) into dst.
// samples[ch][sample_index] — libFLAC planar layout.
// dst must have channels * samples_per_ch * (container_bits/8) bytes.
inline size_t pack_frames(
    uint8_t* dst,
    const int32_t* const* samples,
    uint32_t channels,
    uint32_t samples_per_ch,
    const PackParams& p
) noexcept {
    const uint32_t bytes_per_sample = p.output_float ? 4 : (p.container_bits / 8);
    size_t out_offset = 0;
    for (uint32_t i = 0; i < samples_per_ch; ++i) {
        for (uint32_t ch = 0; ch < channels; ++ch) {
            pack_sample(dst + out_offset, samples[ch][i], p);
            out_offset += bytes_per_sample;
        }
    }
    return out_offset; // total bytes written
}
