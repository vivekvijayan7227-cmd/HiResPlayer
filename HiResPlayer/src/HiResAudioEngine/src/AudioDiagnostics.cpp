#include "AudioDiagnostics.h"
#include <cstring>
#include <algorithm>

void AudioDiagnostics::Populate(
    HP_StatusV1& out,
    uint32_t state,
    uint32_t source_rate, uint32_t source_valid_bits, uint32_t source_channels,
    uint64_t source_total_samples,
    uint32_t output_rate, uint32_t output_valid_bits,
    uint32_t output_container_bits, uint32_t output_channels,
    uint32_t actual_mode,
    bool     decoder_no_errors,
    bool     exclusive_active,
    bool     dsp_disabled,
    bool     pcm_passthrough,
    uint64_t rendered_samples,
    uint64_t underruns,
    int32_t  last_hresult,
    const std::wstring& endpoint_id,
    const std::wstring& endpoint_name
) const noexcept {
    out.state                 = state;
    out.source_rate           = source_rate;
    out.source_valid_bits     = source_valid_bits;
    out.source_channels       = source_channels;
    out.source_total_samples  = source_total_samples;
    out.output_rate           = output_rate;
    out.output_valid_bits     = output_valid_bits;
    out.output_container_bits = output_container_bits;
    out.output_channels       = output_channels;
    out.actual_mode           = actual_mode;
    out.rendered_source_samples = rendered_samples;
    out.underrun_count        = underruns;
    out.last_hresult          = last_hresult;

    // Evaluate all 7 bit-perfect conditions dynamically
    bool rate_match    = (source_rate == output_rate) && output_rate != 0;
    bool bits_match    = (source_valid_bits == output_valid_bits) && output_valid_bits != 0;
    bool ch_match      = (source_channels == output_channels) && output_channels != 0;
    bool pcm_pass      = pcm_passthrough;

    out.bp_no_decoder_errors  = decoder_no_errors ? 1 : 0;
    out.bp_rate_match         = rate_match ? 1 : 0;
    out.bp_bits_match         = bits_match ? 1 : 0;
    out.bp_channels_match     = ch_match ? 1 : 0;
    out.bp_exclusive_active   = exclusive_active ? 1 : 0;
    out.bp_dsp_disabled       = dsp_disabled ? 1 : 0;
    out.bp_pcm_passthrough    = pcm_pass ? 1 : 0;

    out.bitperfect_eligible = EvaluateBitPerfect(
        decoder_no_errors, source_rate, output_rate,
        source_valid_bits, output_valid_bits,
        source_channels, output_channels,
        exclusive_active, dsp_disabled, pcm_pass
    );

    // Determine status message id
    if (last_hresult == (int32_t)0x88890004 ||
        last_hresult == (int32_t)0x88890008) {
        // AUDCLNT_E_DEVICE_INVALIDATED or E_DEVICE_REMOVED
        out.status_message_id = 5;
    } else if (actual_mode == HP_EXCLUSIVE) {
        if (out.bitperfect_eligible) {
            out.status_message_id = 1; // Exclusive eligible
        } else if (last_hresult == (int32_t)0x88890022) {
            // AUDCLNT_E_DEVICE_IN_USE
            out.status_message_id = 3;
        } else {
            out.status_message_id = 2; // Exclusive unsupported format
        }
    } else {
        // Shared mode
        if (rate_match && bits_match && ch_match) {
            out.status_message_id = 0; // Shared, no conversion
        } else {
            out.status_message_id = 4; // Shared, format converted
        }
    }

    // Copy endpoint strings
    size_t id_len = std::min(endpoint_id.size(), (size_t)255);
    std::memcpy(out.endpoint_id, endpoint_id.c_str(), id_len * sizeof(wchar_t));
    out.endpoint_id[id_len] = L'\0';

    size_t nm_len = std::min(endpoint_name.size(), (size_t)255);
    std::memcpy(out.endpoint_friendly_name, endpoint_name.c_str(), nm_len * sizeof(wchar_t));
    out.endpoint_friendly_name[nm_len] = L'\0';
}

std::wstring AudioDiagnostics::StatusMessageForId(uint32_t id) noexcept {
    switch (id) {
    case 0: return L"Shared mode \u2014 Windows mixer in use";
    case 1: return L"Exclusive \u2014 source format preserved (bit-perfect eligible)";
    case 2: return L"Exclusive unavailable \u2014 unsupported format";
    case 3: return L"Exclusive unavailable \u2014 device in use / access denied";
    case 4: return L"Playback using converted format \u2014 not bit-perfect";
    case 5: return L"Device disconnected / stream interrupted";
    default: return L"Unknown status";
    }
}

uint32_t AudioDiagnostics::EvaluateBitPerfect(
    bool decoder_no_errors,
    uint32_t source_rate,  uint32_t output_rate,
    uint32_t source_bits,  uint32_t output_bits,
    uint32_t source_ch,    uint32_t output_ch,
    bool exclusive_active,
    bool dsp_disabled,
    bool pcm_passthrough
) noexcept {
    if (!decoder_no_errors)  return 0;
    if (source_rate  != output_rate  || output_rate == 0)  return 0;
    if (source_bits  != output_bits  || output_bits == 0)  return 0;
    if (source_ch    != output_ch    || output_ch == 0)    return 0;
    if (!exclusive_active)   return 0;
    if (!dsp_disabled)       return 0;
    if (!pcm_passthrough)    return 0;
    return 1;
}
