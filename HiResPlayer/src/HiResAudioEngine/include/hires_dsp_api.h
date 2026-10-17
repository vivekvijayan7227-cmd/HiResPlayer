#pragma once
#include "hires_audio_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─── Equalizer & DSP C API Extensions (M7 & M8) ──────────────────────────────

// Set preamp gain in dB (-18.0 dB to +6.0 dB)
HP_API HP_Result HP_CALL hp_dsp_set_preamp(HP_Handle h, float preamp_db);

// Bypass or enable the entire EQ section
HP_API HP_Result HP_CALL hp_dsp_set_eq_bypass(HP_Handle h, uint32_t bypass);

// Set gain for a specific 10-band Graphic EQ band (band 0..9, gain -12.0 to +12.0 dB)
HP_API HP_Result HP_CALL hp_dsp_set_graphic_eq_band(HP_Handle h, uint32_t band_index, float gain_db);

// Reset Graphic EQ to all 0 dB
HP_API HP_Result HP_CALL hp_dsp_reset_graphic_eq(HP_Handle h);

// Configure a 12-band Parametric EQ band
// filter_type: 0=Flat, 1=LowShelf, 2=HighShelf, 3=Peaking, 4=LowPass, 5=HighPass, 6=Notch
HP_API HP_Result HP_CALL hp_dsp_set_parametric_eq_band(
    HP_Handle h,
    uint32_t band_index,
    uint32_t enabled,
    uint32_t filter_type,
    float frequency,
    float gain_db,
    float q
);

// Reset Parametric EQ (disable all bands)
HP_API HP_Result HP_CALL hp_dsp_reset_parametric_eq(HP_Handle h);

// Query calculated frequency response in dB at frequency f (Hz) for combined active EQ
HP_API HP_Result HP_CALL hp_dsp_evaluate_curve(HP_Handle h, float freq_hz, float* out_db);

// Query calculated frequency response in dB at frequency f (Hz) for Center Channel Processor (Vocal Studio)
HP_API HP_Result HP_CALL hp_dsp_evaluate_center_curve(HP_Handle h, float freq_hz, float* out_db);

// Peak detection metrics
HP_API HP_Result HP_CALL hp_dsp_get_peaks(HP_Handle h, float* out_peak_l, float* out_peak_r, uint32_t* out_clipped);
HP_API HP_Result HP_CALL hp_dsp_reset_peaks(HP_Handle h);

// Traditional DSP Vocal / Instrumental Processing (M8)
// mode: 0=Off/Bypass, 1=Vocal Focus, 2=Instrumental Focus (Center Cancellation)
// amount: 0.0f to 1.0f
HP_API HP_Result HP_CALL hp_dsp_set_center_processor(HP_Handle h, uint32_t mode, float amount);

// AI Stem Mixer (M8)
// Configure volume levels (0.0 to 1.0) and mute states for cached stems
HP_API HP_Result HP_CALL hp_dsp_set_stem_mixer(
    HP_Handle h,
    float vocal_volume,
    float inst_volume,
    uint32_t mute_vocal,
    uint32_t mute_inst
);

HP_API HP_Result HP_CALL hp_dsp_disable_stem_mixer(HP_Handle h);

// Live Spectrum Analysis (AUD-11)
// Retrieves actual windowed FFT frequency bands computed from live PCM frames
HP_API HP_Result HP_CALL hp_dsp_get_spectrum(HP_Handle h, float* out_bands, uint32_t band_count);

// Stereo Phase Correlation Meter (QA-11)
// Retrieves actual normalized Pearson cross-correlation r in [-1.0, +1.0]
HP_API HP_Result HP_CALL hp_dsp_get_stereo_correlation(HP_Handle h, float* out_correlation);

// Comprehensive Telemetry: Pre-DSP & Post-DSP Sample Peaks, Limiter GR (AUD-15)
HP_API HP_Result HP_CALL hp_dsp_get_detailed_meters(
    HP_Handle h,
    float* out_in_peak_l,
    float* out_in_peak_r,
    float* out_out_peak_l,
    float* out_out_peak_r,
    float* out_limiter_gr,
    uint32_t* out_clipped
);

#ifdef __cplusplus
}
#endif
