// HiResPlayer Audio Engine — Public C ABI
// Version: 1
// This header is the contract of record. Managed P/Invoke structs must match exactly.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define HP_API __declspec(dllexport)
#define HP_CALL __cdecl
#else
#error "HiResAudioEngine is Windows-only"
#endif

#include <stdint.h>

typedef void* HP_Handle;

// ─── Result codes ────────────────────────────────────────────────────────────
typedef enum HP_Result {
    HP_OK                       = 0,
    HP_INVALID_ARGUMENT         = 1,
    HP_NOT_READY                = 2,
    HP_FILE_ERROR               = 3,
    HP_UNSUPPORTED_SOURCE       = 4,
    HP_UNSUPPORTED_OUTPUT_FORMAT= 5,
    HP_DEVICE_UNAVAILABLE       = 6,
    HP_DEVICE_BUSY              = 7,
    HP_DECODER_ERROR            = 8,
    HP_INTERNAL_ERROR           = 9,
    HP_ILLEGAL_STATE            = 10,
    HP_BUFFER_TOO_SMALL         = 11,
} HP_Result;

// ─── Playback modes ──────────────────────────────────────────────────────────
typedef enum HP_PlaybackMode {
    HP_SHARED    = 0,
    HP_EXCLUSIVE = 1,
} HP_PlaybackMode;

// ─── Playback states ─────────────────────────────────────────────────────────
typedef enum HP_State {
    HP_STATE_UNINITIALIZED   = 0,
    HP_STATE_READY           = 1,
    HP_STATE_LOADING         = 2,
    HP_STATE_PRIMED          = 3,
    HP_STATE_PLAYING         = 4,
    HP_STATE_PAUSED          = 5,
    HP_STATE_SEEKING         = 6,
    HP_STATE_SWITCHING_DEVICE= 7,
    HP_STATE_RECOVERABLE_ERR = 8,
    HP_STATE_STOPPED         = 9,
    HP_STATE_DISPOSED        = 10,
} HP_State;

// ─── Device states ───────────────────────────────────────────────────────────
typedef enum HP_DeviceState {
    HP_DEVICE_ACTIVE    = 1,
    HP_DEVICE_DISABLED  = 2,
    HP_DEVICE_NOTPRESENT= 4,
    HP_DEVICE_UNPLUGGED = 8,
} HP_DeviceState;

// ─── Event types ─────────────────────────────────────────────────────────────
typedef enum HP_EventType {
    // Device events (1–9)
    HP_EVT_DEVICE_ADDED          = 1,
    HP_EVT_DEVICE_REMOVED        = 2,
    HP_EVT_DEVICE_STATE_CHANGED  = 3,
    HP_EVT_DEFAULT_DEVICE_CHANGED= 4,
    // Playback events (10–19)
    HP_EVT_STATE_CHANGED         = 10,
    HP_EVT_TRACK_END             = 11,
    HP_EVT_UNDERRUN              = 12,
    HP_EVT_ERROR                 = 13,
} HP_EventType;

// ─── Status struct (versioned) ───────────────────────────────────────────────
// structure_size must be set by caller before calling hp_get_status.
typedef struct HP_StatusV1 {
    uint32_t structure_size;          // = sizeof(HP_StatusV1)
    uint32_t state;                   // HP_State
    // Source properties (from FLAC STREAMINFO)
    uint32_t source_rate;             // e.g. 96000
    uint32_t source_valid_bits;       // e.g. 24
    uint32_t source_channels;         // e.g. 2
    uint64_t source_total_samples;    // total PCM samples per channel
    // Output stream properties (after WASAPI initialization)
    uint32_t output_rate;
    uint32_t output_valid_bits;
    uint32_t output_container_bits;   // 16, 24, or 32
    uint32_t output_channels;
    uint32_t actual_mode;             // HP_PlaybackMode actually in use
    // Bit-perfect evaluation (all 7 conditions)
    uint32_t bitperfect_eligible;     // 1 = all conditions met
    uint32_t bp_no_decoder_errors;    // condition 1
    uint32_t bp_rate_match;           // condition 2
    uint32_t bp_bits_match;           // condition 3
    uint32_t bp_channels_match;       // condition 4
    uint32_t bp_exclusive_active;     // condition 5
    uint32_t bp_dsp_disabled;         // condition 6
    uint32_t bp_pcm_passthrough;      // condition 7
    // Progress and diagnostics
    uint64_t rendered_source_samples; // samples rendered so far (per channel)
    uint64_t underrun_count;
    int32_t  last_hresult;
    // Status message index (maps to exact PRD §8 vocabulary string)
    // 0=Shared; 1=Exclusive eligible; 2=Exclusive unsupported format;
    // 3=Exclusive device busy; 4=Converted not bitperfect; 5=Disconnected
    uint32_t status_message_id;
    // Endpoint info
    wchar_t  endpoint_id[256];
    wchar_t  endpoint_friendly_name[256];
} HP_StatusV1;

// ─── Device info struct (versioned) ─────────────────────────────────────────
typedef struct HP_DeviceInfoV1 {
    uint32_t structure_size;          // = sizeof(HP_DeviceInfoV1)
    wchar_t  id[256];
    wchar_t  friendly_name[256];
    uint32_t state;                   // HP_DeviceState
    uint32_t is_default_render;       // 1 if this is the current default
} HP_DeviceInfoV1;

// ─── Format capability struct ────────────────────────────────────────────────
typedef struct HP_FormatCapV1 {
    uint32_t structure_size;          // = sizeof(HP_FormatCapV1)
    uint32_t sample_rate;             // rate to probe
    uint32_t valid_bits;              // valid bits to probe (16 or 24)
    uint32_t container_bits;          // 16 or 32 (24-in-32 for 24-bit)
    uint32_t channels;                // 1 or 2
    uint32_t exclusive_supported;     // out: 1 = IsFormatSupported confirmed
    uint32_t shared_mix_rate;         // out: mix format rate (shared mode)
    uint32_t shared_mix_bits;         // out: mix format bit depth
    int32_t  probe_hresult;           // out: raw HRESULT from probe
} HP_FormatCapV1;

// ─── Callback signatures ─────────────────────────────────────────────────────
// Device event callback — marshaled to control thread, NOT called from audio thread
typedef void (HP_CALL *HP_DeviceEventCallback)(
    void*         user_data,
    uint32_t      event_type,    // HP_EventType
    const wchar_t* endpoint_id  // may be NULL for global events
);

// Playback event callback — marshaled to control thread
typedef void (HP_CALL *HP_PlaybackEventCallback)(
    void*    user_data,
    uint32_t event_type,          // HP_EventType
    uint32_t new_state,           // HP_State (for STATE_CHANGED events)
    int32_t  hresult,             // HRESULT (for ERROR events), else 0
    uint64_t position_samples     // rendered position at event time
);

// ─── Core API ────────────────────────────────────────────────────────────────
HP_API uint32_t  HP_CALL hp_api_version(void);       // returns 1
HP_API HP_Handle HP_CALL hp_create(void);
HP_API void      HP_CALL hp_destroy(HP_Handle h);

// File and device selection
HP_API HP_Result HP_CALL hp_open_flac(HP_Handle h, const wchar_t* path);
HP_API HP_Result HP_CALL hp_select_device(HP_Handle h, const wchar_t* endpoint_id);
HP_API HP_Result HP_CALL hp_set_mode(HP_Handle h, HP_PlaybackMode mode);

// Playback control
HP_API HP_Result HP_CALL hp_play(HP_Handle h);
HP_API HP_Result HP_CALL hp_pause(HP_Handle h);
HP_API HP_Result HP_CALL hp_stop(HP_Handle h);
HP_API HP_Result HP_CALL hp_seek_sample(HP_Handle h, uint64_t sample_position);
HP_API HP_Result HP_CALL hp_set_volume(HP_Handle h, float volume);
HP_API HP_Result HP_CALL hp_get_volume(HP_Handle h, float* out_volume);

// Status
HP_API HP_Result HP_CALL hp_get_status(HP_Handle h, HP_StatusV1* out_status);

// Device enumeration (two-pass: pass buf=NULL/count=0 to get required count)
HP_API HP_Result HP_CALL hp_enumerate_devices(
    HP_Handle       h,
    HP_DeviceInfoV1* buf,
    uint32_t        buf_count,
    uint32_t*       out_count
);

// Format capability probe for a specific endpoint + mode
HP_API HP_Result HP_CALL hp_query_format(
    HP_Handle      h,
    const wchar_t* endpoint_id,
    HP_PlaybackMode mode,
    HP_FormatCapV1* fmt  // in: rate/bits/channels to probe; out: results filled
);

// Event callbacks (safe to register from any thread; callbacks on control thread)
HP_API HP_Result HP_CALL hp_set_device_event_callback(
    HP_Handle h,
    HP_DeviceEventCallback cb,
    void* user_data
);

HP_API HP_Result HP_CALL hp_set_playback_event_callback(
    HP_Handle h,
    HP_PlaybackEventCallback cb,
    void* user_data
);

// Returns null-terminated status message string matching PRD §8 vocabulary.
// Buffer must be at least 256 wchar_t. Returns HP_BUFFER_TOO_SMALL if not.
HP_API HP_Result HP_CALL hp_get_status_message(
    HP_Handle h,
    wchar_t*  buf,
    uint32_t  buf_wchar_count
);

// ─── Bluetooth Transport & Profile Types ─────────────────────────────────────
typedef enum HP_TransportType {
    HP_TRANSPORT_UNKNOWN    = 0,
    HP_TRANSPORT_BUILTIN    = 1,
    HP_TRANSPORT_USB        = 2,
    HP_TRANSPORT_BLUETOOTH  = 3,
} HP_TransportType;

typedef enum HP_BluetoothProfile {
    HP_BT_PROFILE_NONE      = 0,
    HP_BT_PROFILE_UNKNOWN   = 1,
    HP_BT_PROFILE_A2DP      = 2,    // Bluetooth Classic Stereo (A2DP Audio Sink)
    HP_BT_PROFILE_HFP       = 3,    // Hands-Free Profile (Telephony / Mono Communications)
    HP_BT_PROFILE_LE_AUDIO  = 4,    // Bluetooth LE Audio (LC3)
} HP_BluetoothProfile;

typedef enum HP_BluetoothCodec {
    HP_BT_CODEC_UNKNOWN       = 0,
    HP_BT_CODEC_SBC           = 1,
    HP_BT_CODEC_AAC           = 2,
    HP_BT_CODEC_APTX          = 3,
    HP_BT_CODEC_APTX_HD       = 4,
    HP_BT_CODEC_APTX_ADAPTIVE = 5,
    HP_BT_CODEC_APTX_LOSSLESS = 6,
    HP_BT_CODEC_LDAC          = 7,
    HP_BT_CODEC_LC3           = 8,
} HP_BluetoothCodec;

typedef enum HP_TelemetryConfidence {
    HP_CONFIDENCE_UNKNOWN             = 0,
    HP_CONFIDENCE_INFERRED            = 1,
    HP_CONFIDENCE_REPORTED_BY_DRIVER  = 2,
    HP_CONFIDENCE_VERIFIED            = 3,
    HP_CONFIDENCE_UNSUPPORTED         = 4,
} HP_TelemetryConfidence;

typedef struct HP_BluetoothDeviceInfoV1 {
    uint32_t structure_size;           // = sizeof(HP_BluetoothDeviceInfoV1)
    uint32_t is_bluetooth;             // 1 if Bluetooth endpoint
    uint32_t transport_type;           // HP_TransportType
    uint32_t bt_profile;               // HP_BluetoothProfile
    uint32_t detected_codec;           // HP_BluetoothCodec
    uint32_t codec_confidence;         // HP_TelemetryConfidence
    uint32_t native_mix_rate;          // Endpoint native mix rate (e.g. 48000, 44100, 96000)
    uint32_t native_mix_bits;          // Endpoint native mix bits (e.g. 16, 24, 32)
    uint32_t native_channels;          // e.g. 2 for stereo, 1 for mono
    uint32_t is_exclusive_supported;   // 0 for almost all BT endpoints, 1 if driver supports it
    wchar_t  adapter_name[128];        // e.g. "Intel(R) Wireless Bluetooth(R)"
    wchar_t  device_address[32];       // MAC address e.g. "40:72:18:EE:E8:57"
    wchar_t  driver_name[128];         // e.g. "Microsoft Bluetooth A2DP Driver" or "Alternative A2DP Driver"
} HP_BluetoothDeviceInfoV1;

typedef struct HP_BluetoothFidelityStatusV1 {
    uint32_t structure_size;           // = sizeof(HP_BluetoothFidelityStatusV1)
    uint32_t fidelity_mode_enabled;    // 1 if Bluetooth Fidelity Mode is enabled
    uint32_t pcm_transparent;          // 1 if source PCM is unmodified before output format conversion
    uint32_t output_format_matched;    // 1 if exact match, 2 if converted via Kaiser sinc resampler
    uint32_t wireless_lossless_state;  // 0 = Not Verified (Lossy Bluetooth), 1 = Verified Lossless
    uint32_t is_hfp_warning;           // 1 if device switched to low-bandwidth HFP headset mode
    uint32_t active_codec;             // HP_BluetoothCodec
    uint32_t codec_confidence;         // HP_TelemetryConfidence
    uint32_t estimated_bitrate_kbps;   // 0 if unknown, or reported bitrate (e.g. 328, 990, 256)
    uint64_t underrun_count;           // Output buffer underruns
    wchar_t  status_summary[256];      // Human-readable summary
} HP_BluetoothFidelityStatusV1;

// ─── Bluetooth API ───────────────────────────────────────────────────────────
HP_API HP_Result HP_CALL hp_get_bluetooth_device_info(
    HP_Handle                 h,
    const wchar_t*            endpoint_id,
    HP_BluetoothDeviceInfoV1* out_info
);

HP_API HP_Result HP_CALL hp_set_bluetooth_fidelity_mode(
    HP_Handle h,
    uint32_t  enabled
);

HP_API HP_Result HP_CALL hp_get_bluetooth_fidelity_status(
    HP_Handle                     h,
    HP_BluetoothFidelityStatusV1* out_status
);

#ifdef __cplusplus
}
#endif
