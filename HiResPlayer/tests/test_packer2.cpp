#include <iostream>
#include <vector>
#include <cmath>

struct PackParams {
    uint32_t valid_bits;      // source valid bits (16 or 24)
    uint32_t container_bits;  // output container (16, 24, or 32)
    bool     output_float;    // true = convert to float32 instead of integer
};

#include <cstring>
#include <algorithm>

inline void pack_sample(uint8_t* dst, int32_t sample, const PackParams& p) noexcept {
    if (p.output_float) {
        float f = (float)sample / (float)(1 << (p.valid_bits - 1));
        std::memcpy(dst, &f, 4);
        return;
    }
    switch (p.container_bits) {
    case 16: {
        int32_t clamped = std::clamp(sample, -32768, 32767);
        int16_t s16 = static_cast<int16_t>(clamped);
        dst[0] = (uint8_t)(s16 & 0xFF);
        dst[1] = (uint8_t)((s16 >> 8) & 0xFF);
        break;
    }
    case 24: {
        int32_t clamped = std::clamp(sample, -8388608, 8388607);
        dst[0] = (uint8_t)(clamped & 0xFF);
        dst[1] = (uint8_t)((clamped >> 8) & 0xFF);
        dst[2] = (uint8_t)((clamped >> 16) & 0xFF);
        break;
    }
    case 32: {
        int32_t s32;
        if (p.valid_bits == 24) {
            int32_t clamped = std::clamp(sample, -8388608, 8388607);
            s32 = clamped << 8;
        } else {
            int32_t clamped = std::clamp(sample, -32768, 32767);
            s32 = clamped << 16;
        }
        dst[0] = (uint8_t)(s32 & 0xFF);
        dst[1] = (uint8_t)((s32 >> 8) & 0xFF);
        dst[2] = (uint8_t)((s32 >> 16) & 0xFF);
        dst[3] = (uint8_t)((s32 >> 24) & 0xFF);
        break;
    }
    }
}

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

int main() {
    PackParams p;
    p.valid_bits = 16;
    p.container_bits = 16;
    p.output_float = true;

    int32_t s0 = 10000;
    int32_t s1 = -10000;
    const int32_t* ch0 = &s0;
    const int32_t* ch1 = &s1;
    const int32_t* samples[2] = { ch0, ch1 };

    uint8_t dst[64] = {0};
    size_t written = pack_frames(dst, samples, 2, 1, p);
    std::cout << "pack_frames written: " << written << " bytes" << std::endl;

    const float* f = reinterpret_cast<const float*>(dst);
    std::cout << "Float 0: " << f[0] << " (expected " << (10000.0f / 32768.0f) << ")" << std::endl;
    std::cout << "Float 1: " << f[1] << " (expected " << (-10000.0f / 32768.0f) << ")" << std::endl;

    return 0;
}
