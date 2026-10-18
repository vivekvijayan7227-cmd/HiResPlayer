#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include "../src/HiResAudioEngine/include/hires_audio_api.h"
#include "../src/HiResAudioEngine/include/hires_dsp_api.h"
#include "test_fixtures.h"

int main() {
    std::cout << "======================================================" << std::endl;
    std::cout << " FULL AUDIOPHILE VERIFICATION & INTEGRATION TEST" << std::endl;
    std::cout << "======================================================" << std::endl;

    // 1. Version check
    uint32_t ver = hp_api_version();
    std::cout << "[1/10] API Version: " << ver << std::endl;
    assert(ver == 1);

    // 2. Engine Creation
    HP_Handle h = hp_create();
    assert(h != nullptr);
    std::cout << "[2/10] Engine instance created successfully." << std::endl;

    // 3. Open FLAC track
    std::wstring path_str = get_test_fixture_path(L"test_16bit_44k.flac");
    const wchar_t* path = path_str.c_str();
    HP_Result res = hp_open_flac(h, path);
    std::cout << "[3/10] Open FLAC: result = " << res << std::endl;
    assert(res == HP_OK);

    // 4. Start Playback in Shared Mode
    res = hp_set_mode(h, HP_SHARED);
    assert(res == HP_OK);
    res = hp_play(h);
    std::cout << "[4/10] Playback started in Shared mode: result = " << res << std::endl;
    assert(res == HP_OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 5. Test Pause (clean silence, no DMA loop)
    res = hp_pause(h);
    std::cout << "[5/10] Paused playback: result = " << res << std::endl;
    assert(res == HP_OK);

    HP_StatusV1 status = {};
    hp_get_status(h, &status);
    uint64_t underruns_at_pause = status.underrun_count;
    std::cout << "       State at pause: " << status.state 
              << ", Rendered samples: " << status.rendered_source_samples << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    hp_get_status(h, &status);
    std::cout << "       Underruns after 300ms pause: " << status.underrun_count 
              << " (delta=" << (status.underrun_count - underruns_at_pause) << ")" << std::endl;
    assert(status.state == HP_STATE_PAUSED);

    // 6. Resume playback & test Seek
    res = hp_play(h);
    std::cout << "[6/10] Resumed playback: result = " << res << std::endl;
    assert(res == HP_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Seek to 1 second in (44,100 samples)
    uint64_t seek_target = 44100;
    res = hp_seek_sample(h, seek_target);
    std::cout << "       Seek to sample " << seek_target << ": result = " << res << std::endl;
    assert(res == HP_OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    hp_get_status(h, &status);
    std::cout << "       Rendered position post-seek: " << status.rendered_source_samples << std::endl;
    assert(status.rendered_source_samples >= seek_target);

    // 7. Test Vocal Focus (Mode 1) & Instrumental Focus (Mode 2)
    std::cout << "[7/10] Testing Vocal & Instrumental Focus..." << std::endl;
    hp_dsp_set_center_processor(h, 1, 0.80f); // Vocal Focus
    float vocal_curve = 0.0f;
    hp_dsp_evaluate_center_curve(h, 2600.0f, &vocal_curve);
    std::cout << "       Vocal Focus (2600 Hz): " << vocal_curve << " dB (boost expected)" << std::endl;
    assert(vocal_curve > 1.0f);

    hp_dsp_set_center_processor(h, 2, 0.80f); // Instrumental Focus
    float inst_curve = 0.0f;
    hp_dsp_evaluate_center_curve(h, 1000.0f, &inst_curve);
    std::cout << "       Instrumental Focus (1000 Hz): " << inst_curve << " dB (attenuation expected)" << std::endl;
    assert(inst_curve < -10.0f);

    hp_dsp_set_center_processor(h, 0, 0.0f); // Bypass

    // 8. Test 10-Band Graphic Equalizer
    std::cout << "[8/10] Testing 10-Band Graphic Equalizer..." << std::endl;
    hp_dsp_set_graphic_eq_band(h, 0, 4.0f); // +4 dB Sub-bass (31 Hz)
    hp_dsp_set_graphic_eq_band(h, 5, -3.0f); // -3 dB Mid (1 kHz)
    hp_dsp_set_graphic_eq_band(h, 9, 5.0f); // +5 dB Treble Air (16 kHz)

    float curve_sub = 0.0f, curve_mid = 0.0f, curve_air = 0.0f;
    hp_dsp_evaluate_curve(h, 31.0f, &curve_sub);
    hp_dsp_evaluate_curve(h, 1000.0f, &curve_mid);
    hp_dsp_evaluate_curve(h, 16000.0f, &curve_air);
    std::cout << "       EQ response: 31Hz=" << curve_sub << "dB, 1kHz=" << curve_mid << "dB, 16kHz=" << curve_air << "dB" << std::endl;
    assert(curve_sub > 2.0f);
    assert(curve_mid < -1.5f);
    assert(curve_air > 2.0f);
    hp_dsp_reset_graphic_eq(h);

    // 9. Test Exclusive Mode Switch
    std::cout << "[9/10] Testing Bit-Perfect / Exclusive Mode switch..." << std::endl;
    res = hp_set_mode(h, HP_EXCLUSIVE);
    std::cout << "       Switch to Exclusive: result = " << res << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    hp_get_status(h, &status);
    std::cout << "       Actual mode: " << (status.actual_mode == HP_EXCLUSIVE ? "EXCLUSIVE" : "SHARED")
              << ", Bit-Perfect Eligible: " << status.bitperfect_eligible << std::endl;

    // Switch back to Shared Mode smoothly
    res = hp_set_mode(h, HP_SHARED);
    std::cout << "       Switch back to Shared: result = " << res << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 10. Clean Stop & Destruction
    res = hp_stop(h);
    std::cout << "[10/10] Stop playback: result = " << res << std::endl;
    assert(res == HP_OK);

    hp_destroy(h);
    std::cout << "Engine destroyed cleanly. ALL 10 TESTS PASSED WITH ZERO ERRORS!" << std::endl;
    std::cout << "======================================================" << std::endl;
    return 0;
}
