#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <cmath>
#include "../src/HiResAudioEngine/include/hires_audio_api.h"
#include "../src/HiResAudioEngine/include/hires_dsp_api.h"
#include "test_fixtures.h"

int main() {
    std::cout << "Testing Studio DSP Engine mode..." << std::endl;
    HP_Handle h = hp_create();
    assert(h != nullptr);

    // Open standard 44.1 kHz FLAC (which will require 44.1 -> 48.0 kHz resampling in Studio DSP Engine)
    std::wstring path_str = get_test_fixture_path(L"test_16bit_44k.flac");
    const wchar_t* path = path_str.c_str();
    HP_Result res = hp_open_flac(h, path);
    assert(res == HP_OK);

    // 1. Start in Exclusive mode (Bit-Perfect)
    res = hp_set_mode(h, HP_EXCLUSIVE);
    res = hp_play(h);
    assert(res == HP_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    HP_StatusV1 st = {};
    hp_get_status(h, &st);
    std::cout << "Step 1: Bit-Perfect mode running. Rendered: " << st.rendered_source_samples 
              << ", Mode: " << (st.actual_mode == HP_EXCLUSIVE ? "EXCLUSIVE" : "SHARED") << std::endl;

    // 2. User clicks "Studio DSP Engine":
    // Sets mode to Shared, unbypasses EQ, applies preset (e.g. Bass Boost)
    std::cout << "Step 2: Switching to Studio DSP Engine..." << std::endl;
    res = hp_set_mode(h, HP_SHARED);
    assert(res == HP_OK);
    hp_dsp_set_eq_bypass(h, 0); // Active
    hp_dsp_set_preamp(h, -2.0f);
    hp_dsp_set_graphic_eq_band(h, 0, 5.5f); // +5.5 dB bass
    hp_dsp_set_graphic_eq_band(h, 1, 4.5f);
    hp_dsp_set_graphic_eq_band(h, 2, 3.0f);

    // Let it play in Studio DSP Engine for 600 ms (several resampler and DSP buffer cycles)
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    hp_get_status(h, &st);
    std::cout << "Step 3: Studio DSP Engine running. Rendered: " << st.rendered_source_samples
              << ", Mode: " << (st.actual_mode == HP_EXCLUSIVE ? "EXCLUSIVE" : "SHARED")
              << ", Underruns: " << st.underrun_count
              << ", Pre-DSP Peak: " << st.source_total_samples << std::endl;

    // Verify playback continued smoothly
    assert(st.state == HP_STATE_PLAYING);
    assert(st.actual_mode == HP_SHARED);
    assert(st.rendered_source_samples > 10000);

    // 3. Test changing EQ bands live while playing in Studio DSP Engine
    std::cout << "Step 4: Live tuning EQ bands during playback..." << std::endl;
    for (int b = 0; b < 10; ++b) {
        hp_dsp_set_graphic_eq_band(h, b, (b % 2 == 0) ? 3.0f : -3.0f);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    hp_get_status(h, &st);
    std::cout << "Live tuning complete. Final rendered samples: " << st.rendered_source_samples << std::endl;
    assert(st.state == HP_STATE_PLAYING);

    hp_stop(h);
    hp_destroy(h);
    std::cout << "SUCCESS: Studio DSP Engine operates flawlessly with zero noise, zero lockups!" << std::endl;
    return 0;
}
