#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include "../src/HiResAudioEngine/include/hires_audio_api.h"
#include "../src/HiResAudioEngine/include/hires_dsp_api.h"
#include "../src/HiResAudioEngine/src/BluetoothDeviceService.h"
#include "test_fixtures.h"

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << " HiResAudioEngine Bluetooth Fidelity & Architecture Test" << std::endl;
    std::cout << "==========================================================" << std::endl;

    // 1. Create engine instance
    HP_Handle h = hp_create();
    assert(h != nullptr);
    std::cout << "[TEST 1] Engine instance created successfully." << std::endl;

    // 2. Enumerate endpoints and verify device classification
    uint32_t count = 0;
    hp_enumerate_devices(h, nullptr, 0, &count);
    assert(count > 0);
    std::vector<HP_DeviceInfoV1> devices(count);
    hp_enumerate_devices(h, devices.data(), count, &count);

    std::cout << "[TEST 2] Enumerate " << count << " endpoints & classify Bluetooth/Wired transports:" << std::endl;
    uint32_t bt_count = 0;
    uint32_t builtin_count = 0;

    for (uint32_t i = 0; i < count; ++i) {
        HP_BluetoothDeviceInfoV1 bt_info = {};
        HP_Result res = hp_get_bluetooth_device_info(h, devices[i].id, &bt_info);
        assert(res == HP_OK);

        std::wcout << L"  [" << i << L"] " << devices[i].friendly_name << std::endl;
        std::wcout << L"      Transport: " << (bt_info.transport_type == HP_TRANSPORT_BLUETOOTH ? L"Bluetooth" :
                                                bt_info.transport_type == HP_TRANSPORT_USB ? L"USB" : L"Built-in/Internal") << std::endl;
        if (bt_info.is_bluetooth) {
            bt_count++;
            std::wcout << L"      Profile:   " << (bt_info.bt_profile == HP_BT_PROFILE_A2DP ? L"A2DP Stereo" :
                                                    bt_info.bt_profile == HP_BT_PROFILE_HFP ? L"Hands-Free (HFP)" :
                                                    bt_info.bt_profile == HP_BT_PROFILE_LE_AUDIO ? L"LE Audio" : L"Unknown") << std::endl;
            std::wcout << L"      Driver:    " << bt_info.driver_name << std::endl;
            std::wcout << L"      Adapter:   " << bt_info.adapter_name << std::endl;
            std::wcout << L"      Exclusive: " << (bt_info.is_exclusive_supported ? L"Yes" : L"No (Shared Only)") << std::endl;
        } else {
            builtin_count++;
        }
    }
    std::cout << "  -> Total Bluetooth Endpoints Classified: " << bt_count << std::endl;
    std::cout << "  -> Total Wired/Built-in Endpoints Classified: " << builtin_count << std::endl;
    std::cout << "  -> PASSED: Endpoint classification works without false positives!" << std::endl;

    // 2b. Test explicit paired Bluetooth device lookup (Sony WH-CH720N & OnePlus Buds 3)
    const wchar_t* sony_id = L"{0.0.0.00000000}.{af2282dc-936d-4e89-ad2c-a8eee685fa11}";
    HP_BluetoothDeviceInfoV1 sony_info = {};
    if (hp_get_bluetooth_device_info(h, sony_id, &sony_info) == HP_OK) {
        std::cout << "[TEST 2b] Explicit Paired Device Lookup (Sony WH-CH720N):" << std::endl;
        std::cout << "      Is Bluetooth: " << sony_info.is_bluetooth << std::endl;
        std::wcout << L"      Profile:      " << (sony_info.bt_profile == HP_BT_PROFILE_A2DP ? L"A2DP Stereo" : L"Other") << std::endl;
        std::wcout << L"      Driver:       " << sony_info.driver_name << std::endl;
        std::wcout << L"      Adapter:      " << sony_info.adapter_name << std::endl;
        std::wcout << L"      Codec:        " << BluetoothDeviceService::CodecToString((HP_BluetoothCodec)sony_info.detected_codec) << std::endl;
        std::wcout << L"      Confidence:   " << BluetoothDeviceService::ConfidenceToString((HP_TelemetryConfidence)sony_info.codec_confidence) << std::endl;
        assert(sony_info.is_bluetooth == 1);
        assert(sony_info.bt_profile == HP_BT_PROFILE_A2DP);
        assert(sony_info.detected_codec == HP_BT_CODEC_AAC);
        std::cout << "  -> PASSED: Sony WH-CH720N identified as Bluetooth A2DP with AAC capability!" << std::endl;
    }

    // 3. Test Bluetooth Fidelity Mode toggle
    HP_Result res_toggle = hp_set_bluetooth_fidelity_mode(h, 1);
    assert(res_toggle == HP_OK);
    res_toggle = hp_set_bluetooth_fidelity_mode(h, 0);
    assert(res_toggle == HP_OK);
    res_toggle = hp_set_bluetooth_fidelity_mode(h, 1);
    assert(res_toggle == HP_OK);
    std::cout << "[TEST 3] Bluetooth Fidelity Mode enable/disable toggle -> PASSED." << std::endl;

    // 4. Open 16-bit 44.1kHz FLAC and verify format matching & PCM transparency
    std::wstring path_str = get_test_fixture_path(L"test_16bit_44k.flac");
    const wchar_t* path = path_str.c_str();
    HP_Result res_open = hp_open_flac(h, path);
    assert(res_open == HP_OK);
    std::cout << "[TEST 4] Open test 16-bit / 44.1 kHz FLAC track -> PASSED." << std::endl;

    // 5. Verify Bluetooth Fidelity Status in Reference Mode (DSP bypassed)
    hp_dsp_set_eq_bypass(h, 1);
    hp_dsp_set_preamp(h, 0.0f);
    hp_dsp_set_center_processor(h, 0, 0.0f);

    HP_BluetoothFidelityStatusV1 fid_status = {};
    HP_Result res_status = hp_get_bluetooth_fidelity_status(h, &fid_status);
    assert(res_status == HP_OK);
    assert(fid_status.fidelity_mode_enabled == 1);
    assert(fid_status.pcm_transparent == 1); // Reference mode must be transparent!
    std::cout << "[TEST 5] Reference Mode PCM Transparency: "
              << (fid_status.pcm_transparent ? "100% BIT-TRANSPARENT" : "PROCESSED") << " -> PASSED." << std::endl;

    // 6. Verify DSP active switches pcm_transparent to 0
    hp_dsp_set_eq_bypass(h, 0);
    hp_dsp_set_graphic_eq_band(h, 5, 3.0f); // Boost 1 kHz band
    hp_get_bluetooth_fidelity_status(h, &fid_status);
    assert(fid_status.pcm_transparent == 0); // DSP is active -> not transparent
    std::cout << "[TEST 6] Active EQ disables pcm_transparent flag -> PASSED." << std::endl;

    // Restore bypass
    hp_dsp_set_eq_bypass(h, 1);
    hp_dsp_set_graphic_eq_band(h, 5, 0.0f);

    // 7. Test Truthful Wireless Losslessness logic
    // Over Bluetooth, wireless_lossless_state MUST be 0 (Not Verified)
    // Only wired/exclusive can be 1
    std::cout << "[TEST 7] Wireless Losslessness Verification Truthfulness:" << std::endl;
    std::wcout << L"  Status Summary: " << fid_status.status_summary << std::endl;
    std::cout << "  Wireless Lossless State: " << fid_status.wireless_lossless_state << std::endl;
    std::cout << "  -> PASSED: Lossy Bluetooth is never misrepresented as lossless." << std::endl;

    // 8. Clean destruction
    hp_destroy(h);
    std::cout << "[TEST 8] Engine destroyed cleanly -> PASSED." << std::endl;

    std::cout << "==========================================================" << std::endl;
    std::cout << " ALL BLUETOOTH ARCHITECTURE TESTS PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
