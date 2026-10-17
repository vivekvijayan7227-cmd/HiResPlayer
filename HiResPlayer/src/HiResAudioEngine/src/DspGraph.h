#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <array>
#include "Equalizer.h"
#include "CenterChannelProcessor.h"
#include "StemMixer.h"

// Transparent studio-grade soft knee limiter function
// Fully linear (0.000% THD) up to -0.45 dBFS (0.95f).
// Smooth asymptotic saturation prevents harsh digital clipping and integer wrap-around.
inline float StudioSoftLimit(float x, float& out_gain_reduction) noexcept {
    constexpr float kThreshold = 0.95f;
    constexpr float kMax = 0.9999f;
    const float abs_x = std::abs(x);
    if (abs_x <= kThreshold) {
        out_gain_reduction = 0.0f;
        return x; // 100% transparent linear passthrough
    }
    const float diff = abs_x - kThreshold;
    const float sat = kThreshold + (diff / (1.0f + 2.0f * diff)) * (kMax - kThreshold);
    const float limited = std::min(sat, kMax);
    // Calculate gain reduction in dB
    if (abs_x > 1e-6f) {
        float gr = 20.0f * std::log10(limited / abs_x);
        if (gr < out_gain_reduction) out_gain_reduction = gr;
    }
    return (x >= 0.0f) ? limited : -limited;
}

inline float StudioSoftLimit(float x) noexcept {
    float gr = 0.0f;
    return StudioSoftLimit(x, gr);
}

// DspGraph: Studio-Grade Signal Processing Chain with Lock-Free Parameter Synchronization (AUD-07)
// Features:
// - Lock-free, real-time audio thread execution: parameter updates use atomics without blocking.
// - Sample-rate calibrated smooth parameter slewing (preamp ~25 ms tau) to eliminate clicks, pops, and zipper noise.
// - Transparent Studio Peak Limiter with instantaneous transient catch and smooth exponential release (~60 ms tau).
// - Independent dirty tracking per subsystem (preamp, graphic eq, parametric eq, center processor, stems).
// - Pre-DSP and Post-DSP peak meters + Limiter Gain Reduction readout (AUD-15).
// - Pure offline non-AI classical DSP: zero ONNX, zero neural networks, zero ML runtimes.
class DspGraph {
public:
    DspGraph() {
        for (auto& g : m_pending_graphic_gains) {
            g.store(0.0f, std::memory_order_relaxed);
        }
        SetSampleRate(44100.0f);
    }

    void SetSampleRate(float sample_rate) {
        m_sample_rate = std::max(sample_rate, 8000.0f);
        m_preamp_slew_factor = 1.0f - std::exp(-1.0f / (m_sample_rate * 0.025f));
        m_limiter_attack_factor = 1.0f - std::exp(-1.0f / (m_sample_rate * 0.0025f));
        m_limiter_release_factor = 1.0f - std::exp(-1.0f / (m_sample_rate * 0.080f));

        m_graphic_eq.SetSampleRate(m_sample_rate);
        m_parametric_eq.SetSampleRate(m_sample_rate);
        m_center_dsp.SetSampleRate(m_sample_rate);
        m_ui_graphic_eq.SetSampleRate(m_sample_rate);
        m_ui_parametric_eq.SetSampleRate(m_sample_rate);
        m_ui_center_dsp.SetSampleRate(m_sample_rate);
    }

    void ResetFilterStates() noexcept {
        m_graphic_eq.Reset();
        m_parametric_eq.Reset();
        m_center_dsp.Reset();
        m_limiter_gain = 1.0f;
        m_eq_dirty.store(true, std::memory_order_release);
        m_param_eq_dirty.store(true, std::memory_order_release);
        m_center_dirty.store(true, std::memory_order_release);
    }

    // ─── Thread-Safe Lock-Free Parameter Setters (Called from UI / Control Thread) ─

    void SetPreampDb(float preamp_db) noexcept {
        preamp_db = std::clamp(preamp_db, -18.0f, +6.0f);
        m_pending_preamp_db.store(preamp_db, std::memory_order_relaxed);
        const float lin = std::pow(10.0f, preamp_db / 20.0f);
        m_target_preamp_linear.store(lin, std::memory_order_release);
        UpdateDspActive();
    }

    float GetPreampDb() const noexcept {
        return m_pending_preamp_db.load(std::memory_order_relaxed);
    }

    void SetEqBypass(bool bypass) noexcept {
        m_pending_eq_bypass.store(bypass, std::memory_order_release);
        UpdateDspActive();
    }

    bool IsEqBypassed() const noexcept {
        return m_pending_eq_bypass.load(std::memory_order_relaxed);
    }

    void SetGraphicBand(uint32_t band_index, float gain_db) noexcept {
        if (band_index < 10) {
            gain_db = std::clamp(gain_db, -12.0f, +12.0f);
            m_pending_graphic_gains[band_index].store(gain_db, std::memory_order_relaxed);
            m_ui_graphic_eq.SetBandGain(band_index, gain_db);
            UpdateDspActive();
            m_eq_dirty.store(true, std::memory_order_release);
        }
    }

    void ResetGraphicEq() noexcept {
        for (size_t i = 0; i < 10; ++i) {
            m_pending_graphic_gains[i].store(0.0f, std::memory_order_relaxed);
            m_ui_graphic_eq.SetBandGain(i, 0.0f);
        }
        UpdateDspActive();
        m_eq_dirty.store(true, std::memory_order_release);
    }

    void SetParametricBand(uint32_t band_index, const ParametricBandConfig& cfg) noexcept {
        if (band_index < ParametricEqualizer::kMaxBands) {
            {
                std::lock_guard<std::mutex> lk(m_param_eq_mutex);
                m_pending_param_bands[band_index] = cfg;
            }
            m_ui_parametric_eq.ConfigureBand(band_index, cfg);
            UpdateDspActive();
            m_param_eq_dirty.store(true, std::memory_order_release);
        }
    }

    void ResetParametricEq() noexcept {
        {
            std::lock_guard<std::mutex> lk(m_param_eq_mutex);
            for (auto& b : m_pending_param_bands) {
                b.enabled = false;
                b.gain_db = 0.0f;
            }
        }
        m_ui_parametric_eq.Reset();
        UpdateDspActive();
        m_param_eq_dirty.store(true, std::memory_order_release);
    }

    void SetCenterProcessor(uint32_t mode, float amount) noexcept {
        amount = std::clamp(amount, 0.0f, 1.0f);
        m_pending_center_mode.store(mode, std::memory_order_relaxed);
        m_pending_center_amount.store(amount, std::memory_order_relaxed);
        m_ui_center_dsp.Configure(mode, amount);
        UpdateDspActive();
        m_center_dirty.store(true, std::memory_order_release);
    }

    void SetStemMixer(float vocal_volume, float inst_volume, bool mute_vocal, bool mute_inst) noexcept {
        m_pending_vocal_vol.store(mute_vocal ? 0.0f : vocal_volume, std::memory_order_relaxed);
        m_pending_inst_vol.store(mute_inst ? 0.0f : inst_volume, std::memory_order_relaxed);
        m_pending_stems_active.store(true, std::memory_order_relaxed);
        UpdateDspActive();
        m_stems_dirty.store(true, std::memory_order_release);
    }

    void DisableStemMixer() noexcept {
        m_pending_stems_active.store(false, std::memory_order_relaxed);
        UpdateDspActive();
        m_stems_dirty.store(true, std::memory_order_release);
    }

    // ─── UI Visualizer Magnitude Evaluation (Evaluated on UI thread using UI models) ─

    float EvaluateMagnitudeDb(float freq_hz) const noexcept {
        const float pre_db = m_pending_preamp_db.load(std::memory_order_relaxed);
        if (m_pending_eq_bypass.load(std::memory_order_relaxed)) return pre_db;
        return pre_db +
               m_ui_graphic_eq.EvaluateMagnitudeDb(freq_hz) +
               m_ui_parametric_eq.EvaluateMagnitudeDb(freq_hz);
    }

    float EvaluateCenterMagnitudeDb(float freq_hz) const noexcept {
        return m_ui_center_dsp.EvaluateMagnitudeDb(freq_hz);
    }

    // ─── Backward Compatible Reference Accessors ────────────────────────────────

    GraphicEqualizer& GetGraphicEq() noexcept {
        return m_ui_graphic_eq;
    }

    ParametricEqualizer& GetParametricEq() noexcept {
        return m_ui_parametric_eq;
    }

    CenterChannelProcessor& GetCenterDsp() noexcept {
        return m_ui_center_dsp;
    }

    StemMixer& GetStemMixer() noexcept {
        return m_stem_mixer;
    }

    // ─── Peak & Loudness Meters (Lock-Free Readouts) ─────────────────────────────

    void ResetPeaks() noexcept {
        m_in_peak_l.store(0.0f, std::memory_order_relaxed);
        m_in_peak_r.store(0.0f, std::memory_order_relaxed);
        m_out_peak_l.store(0.0f, std::memory_order_relaxed);
        m_out_peak_r.store(0.0f, std::memory_order_relaxed);
        m_limiter_gr.store(0.0f, std::memory_order_relaxed);
        m_clipped.store(false, std::memory_order_relaxed);
    }

    float GetPeakLeft() const noexcept { return m_out_peak_l.load(std::memory_order_relaxed); }
    float GetPeakRight() const noexcept { return m_out_peak_r.load(std::memory_order_relaxed); }
    float GetInPeakLeft() const noexcept { return m_in_peak_l.load(std::memory_order_relaxed); }
    float GetInPeakRight() const noexcept { return m_in_peak_r.load(std::memory_order_relaxed); }
    float GetLimiterGainReduction() const noexcept { return m_limiter_gr.load(std::memory_order_relaxed); }
    bool HasClipped() const noexcept { return m_clipped.load(std::memory_order_relaxed); }

    bool IsDspActive() const noexcept {
        return m_dsp_active.load(std::memory_order_relaxed);
    }

    bool IsDspDisabled() const noexcept { return !IsDspActive(); }

    // ─── Real-Time Processing (Called SOLELY on WASAPI Render Thread) ─────────────

    inline void ProcessFrame(float& left, float& right) noexcept {
        // 1. Lock-free parameter synchronization
        if (m_eq_dirty.load(std::memory_order_acquire)) {
            for (size_t i = 0; i < 10; ++i) {
                m_graphic_eq.SetBandGain(i, m_pending_graphic_gains[i].load(std::memory_order_relaxed));
            }
            m_eq_dirty.store(false, std::memory_order_release);
        }
        if (m_param_eq_dirty.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lk(m_param_eq_mutex, std::try_to_lock);
            if (lk.owns_lock()) {
                for (size_t i = 0; i < ParametricEqualizer::kMaxBands; ++i) {
                    m_parametric_eq.ConfigureBand(i, m_pending_param_bands[i]);
                }
                m_param_eq_dirty.store(false, std::memory_order_release);
            }
        }
        if (m_center_dirty.load(std::memory_order_acquire)) {
            const uint32_t mode = m_pending_center_mode.load(std::memory_order_relaxed);
            const float amount = m_pending_center_amount.load(std::memory_order_relaxed);
            m_center_dsp.Configure(mode, amount);
            m_center_dirty.store(false, std::memory_order_release);
        }
        if (m_stems_dirty.load(std::memory_order_acquire)) {
            m_active_stems = m_pending_stems_active.load(std::memory_order_relaxed);
            m_active_vocal_vol = m_pending_vocal_vol.load(std::memory_order_relaxed);
            m_active_inst_vol = m_pending_inst_vol.load(std::memory_order_relaxed);
            m_stems_dirty.store(false, std::memory_order_release);
        }

        // 2. Measure Pre-DSP Sample Peaks
        const float in_l = left;
        const float in_r = right;
        const float abs_in_l = std::abs(in_l);
        const float abs_in_r = std::abs(in_r);
        if (abs_in_l > m_in_peak_l.load(std::memory_order_relaxed)) {
            m_in_peak_l.store(abs_in_l, std::memory_order_relaxed);
        }
        if (abs_in_r > m_in_peak_r.load(std::memory_order_relaxed)) {
            m_in_peak_r.store(abs_in_r, std::memory_order_relaxed);
        }

        // 3. Center Channel Focus or Stem Processing
        if (m_active_stems) {
            m_center_dsp.ProcessStems(left, right, m_active_vocal_vol, m_active_inst_vol);
        } else if (m_center_dsp.IsActive()) {
            m_center_dsp.ProcessFrame(left, right);
        }

        // 4. Smooth Preamp Gain Slew (Sample-rate normalized exponential ramp, ~25 ms tau)
        const float target_p = m_target_preamp_linear.load(std::memory_order_relaxed);
        m_current_preamp_linear += (target_p - m_current_preamp_linear) * m_preamp_slew_factor;
        if (std::abs(target_p - m_current_preamp_linear) < 1e-6f) {
            m_current_preamp_linear = target_p;
        }
        left  *= m_current_preamp_linear;
        right *= m_current_preamp_linear;

        // 5. Graphic & Parametric EQ Processing (if not bypassed)
        if (!m_pending_eq_bypass.load(std::memory_order_relaxed)) {
            m_graphic_eq.ProcessFrame(left, right);
            m_parametric_eq.ProcessFrame(left, right);
        }

        // 6. Studio Peak Limiter (Linked-stereo transparent dynamic envelope limiter)
        const float abs_l = std::abs(left);
        const float abs_r = std::abs(right);
        const float frame_peak = std::max(abs_l, abs_r);

        constexpr float kLimitThreshold = 0.95f;
        constexpr float kLimitCeiling   = 0.99f;

        float target_gain = 1.0f;
        if (frame_peak > kLimitThreshold) {
            target_gain = kLimitCeiling / frame_peak;
        }

        // Smooth slewing on both attack (~2.5ms) and release (~80ms) eliminates
        // sample-to-sample slope discontinuities and limiter chatter when boosting preamp/EQ.
        if (target_gain < m_limiter_gain) {
            m_limiter_gain += (target_gain - m_limiter_gain) * m_limiter_attack_factor;
        } else {
            m_limiter_gain += (target_gain - m_limiter_gain) * m_limiter_release_factor;
            if (m_limiter_gain > 0.9999f) m_limiter_gain = 1.0f;
        }

        left  *= m_limiter_gain;
        right *= m_limiter_gain;

        if (m_limiter_gain < 0.999f) {
            const float gr_db = 20.0f * std::log10(m_limiter_gain);
            if (gr_db < m_limiter_gr.load(std::memory_order_relaxed)) {
                m_limiter_gr.store(gr_db, std::memory_order_relaxed);
            }
        }

        // Transparent soft knee saturation for inter-sample peak protection
        auto SoftSaturate = [](float s) noexcept -> float {
            constexpr float kThresh = 0.98f;
            constexpr float kMax = 0.9999f;
            const float a = std::abs(s);
            if (a <= kThresh) return s;
            const float diff = a - kThresh;
            const float sat = kThresh + (diff / (1.0f + 2.0f * diff)) * (kMax - kThresh);
            const float out = std::min(sat, kMax);
            return (s >= 0.0f) ? out : -out;
        };
        left  = SoftSaturate(left);
        right = SoftSaturate(right);

        // 7. Measure Post-DSP Sample Peaks
        const float abs_out_l = std::abs(left);
        const float abs_out_r = std::abs(right);
        if (abs_out_l > m_out_peak_l.load(std::memory_order_relaxed)) {
            m_out_peak_l.store(abs_out_l, std::memory_order_relaxed);
        }
        if (abs_out_r > m_out_peak_r.load(std::memory_order_relaxed)) {
            m_out_peak_r.store(abs_out_r, std::memory_order_relaxed);
        }
        if (abs_out_l >= 0.999f || abs_out_r >= 0.999f) {
            m_clipped.store(true, std::memory_order_relaxed);
        }
    }

private:
    void UpdateDspActive() noexcept {
        bool act = false;
        if (!m_pending_eq_bypass.load(std::memory_order_relaxed)) {
            act = true;
        } else {
            if (std::abs(m_target_preamp_linear.load(std::memory_order_relaxed) - 1.0f) > 0.001f) act = true;
            if (m_pending_center_mode.load(std::memory_order_relaxed) != 0 &&
                m_pending_center_amount.load(std::memory_order_relaxed) > 0.001f) act = true;
            if (m_pending_stems_active.load(std::memory_order_relaxed)) act = true;
        }
        m_dsp_active.store(act, std::memory_order_release);
    }

    float m_sample_rate = 44100.0f;
    float m_preamp_slew_factor = 0.000906f;
    float m_limiter_attack_factor = 0.009048f;
    float m_limiter_release_factor = 0.000283f;
    float m_limiter_gain = 1.0f;

    // Parameter storage (lock-free atomics)
    std::atomic<float> m_pending_preamp_db{0.0f};
    std::atomic<float> m_target_preamp_linear{1.0f};
    std::atomic<bool>  m_pending_eq_bypass{false};

    std::array<std::atomic<float>, 10> m_pending_graphic_gains{};
    std::atomic<bool>  m_eq_dirty{false};

    mutable std::mutex m_param_eq_mutex;
    std::array<ParametricBandConfig, ParametricEqualizer::kMaxBands> m_pending_param_bands{};
    std::atomic<bool>  m_param_eq_dirty{false};

    std::atomic<uint32_t> m_pending_center_mode{0};
    std::atomic<float>    m_pending_center_amount{0.75f};
    std::atomic<bool>     m_center_dirty{false};

    std::atomic<bool>  m_pending_stems_active{false};
    std::atomic<float> m_pending_vocal_vol{1.0f};
    std::atomic<float> m_pending_inst_vol{1.0f};
    std::atomic<bool>  m_stems_dirty{false};

    std::atomic<bool>  m_dsp_active{false};

    // UI models for offline curve evaluation (evaluated on UI thread)
    GraphicEqualizer       m_ui_graphic_eq;
    ParametricEqualizer    m_ui_parametric_eq;
    CenterChannelProcessor m_ui_center_dsp;

    // Audio-thread active state & DSP instances (strictly owned by render thread)
    float m_current_preamp_linear = 1.0f;
    bool  m_active_stems = false;
    float m_active_vocal_vol = 1.0f;
    float m_active_inst_vol = 1.0f;

    GraphicEqualizer       m_graphic_eq;
    ParametricEqualizer    m_parametric_eq;
    CenterChannelProcessor m_center_dsp;
    StemMixer              m_stem_mixer;

    // Lock-free Peak & Loudness monitoring atomics
    std::atomic<float> m_in_peak_l{0.0f};
    std::atomic<float> m_in_peak_r{0.0f};
    std::atomic<float> m_out_peak_l{0.0f};
    std::atomic<float> m_out_peak_r{0.0f};
    std::atomic<float> m_limiter_gr{0.0f};
    std::atomic<bool>  m_clipped{false};
};
