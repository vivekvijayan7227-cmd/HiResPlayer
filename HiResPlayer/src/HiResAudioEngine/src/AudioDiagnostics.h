#pragma once
#include "../include/hires_audio_api.h"
#include <string>

// Evaluates and stores bit-perfect eligibility and status messages.
class AudioDiagnostics {
public:
    // Fill HP_StatusV1 from current state. Also populates status_message_id.
    void Populate(
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
        uint64_t rendered_samples,
        uint64_t underruns,
        int32_t  last_hresult,
        const std::wstring& endpoint_id,
        const std::wstring& endpoint_name
    ) const noexcept;

    // Returns exact PRD §8 status string for message id
    static std::wstring StatusMessageForId(uint32_t id) noexcept;

    // Returns 0=not eligible, 1=eligible
    static uint32_t EvaluateBitPerfect(
        bool decoder_no_errors,
        uint32_t source_rate,  uint32_t output_rate,
        uint32_t source_bits,  uint32_t output_bits,
        uint32_t source_ch,    uint32_t output_ch,
        bool exclusive_active,
        bool dsp_disabled,
        bool pcm_passthrough
    ) noexcept;
};
