#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <propsys.h>
#include <string>
#include "../include/hires_audio_api.h"

class BluetoothDeviceService {
public:
    BluetoothDeviceService();
    ~BluetoothDeviceService();

    // Inspect an IMMDevice endpoint and populate full Bluetooth device info
    static bool InspectEndpoint(
        IMMDevice*                device,
        IAudioClient*             client,
        HP_BluetoothDeviceInfoV1* out_info
    );

    // Helper: Query internal Bluetooth radio adapter name
    static std::wstring QuerySystemBluetoothAdapterName();

    // Helper: Check if Alternative A2DP Driver is installed and query its configured codec
    static bool CheckAlternativeA2dpDriver(
        const std::wstring& device_address,
        std::wstring&       out_driver_name,
        HP_BluetoothCodec&  out_codec,
        uint32_t&           out_bitrate
    );

    // Build human-readable status summary for Bluetooth Fidelity status
    static void BuildFidelitySummary(
        const HP_BluetoothDeviceInfoV1& dev_info,
        uint32_t                        source_rate,
        uint32_t                        source_bits,
        bool                            pcm_transparent,
        uint32_t                        format_match_state,
        wchar_t*                        out_buf,
        size_t                          buf_size
    );

    // Codec name to human-readable string
    static const wchar_t* CodecToString(HP_BluetoothCodec codec);

    // Confidence to human-readable string
    static const wchar_t* ConfidenceToString(HP_TelemetryConfidence conf);

    // Profile to human-readable string
    static const wchar_t* ProfileToString(HP_BluetoothProfile profile);
};
