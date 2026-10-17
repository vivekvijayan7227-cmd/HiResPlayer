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
#include <ksmedia.h>
#include <cstdint>
#include <string>
#include "../include/hires_audio_api.h"

struct FormatResult {
    bool     success             = false;
    bool     exclusive_supported = false;
    bool     needs_conversion    = false; // true if source rate != mix rate
    WAVEFORMATEXTENSIBLE wfex   = {};
    WAVEFORMATEX         mix_wfx= {}; // shared mode mix format
    int32_t  hresult            = 0;
    std::wstring reason;   // human-readable
};

// Generates and probes WAVEFORMATEXTENSIBLE configurations.
// Call from control thread; never from audio render thread.
class AudioFormatNegotiator {
public:
    // Build WAVEFORMATEXTENSIBLE for stereo integer PCM at given rate/bits.
    // Supports: 16-bit/16-container, 24-bit/32-container, 24-bit/24-container
    static WAVEFORMATEXTENSIBLE BuildExclusiveFormat(
        uint32_t sample_rate,
        uint32_t valid_bits,   // 16 or 24
        uint32_t channels      // 1 or 2
    );

    // Build float32 format at a given rate (for shared-mode rendering)
    static WAVEFORMATEXTENSIBLE BuildSharedFloat32Format(
        uint32_t sample_rate,
        uint32_t channels
    );

    // Probe exclusive support. Returns FormatResult.
    // Tries: (valid_bits in 32-bit container), then (valid_bits packed if 24-bit)
    static FormatResult ProbeExclusive(
        IAudioClient* client,
        uint32_t sample_rate,
        uint32_t valid_bits,
        uint32_t channels
    );

    // Get shared-mode mix format from client.
    static FormatResult GetSharedMixFormat(IAudioClient* client);

    // Set WAVEFORMATEXTENSIBLE channel mask for channel count
    static DWORD ChannelMask(uint32_t channels);
};
