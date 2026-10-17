#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include "d:/AudioPlayer/HiResPlayer/src/HiResAudioEngine/src/AudioResampler.h"

int main() {
    AudioResampler resampler;
    // Downsample 96000 -> 48000
    resampler.Reset(96000, 48000);

    // Generate 30kHz sine at 96kHz (Nyquist is 48kHz, target Nyquist is 24kHz)
    constexpr size_t kFrames = 96000; // 1 second
    std::vector<float> in_block(kFrames * 2);
    std::vector<float> out_block(kFrames * 2);

    double phase = 0.0;
    double freq = 30000.0; // 30 kHz
    double two_pi = 6.283185307179586;

    for (size_t i = 0; i < kFrames; ++i) {
        float s = 0.5f * static_cast<float>(std::sin(phase)); // 0.5 peak
        in_block[i * 2]     = s;
        in_block[i * 2 + 1] = s;
        phase += two_pi * freq / 96000.0;
        if (phase > two_pi) phase -= two_pi;
    }

    size_t out_frames = resampler.Process(in_block.data(), kFrames, out_block.data(), kFrames);

    // Measure RMS of output after initial settling (skip first 200 samples)
    double sum_sq = 0.0;
    size_t count = 0;
    for (size_t i = 200; i < out_frames - 200; ++i) {
        float s = out_block[i * 2];
        sum_sq += s * s;
        count++;
    }
    double rms = std::sqrt(sum_sq / count);
    double db_rel_fullscale = 20.0 * std::log10(rms / 0.35355339); // 0.5 peak sine RMS is ~0.35355

    std::cout << "AUD-08 Benchmark: 30kHz at 96kHz downsampled to 48kHz:" << std::endl;
    std::cout << "Output RMS: " << rms << std::endl;
    std::cout << "Attenuation relative to input: " << db_rel_fullscale << " dB" << std::endl;

    if (db_rel_fullscale < -100.0) {
        std::cout << "PASS: Stopband alias rejection is >= 100 dB! (AUD-08 / QA-05 satisfied)" << std::endl;
        return 0;
    } else {
        std::cout << "FAIL: Rejection insufficient: " << db_rel_fullscale << " dB" << std::endl;
        return 1;
    }
}
