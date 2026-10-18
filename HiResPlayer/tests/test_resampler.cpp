#include <iostream>
#include <vector>
#include <cmath>
#include "../src/HiResAudioEngine/src/AudioResampler.h"

int main() {
    AudioResampler resampler;
    resampler.Reset(44100, 48000);

    // Generate 440Hz stereo sine blocks at 44.1kHz
    constexpr size_t kBlockSize = 4096;
    std::vector<float> in_block(kBlockSize * 2);
    std::vector<float> out_block(kBlockSize * 4);

    double phase = 0.0;
    double freq = 440.0;
    double two_pi = 6.283185307179586;

    for (int block = 0; block < 5; ++block) {
        for (size_t i = 0; i < kBlockSize; ++i) {
            float s = static_cast<float>(std::sin(phase));
            in_block[i * 2]     = s;
            in_block[i * 2 + 1] = s;
            phase += two_pi * freq / 44100.0;
            if (phase > two_pi) phase -= two_pi;
        }

        size_t out_frames = resampler.Process(in_block.data(), kBlockSize, out_block.data(), out_block.size() / 2);
        std::cout << "Block " << block << ": in=" << kBlockSize << " out=" << out_frames << std::endl;
        std::cout << "  First out: " << out_block[0] << " Mid out: " << out_block[out_frames] << " Last out: " << out_block[(out_frames-1)*2] << std::endl;
    }

    return 0;
}
