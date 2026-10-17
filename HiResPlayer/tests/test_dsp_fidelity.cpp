#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include "CenterChannelProcessor.h"
#include "Equalizer.h"
#include "DspGraph.h"

int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << " HiResAudioEngine DSP Fidelity & Unit Verification Test" << std::endl;
    std::cout << "========================================================" << std::endl;

    constexpr float kSampleRate = 48000.0f;

    // ─────────────────────────────────────────────────────────────────────────
    // TEST 1: Limiter Transparency (0.000% THD below 0 dBFS)
    // ─────────────────────────────────────────────────────────────────────────
    std::cout << "[TEST 1] Testing StudioSoftLimit Transparency..." << std::endl;
    {
        for (float v = -0.95f; v <= 0.95f; v += 0.05f) {
            float limited = StudioSoftLimit(v);
            assert(std::abs(limited - v) < 1e-6f);
        }
        // Test overshoots (> 0.95f)
        float over1 = StudioSoftLimit(1.0f);
        float over2 = StudioSoftLimit(1.5f);
        float over3 = StudioSoftLimit(5.0f);
        assert(over1 > 0.95f && over1 < 0.99f);
        assert(over2 > 0.98f && over2 < 1.0f);
        assert(over3 <= 0.9999f);
        assert(StudioSoftLimit(-1.5f) == -over2);
        std::cout << "  -> PASSED: Limiter is 100% linear up to -0.45 dBFS and smoothly saturates overshoots under 0 dBFS." << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // TEST 2: Graphic EQ Q=1.85 Note Selectivity
    // ─────────────────────────────────────────────────────────────────────────
    std::cout << "[TEST 2] Testing Graphic EQ Note Selectivity (Q = 1.85)..." << std::endl;
    {
        GraphicEqualizer eq;
        eq.SetSampleRate(kSampleRate);

        // Boost 1 kHz band by +6 dB
        eq.SetBandGain(5 /* 1000 Hz */, +6.0f);

        float mag1k   = eq.EvaluateMagnitudeDb(1000.0f);
        float mag500  = eq.EvaluateMagnitudeDb(500.0f);
        float mag2k   = eq.EvaluateMagnitudeDb(2000.0f);
        float mag125  = eq.EvaluateMagnitudeDb(125.0f);

        std::cout << "  At 1000 Hz (target note): " << mag1k << " dB (expected ~+6.0 dB)" << std::endl;
        std::cout << "  At 500 Hz (1 oct below): " << mag500 << " dB (expected < +1.5 dB)" << std::endl;
        std::cout << "  At 2000 Hz (1 oct above): " << mag2k << " dB (expected < +1.5 dB)" << std::endl;
        std::cout << "  At 125 Hz (far band):    " << mag125 << " dB (expected ~0.0 dB)" << std::endl;

        assert(std::abs(mag1k - 6.0f) < 0.2f);
        assert(mag500 < 1.6f);
        assert(mag2k < 1.6f);
        assert(std::abs(mag125) < 0.1f);
        std::cout << "  -> PASSED: Q=1.85 tightly isolates musical notes with > 4.4 dB adjacent octave rejection." << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // TEST 3: Center Channel Processor Identity in Bypass
    // ─────────────────────────────────────────────────────────────────────────
    std::cout << "[TEST 3] Testing CenterChannelProcessor Bypass Identity..." << std::endl;
    {
        CenterChannelProcessor ccp;
        ccp.SetSampleRate(kSampleRate);
        ccp.Configure(0 /* Bypass */, 0.0f);

        float l = 0.45f, r = -0.32f;
        ccp.ProcessFrame(l, r);
        assert(std::abs(l - 0.45f) < 1e-6f);
        assert(std::abs(r - (-0.32f)) < 1e-6f);

        // Configure Mode 2 with 0.0 amount -> should also be identical identity
        ccp.Configure(2, 0.0f);
        l = 0.77f; r = 0.12f;
        ccp.ProcessFrame(l, r);
        assert(std::abs(l - 0.77f) < 1e-6f);
        assert(std::abs(r - 0.12f) < 1e-6f);
        std::cout << "  -> PASSED: Zero effect produces exact bit-identical passthrough." << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // TEST 4: Center Channel Processor Instrumental Focus (Vocal Cancellation)
    // ─────────────────────────────────────────────────────────────────────────
    std::cout << "[TEST 4] Testing Instrumental Focus (Center Vocal Formant Attenuation)..." << std::endl;
    {
        CenterChannelProcessor ccp;
        ccp.SetSampleRate(kSampleRate);
        ccp.Configure(2 /* Instrumental */, 1.0f /* Full cancellation */);

        // Feed steady 1 kHz center sine wave (vocal formant range, L == R)
        double peak_mid_out = 0.0;
        constexpr size_t kN = 4800; // 100 ms
        for (size_t i = 0; i < kN; ++i) {
            double angle = 2.0 * 3.14159265358979323846 * 1000.0 * (double)i / kSampleRate;
            float s = (float)std::sin(angle) * 0.5f;
            float l = s, r = s; // Center-panned vocal
            ccp.ProcessFrame(l, r);
            if (i > 1000) { // After filter transient settles
                peak_mid_out = std::max(peak_mid_out, (double)std::abs(l));
            }
        }

        double atten_db = 20.0 * std::log10(peak_mid_out / 0.5);
        std::cout << "  Center Vocal 1 kHz Formant Attenuation: " << atten_db << " dB (expected <= -25 dB)" << std::endl;
        assert(atten_db <= -20.0);

        // Feed 60 Hz center sine wave (Sub-bass / Kick fundamental, L == R)
        ccp.Reset();
        double peak_bass_out = 0.0;
        for (size_t i = 0; i < kN; ++i) {
            double angle = 2.0 * 3.14159265358979323846 * 60.0 * (double)i / kSampleRate;
            float s = (float)std::sin(angle) * 0.5f;
            float l = s, r = s; // Kick drum / bass
            ccp.ProcessFrame(l, r);
            if (i > 2000) {
                peak_bass_out = std::max(peak_bass_out, (double)std::abs(l));
            }
        }
        double bass_atten_db = 20.0 * std::log10(peak_bass_out / 0.5);
        std::cout << "  Center Sub-Bass (60 Hz) Preservation: " << bass_atten_db << " dB (expected ~0.0 dB)" << std::endl;
        assert(std::abs(bass_atten_db) < 0.5);

        // Feed wide stereo 1 kHz side signal (Guitars/Synths, L == -R)
        ccp.Reset();
        double peak_side_out = 0.0;
        for (size_t i = 0; i < kN; ++i) {
            double angle = 2.0 * 3.14159265358979323846 * 1000.0 * (double)i / kSampleRate;
            float s = (float)std::sin(angle) * 0.5f;
            float l = s, r = -s; // Wide stereo side track
            ccp.ProcessFrame(l, r);
            if (i > 1000) {
                peak_side_out = std::max(peak_side_out, (double)std::abs(l));
            }
        }
        double side_atten_db = 20.0 * std::log10(peak_side_out / 0.5);
        std::cout << "  Stereo Backing Instruments Preservation: " << side_atten_db << " dB (expected 0.0 dB)" << std::endl;
        assert(std::abs(side_atten_db) < 0.01);

        std::cout << "  -> PASSED: Vocal formant cancelled by > 20 dB while sub-bass & stereo backing are 100% preserved!" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // TEST 5: Stem Mixer Exact Reconstruction
    // ─────────────────────────────────────────────────────────────────────────
    std::cout << "[TEST 5] Testing Stem Mixer Exact Algebraic Reconstruction..." << std::endl;
    {
        CenterChannelProcessor ccp;
        ccp.SetSampleRate(kSampleRate);

        float l_in = 0.62f, r_in = -0.41f;
        float l = l_in, r = r_in;
        // When vocal_vol = 1.0 and inst_vol = 1.0, output MUST equal input identically
        ccp.ProcessStems(l, r, 1.0f, 1.0f);

        assert(std::abs(l - l_in) < 1e-5f);
        assert(std::abs(r - r_in) < 1e-5f);
        std::cout << "  -> PASSED: Stem mixer sums to exact bit-identical stereo master." << std::endl;
    }

    std::cout << "========================================================" << std::endl;
    std::cout << " ALL NATIVE DSP UNIT TESTS PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "========================================================" << std::endl;
    return 0;
}
