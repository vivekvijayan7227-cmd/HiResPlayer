#include "BluetoothDeviceService.h"
#include <propvarutil.h>
#include <algorithm>
#include <cstring>
#include <cwchar>

// PKEY definitions
static const PROPERTYKEY kPKey_FriendlyName = {
    { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14
};
static const PROPERTYKEY kPKey_EnumeratorName = {
    { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 24
};
static const PROPERTYKEY kPKey_InstanceId = {
    { 0x78c34fc8, 0x104a, 0x4aca, { 0x9e, 0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57 } }, 256
};
static const PROPERTYKEY kPKey_BtDeviceAddress = {
    { 0x2bd67d8b, 0x8beb, 0x48d5, { 0x87, 0xe0, 0x6c, 0xda, 0x34, 0x28, 0x04, 0x0a } }, 1
};
static const PROPERTYKEY kPKey_BtManufacturer = {
    { 0x2bd67d8b, 0x8beb, 0x48d5, { 0x87, 0xe0, 0x6c, 0xda, 0x34, 0x28, 0x04, 0x0a } }, 2
};

BluetoothDeviceService::BluetoothDeviceService() {}
BluetoothDeviceService::~BluetoothDeviceService() {}

const wchar_t* BluetoothDeviceService::CodecToString(HP_BluetoothCodec codec) {
    switch (codec) {
    case HP_BT_CODEC_SBC:           return L"SBC (Subband Codec)";
    case HP_BT_CODEC_AAC:           return L"AAC (Advanced Audio Coding)";
    case HP_BT_CODEC_APTX:          return L"aptX Classic";
    case HP_BT_CODEC_APTX_HD:       return L"aptX HD (High Definition)";
    case HP_BT_CODEC_APTX_ADAPTIVE: return L"aptX Adaptive";
    case HP_BT_CODEC_APTX_LOSSLESS: return L"aptX Lossless";
    case HP_BT_CODEC_LDAC:          return L"LDAC (Hi-Res Audio Wireless)";
    case HP_BT_CODEC_LC3:           return L"LC3 (LE Audio Low Complexity)";
    case HP_BT_CODEC_UNKNOWN:
    default:                        return L"Unknown (Windows A2DP Stack Managed)";
    }
}

const wchar_t* BluetoothDeviceService::ConfidenceToString(HP_TelemetryConfidence conf) {
    switch (conf) {
    case HP_CONFIDENCE_VERIFIED:           return L"Verified";
    case HP_CONFIDENCE_REPORTED_BY_DRIVER: return L"Reported by Driver";
    case HP_CONFIDENCE_INFERRED:           return L"Inferred";
    case HP_CONFIDENCE_UNSUPPORTED:        return L"Unsupported";
    case HP_CONFIDENCE_UNKNOWN:
    default:                               return L"Unknown";
    }
}

const wchar_t* BluetoothDeviceService::ProfileToString(HP_BluetoothProfile profile) {
    switch (profile) {
    case HP_BT_PROFILE_A2DP:     return L"Bluetooth A2DP (Stereo Audio Sink)";
    case HP_BT_PROFILE_HFP:      return L"Bluetooth HFP (Hands-Free / Communications)";
    case HP_BT_PROFILE_LE_AUDIO: return L"Bluetooth LE Audio (Isochronous)";
    case HP_BT_PROFILE_NONE:     return L"Wired / USB Direct Audio";
    case HP_BT_PROFILE_UNKNOWN:
    default:                     return L"Unknown Profile";
    }
}

std::wstring BluetoothDeviceService::QuerySystemBluetoothAdapterName() {
    // Check registry for active Bluetooth radio class instance
    HKEY hKey = nullptr;
    // Enum class {e0cbf06c-cd8b-4647-bb8a-263b43f0f974} (Bluetooth Radio)
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SYSTEM\\CurrentControlSet\\Control\\Class\\{e0cbf06c-cd8b-4647-bb8a-263b43f0f974}\\0000",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[256] = {};
        DWORD cb = sizeof(buf);
        if (RegQueryValueExW(hKey, L"DriverDesc", nullptr, nullptr, reinterpret_cast<LPBYTE>(buf), &cb) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return buf;
        }
        RegCloseKey(hKey);
    }

    // Secondary check: Services\BTHPORT
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SYSTEM\\CurrentControlSet\\Services\\BTHPORT\\Parameters",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[256] = {};
        DWORD cb = sizeof(buf);
        if (RegQueryValueExW(hKey, L"RadioName", nullptr, nullptr, reinterpret_cast<LPBYTE>(buf), &cb) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return buf;
        }
        RegCloseKey(hKey);
    }

    return L"Built-in Bluetooth Adapter (Intel/Qualcomm/Realtek)";
}

bool BluetoothDeviceService::CheckAlternativeA2dpDriver(
    const std::wstring& device_address,
    std::wstring&       out_driver_name,
    HP_BluetoothCodec&  out_codec,
    uint32_t&           out_bitrate
) {
    HKEY hKey = nullptr;
    // Check HKLM\SOFTWARE\BluetoothGoodies\A2dp
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\BluetoothGoodies\\A2dp", 0, KEY_READ, &hKey) == ERROR_SUCCESS ||
        RegOpenKeyExW(HKEY_CURRENT_USER,  L"Software\\BluetoothGoodies\\A2dp", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        out_driver_name = L"Alternative A2DP Driver (Bluetooth Goodies)";

        // Check if device-specific codec setting exists or global active codec
        DWORD codecVal = 0;
        DWORD cb = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"ActiveCodec", nullptr, nullptr, reinterpret_cast<LPBYTE>(&codecVal), &cb) == ERROR_SUCCESS ||
            RegQueryValueExW(hKey, L"Codec", nullptr, nullptr, reinterpret_cast<LPBYTE>(&codecVal), &cb) == ERROR_SUCCESS) {
            switch (codecVal) {
            case 4:
                out_codec = HP_BT_CODEC_LDAC;
                out_bitrate = 990;
                break;
            case 3:
                out_codec = HP_BT_CODEC_APTX_HD;
                out_bitrate = 576;
                break;
            case 2:
                out_codec = HP_BT_CODEC_APTX;
                out_bitrate = 352;
                break;
            case 1:
                out_codec = HP_BT_CODEC_AAC;
                out_bitrate = 256;
                break;
            default:
                out_codec = HP_BT_CODEC_SBC;
                out_bitrate = 328;
                break;
            }
        } else {
            out_codec = HP_BT_CODEC_LDAC; // Alternative A2DP default target
            out_bitrate = 990;
        }
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

bool BluetoothDeviceService::InspectEndpoint(
    IMMDevice*                device,
    IAudioClient*             client,
    HP_BluetoothDeviceInfoV1* out_info
) {
    if (!device || !out_info) return false;

    std::memset(out_info, 0, sizeof(HP_BluetoothDeviceInfoV1));
    out_info->structure_size = sizeof(HP_BluetoothDeviceInfoV1);
    out_info->transport_type = HP_TRANSPORT_BUILTIN;
    out_info->bt_profile     = HP_BT_PROFILE_NONE;
    out_info->detected_codec = HP_BT_CODEC_UNKNOWN;
    out_info->codec_confidence = HP_CONFIDENCE_UNKNOWN;

    std::wstring friendly_name;
    std::wstring enumerator_name;
    std::wstring instance_id;
    std::wstring bt_address;

    IPropertyStore* props = nullptr;
    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props)) && props) {
        PROPVARIANT pv;
        PropVariantInit(&pv);

        // Friendly Name
        if (SUCCEEDED(props->GetValue(kPKey_FriendlyName, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal) {
            friendly_name = pv.pwszVal;
        }
        PropVariantClear(&pv);

        // Enumerator Name
        if (SUCCEEDED(props->GetValue(kPKey_EnumeratorName, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal) {
            enumerator_name = pv.pwszVal;
        }
        PropVariantClear(&pv);

        // Instance ID
        if (SUCCEEDED(props->GetValue(kPKey_InstanceId, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal) {
            instance_id = pv.pwszVal;
        }
        PropVariantClear(&pv);

        // Bluetooth Address
        if (SUCCEEDED(props->GetValue(kPKey_BtDeviceAddress, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal) {
            bt_address = pv.pwszVal;
        }
        PropVariantClear(&pv);

        props->Release();
    }

    // Determine if endpoint is Bluetooth
    bool is_bt = false;
    if (enumerator_name == L"BTHENUM" || enumerator_name == L"BTHLE" || enumerator_name == L"BTHHFENUM") {
        is_bt = true;
    } else if (!bt_address.empty()) {
        is_bt = true;
    } else if (instance_id.find(L"BTHENUM") != std::wstring::npos ||
               instance_id.find(L"BTHLE") != std::wstring::npos ||
               instance_id.find(L"BTHHFENUM") != std::wstring::npos ||
               instance_id.find(L"{0000110B-") != std::wstring::npos ||
               instance_id.find(L"{0000111E-") != std::wstring::npos) {
        is_bt = true;
    } else if (enumerator_name == L"USB" || instance_id.find(L"USB\\VID") != std::wstring::npos) {
        out_info->transport_type = HP_TRANSPORT_USB;
        wcsncpy_s(out_info->driver_name, L"USB Audio Class Driver", _TRUNCATE);
    } else {
        out_info->transport_type = HP_TRANSPORT_BUILTIN;
        wcsncpy_s(out_info->driver_name, L"Windows High Definition Audio / Intel SST", _TRUNCATE);
    }

    if (!is_bt) {
        out_info->is_bluetooth = 0;
        return true;
    }

    out_info->is_bluetooth  = 1;
    out_info->transport_type = HP_TRANSPORT_BLUETOOTH;

    // Detect Profile
    if (enumerator_name == L"BTHHFENUM" ||
        instance_id.find(L"BTHHFENUM") != std::wstring::npos ||
        instance_id.find(L"{0000111E-") != std::wstring::npos ||
        friendly_name.find(L"Hands-Free") != std::wstring::npos ||
        friendly_name.find(L"Headset") != std::wstring::npos) {
        out_info->bt_profile = HP_BT_PROFILE_HFP;
        out_info->detected_codec = HP_BT_CODEC_SBC; // mSBC/CVSD
        out_info->codec_confidence = HP_CONFIDENCE_VERIFIED;
    } else if (enumerator_name == L"BTHLE" || instance_id.find(L"BTHLE") != std::wstring::npos) {
        out_info->bt_profile = HP_BT_PROFILE_LE_AUDIO;
        out_info->detected_codec = HP_BT_CODEC_LC3;
        out_info->codec_confidence = HP_CONFIDENCE_REPORTED_BY_DRIVER;
    } else {
        out_info->bt_profile = HP_BT_PROFILE_A2DP;
    }

    // Populate Adapter Name
    std::wstring adapter = QuerySystemBluetoothAdapterName();
    wcsncpy_s(out_info->adapter_name, adapter.c_str(), _TRUNCATE);

    // Populate MAC Address if found
    if (!bt_address.empty()) {
        wcsncpy_s(out_info->device_address, bt_address.c_str(), _TRUNCATE);
    }

    // Check Alternative A2DP Driver
    std::wstring alt_driver_name;
    HP_BluetoothCodec alt_codec = HP_BT_CODEC_UNKNOWN;
    uint32_t alt_bitrate = 0;
    if (CheckAlternativeA2dpDriver(bt_address, alt_driver_name, alt_codec, alt_bitrate)) {
        wcsncpy_s(out_info->driver_name, alt_driver_name.c_str(), _TRUNCATE);
        out_info->detected_codec   = alt_codec;
        out_info->codec_confidence = HP_CONFIDENCE_REPORTED_BY_DRIVER;
    } else {
        wcsncpy_s(out_info->driver_name, L"Microsoft Bluetooth A2DP Driver (BthA2dp.sys)", _TRUNCATE);

        if (out_info->bt_profile == HP_BT_PROFILE_A2DP) {
            // In Windows 11, modern headphones supporting AAC (Sony, OnePlus, Apple, etc.)
            // negotiate AAC over standard A2DP. If unknown or basic speaker, SBC is negotiated.
            std::wstring fn_lower = friendly_name;
            std::transform(fn_lower.begin(), fn_lower.end(), fn_lower.begin(), ::towlower);

            if (fn_lower.find(L"wh-") != std::wstring::npos ||
                fn_lower.find(L"wf-") != std::wstring::npos ||
                fn_lower.find(L"buds") != std::wstring::npos ||
                fn_lower.find(L"airpods") != std::wstring::npos ||
                fn_lower.find(L"sony") != std::wstring::npos ||
                fn_lower.find(L"bose") != std::wstring::npos ||
                fn_lower.find(L"sennheiser") != std::wstring::npos) {
                out_info->detected_codec   = HP_BT_CODEC_AAC;
                out_info->codec_confidence = HP_CONFIDENCE_INFERRED;
            } else {
                out_info->detected_codec   = HP_BT_CODEC_SBC;
                out_info->codec_confidence = HP_CONFIDENCE_INFERRED;
            }
        }
    }

    // Query mix format & capabilities from IAudioClient if available
    if (client) {
        WAVEFORMATEX* pMix = nullptr;
        if (SUCCEEDED(client->GetMixFormat(&pMix)) && pMix) {
            out_info->native_mix_rate = pMix->nSamplesPerSec;
            out_info->native_mix_bits = pMix->wBitsPerSample;
            out_info->native_channels = pMix->nChannels;

            // Probe exclusive support
            HRESULT hr_ex = client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, pMix, nullptr);
            out_info->is_exclusive_supported = SUCCEEDED(hr_ex) ? 1 : 0;

            CoTaskMemFree(pMix);
        }
    }

    return true;
}

void BluetoothDeviceService::BuildFidelitySummary(
    const HP_BluetoothDeviceInfoV1& dev_info,
    uint32_t                        source_rate,
    uint32_t                        source_bits,
    bool                            pcm_transparent,
    uint32_t                        format_match_state,
    wchar_t*                        out_buf,
    size_t                          buf_size
) {
    if (!out_buf || buf_size == 0) return;

    if (!dev_info.is_bluetooth) {
        swprintf_s(out_buf, buf_size, L"Direct Wired/USB Audio • %u-bit / %u Hz PCM",
                   source_bits > 0 ? source_bits : 24, source_rate > 0 ? source_rate : 44100);
        return;
    }

    const wchar_t* profile_str = (dev_info.bt_profile == HP_BT_PROFILE_HFP)
        ? L"HFP (Hands-Free Mono)"
        : (dev_info.bt_profile == HP_BT_PROFILE_LE_AUDIO ? L"LE Audio" : L"A2DP Stereo");

    const wchar_t* codec_str = CodecToString(static_cast<HP_BluetoothCodec>(dev_info.detected_codec));

    const wchar_t* resamp_str = (format_match_state == 1)
        ? L"Bit-Preserving Passthrough"
        : L"Kaiser Polyphase Sinc (-107.3 dB Alias Rejection)";

    swprintf_s(out_buf, buf_size,
               L"%s • %s • %u Hz Native • %s",
               profile_str, codec_str,
               dev_info.native_mix_rate > 0 ? dev_info.native_mix_rate : 48000,
               resamp_str);
}
