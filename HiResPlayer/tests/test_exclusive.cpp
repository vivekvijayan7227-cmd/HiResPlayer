#include <iostream>
#include <vector>
#include "../src/HiResAudioEngine/src/PcmSamplePacker.h"

int main() {
    // 16-bit Exclusive
    PackParams p16;
    p16.valid_bits = 16;
    p16.container_bits = 16;
    p16.output_float = false;

    int32_t s0 = 10000;
    int32_t s1 = -10000;
    const int32_t* ch0 = &s0;
    const int32_t* ch1 = &s1;
    const int32_t* samples[2] = { ch0, ch1 };

    uint8_t dst16[64] = {0};
    size_t w16 = pack_frames(dst16, samples, 2, 1, p16);
    const int16_t* s16 = reinterpret_cast<const int16_t*>(dst16);
    std::cout << "16-bit Exclusive: written=" << w16 << " s0=" << s16[0] << " s1=" << s16[1] << std::endl;

    // 24-bit Exclusive
    PackParams p24;
    p24.valid_bits = 24;
    p24.container_bits = 32;
    p24.output_float = false;

    int32_t s24_0 = 500000;
    int32_t s24_1 = -500000;
    const int32_t* ch24_0 = &s24_0;
    const int32_t* ch24_1 = &s24_1;
    const int32_t* samples24[2] = { ch24_0, ch24_1 };

    uint8_t dst24[64] = {0};
    size_t w24 = pack_frames(dst24, samples24, 2, 1, p24);
    const int32_t* s32 = reinterpret_cast<const int32_t*>(dst24);
    std::cout << "24-bit Exclusive: written=" << w24 << " s0=" << (s32[0] >> 8) << " s1=" << (s32[1] >> 8) << std::endl;

    return 0;
}
