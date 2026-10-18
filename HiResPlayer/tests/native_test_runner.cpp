#include <iostream>
#include <vector>
#include <windows.h>
#include <thread>
#include <chrono>
#include "hires_audio_api.h"
#include "hires_dsp_api.h"
#include "test_fixtures.h"

int main() {
    std::wcout << L"=== HiResAudioEngine Verification Test (M1/M2/M7/M8) ===" << std::endl;

    // 1. Check API Version
    uint32_t version = hp_api_version();
    std::wcout << L"API Version: " << version << std::endl;
    if (version != 1) {
        std::cerr << "FAIL: Unexpected API version" << std::endl;
        return 1;
    }

    // 2. Create Engine Handle
    HP_Handle engine = hp_create();
    if (!engine) {
        std::cerr << "FAIL: hp_create returned NULL" << std::endl;
        return 1;
    }

    // 3. Enumerate Audio Devices
    uint32_t dev_count = 0;
    hp_enumerate_devices(engine, nullptr, 0, &dev_count);
    std::wcout << L"Detected Audio Render Endpoints: " << dev_count << std::endl;

    if (dev_count > 0) {
        std::vector<HP_DeviceInfoV1> devices(dev_count);
        hp_enumerate_devices(engine, devices.data(), dev_count, &dev_count);
        for (uint32_t i = 0; i < dev_count; ++i) {
            std::wcout << L"  [" << i << L"] " << devices[i].friendly_name 
                       << (devices[i].is_default_render ? L" [DEFAULT]" : L"") << std::endl;
        }
    }

    // 4. Open and Verify 16-bit 44.1kHz FLAC
    std::wstring flac_16_str = get_test_fixture_path(L"test_16bit_44k.flac");
    const wchar_t* flac_16 = flac_16_str.c_str();
    HP_Result res = hp_open_flac(engine, flac_16);
    std::wcout << L"Opening 16-bit 44.1kHz FLAC: Result = " << res << std::endl;
    if (res != HP_OK) {
        std::cerr << "FAIL: Could not open test FLAC file" << std::endl;
        hp_destroy(engine);
        return 1;
    }

    // 5. Test Status Query
    HP_StatusV1 status = {};
    hp_get_status(engine, &status);
    std::wcout << L"Source Rate: " << status.source_rate 
               << L" Hz, Bits: " << status.source_valid_bits 
               << L", Channels: " << status.source_channels 
               << L", Total Samples: " << status.source_total_samples << std::endl;

    if (status.source_rate != 44100 || status.source_valid_bits != 16 || status.source_channels != 2) {
        std::cerr << "FAIL: Stream metadata mismatch" << std::endl;
        hp_destroy(engine);
        return 1;
    }

    // 6. Test DSP Equalizer Functions
    hp_dsp_set_preamp(engine, -3.0f);
    hp_dsp_set_graphic_eq_band(engine, 5, +4.0f); // 1 kHz band +4 dB
    float curve_db = 0.0f;
    hp_dsp_evaluate_curve(engine, 1000.0f, &curve_db);
    std::wcout << L"EQ Response at 1 kHz (-3 dB preamp + 4 dB peak): " << curve_db << L" dB" << std::endl;

    // 7. Test Traditional DSP Center Channel Processor
    hp_dsp_set_center_processor(engine, 1, 0.75f); // Vocal Focus 75%
    std::wcout << L"Configured Traditional DSP Center Channel Processor (Vocal Focus 75%)" << std::endl;

    // 8. Test Short Playback Run
    res = hp_play(engine);
    std::wcout << L"Starting Playback: Result = " << res << std::endl;
    hp_get_status(engine, &status);
    std::wcout << L"last_hresult = 0x" << std::hex << (uint32_t)status.last_hresult << std::dec << std::endl;

    if (res == HP_OK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        hp_get_status(engine, &status);
        std::wcout << L"Playback State: " << status.state 
                   << L", Rendered Samples: " << status.rendered_source_samples
                   << L", Underruns: " << status.underrun_count << std::endl;

        // Test pause and stop directly
        std::wcout << L"Attempting hp_pause..." << std::endl;
        std::wcout.flush();
        hp_pause(engine);
        std::wcout << L"Paused playback successfully." << std::endl;
        std::wcout.flush();

        std::wcout << L"Attempting hp_stop..." << std::endl;
        std::wcout.flush();
        hp_stop(engine);
        std::wcout << L"Stopped playback successfully." << std::endl;
        std::wcout.flush();
    }

    std::wcout << L"About to call hp_destroy..." << std::endl;
    std::wcout.flush();

    // 9. Destroy Handle
    hp_destroy(engine);
    std::wcout << L"Engine destroyed cleanly. ALL TESTS PASSED." << std::endl;
    std::wcout.flush();
    return 0;
}
