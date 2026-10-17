#include <iostream>
#include <vector>
#include <cmath>
#include "d:/AudioPlayer/HiResPlayer/src/HiResAudioEngine/src/PcmSamplePacker.h"
#include "d:/AudioPlayer/HiResPlayer/src/HiResAudioEngine/src/AudioResampler.h"

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
    std::cout << "Float 0: " << f[0] << std::endl;
    std::cout << "Float 1: " << f[1] << std::endl;

    return 0;
}
