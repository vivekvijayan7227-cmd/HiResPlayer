#pragma once
#include "BiquadFilter.h"
#include <vector>
#include <array>
#include <cmath>

// 10-band ISO standard audiophile graphic equalizer
// Features Low-Shelf sub-bass (31 Hz) and High-Shelf silky air (16 kHz)
class GraphicEqualizer {
public:
    static constexpr size_t kBandCount = 10;
    static constexpr std::array<float, kBandCount> kFrequencies = {
        31.25f, 62.5f, 125.0f, 250.0f, 500.0f,
        1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
    };

    GraphicEqualizer() {
        for (size_t i = 0; i < kBandCount; ++i) {
            m_gains_db[i] = 0.0f;
        }
    }

    void SetSampleRate(float sample_rate) {
        m_sample_rate = sample_rate;
        for (size_t i = 0; i < kBandCount; ++i) {
            m_filters[i].Reset();
        }
        Recompute();
    }

    void SetBandGain(size_t band, float gain_db) {
        if (band >= kBandCount) return;
        gain_db = std::clamp(gain_db, -12.0f, +12.0f);
        if (std::abs(m_gains_db[band] - gain_db) < 0.0001f) return;
        m_gains_db[band] = gain_db;
        RecomputeBand(band);
    }

    float GetBandGain(size_t band) const {
        return (band < kBandCount) ? m_gains_db[band] : 0.0f;
    }

    bool IsFlat() const noexcept {
        for (size_t i = 0; i < kBandCount; ++i) {
            if (std::abs(m_gains_db[i]) > 0.001f) return false;
        }
        return true;
    }

    void Reset() {
        for (size_t i = 0; i < kBandCount; ++i) {
            m_gains_db[i] = 0.0f;
            m_filters[i].Reset();
        }
        Recompute();
    }

    void ProcessFrame(float& left, float& right) noexcept {
        if (IsFlat()) return;
        for (size_t i = 0; i < kBandCount; ++i) {
            if (std::abs(m_gains_db[i]) > 0.001f) {
                m_filters[i].ProcessFrame(left, right);
            }
        }
    }

    float EvaluateMagnitudeDb(float f) const noexcept {
        float total_db = 0.0f;
        for (size_t i = 0; i < kBandCount; ++i) {
            total_db += m_filters[i].EvaluateMagnitudeDb(f);
        }
        return total_db;
    }

private:
    void Recompute() {
        for (size_t i = 0; i < kBandCount; ++i) {
            RecomputeBand(i);
        }
    }

    void RecomputeBand(size_t i) {
        if (i == 0) {
            // Audiophile Low Shelf for deep solid sub-bass (31.25 Hz)
            m_filters[i].Configure(
                BiquadType::LowShelf,
                m_sample_rate,
                kFrequencies[i],
                m_gains_db[i],
                0.7071
            );
        } else if (i == kBandCount - 1) {
            // High Shelf for silky air without Nyquist reflection
            const double high_freq = std::min(16000.0, m_sample_rate * 0.46);
            m_filters[i].Configure(
                BiquadType::HighShelf,
                m_sample_rate,
                high_freq,
                m_gains_db[i],
                0.7071
            );
        } else {
            // Constant-Q Peaking filter with tight Q = 1.85 for surgical note separation
            m_filters[i].Configure(
                BiquadType::Peaking,
                m_sample_rate,
                kFrequencies[i],
                m_gains_db[i],
                1.85
            );
        }
    }

    float m_sample_rate = 44100.0f;
    std::array<float, kBandCount> m_gains_db;
    std::array<BiquadFilter, kBandCount> m_filters;
};

// 12-band fully parametric equalizer
struct ParametricBandConfig {
    bool enabled = false;
    BiquadType type = BiquadType::Peaking;
    float frequency = 1000.0f;
    float gain_db = 0.0f;
    float q = 0.707f;
};

class ParametricEqualizer {
public:
    static constexpr size_t kMaxBands = 12;

    ParametricEqualizer() {
        for (size_t i = 0; i < kMaxBands; ++i) {
            m_bands[i].enabled = false;
            m_bands[i].type = BiquadType::Peaking;
            m_bands[i].frequency = 100.0f * std::pow(2.0f, static_cast<float>(i));
            m_bands[i].gain_db = 0.0f;
            m_bands[i].q = 0.707f;
        }
    }

    void SetSampleRate(float sample_rate) {
        m_sample_rate = sample_rate;
        for (size_t i = 0; i < kMaxBands; ++i) {
            m_filters[i].Reset();
        }
        Recompute();
    }

    void ConfigureBand(size_t index, const ParametricBandConfig& cfg) {
        if (index >= kMaxBands) return;
        if (m_bands[index].enabled == cfg.enabled &&
            m_bands[index].type == cfg.type &&
            std::abs(m_bands[index].frequency - cfg.frequency) < 0.01f &&
            std::abs(m_bands[index].gain_db - cfg.gain_db) < 0.0001f &&
            std::abs(m_bands[index].q - cfg.q) < 0.001f) {
            return;
        }
        m_bands[index] = cfg;
        RecomputeBand(index);
    }

    const ParametricBandConfig& GetBand(size_t index) const {
        return m_bands[index];
    }

    bool HasActiveBands() const noexcept {
        for (size_t i = 0; i < kMaxBands; ++i) {
            if (!m_bands[i].enabled) continue;
            const bool is_gain_type = (m_bands[i].type == BiquadType::Peaking ||
                                       m_bands[i].type == BiquadType::LowShelf ||
                                       m_bands[i].type == BiquadType::HighShelf);
            if (is_gain_type) {
                if (std::abs(m_bands[i].gain_db) > 0.001f) return true;
            } else if (m_bands[i].type != BiquadType::Flat) {
                return true; // LowPass, HighPass, Notch, BandPass are always active when enabled
            }
        }
        return false;
    }

    void Reset() {
        for (size_t i = 0; i < kMaxBands; ++i) {
            m_bands[i].enabled = false;
            m_bands[i].gain_db = 0.0f;
            m_filters[i].Reset();
        }
        Recompute();
    }

    void ProcessFrame(float& left, float& right) noexcept {
        for (size_t i = 0; i < kMaxBands; ++i) {
            if (m_bands[i].enabled) {
                m_filters[i].ProcessFrame(left, right);
            }
        }
    }

    float EvaluateMagnitudeDb(float f) const noexcept {
        float total_db = 0.0f;
        for (size_t i = 0; i < kMaxBands; ++i) {
            if (m_bands[i].enabled) {
                total_db += m_filters[i].EvaluateMagnitudeDb(f);
            }
        }
        return total_db;
    }

private:
    void Recompute() {
        for (size_t i = 0; i < kMaxBands; ++i) {
            RecomputeBand(i);
        }
    }

    void RecomputeBand(size_t i) {
        if (!m_bands[i].enabled) {
            m_filters[i].Configure(BiquadType::Flat, m_sample_rate, 1000.0, 0.0, 1.0);
            return;
        }
        m_filters[i].Configure(
            m_bands[i].type,
            m_sample_rate,
            m_bands[i].frequency,
            m_bands[i].gain_db,
            m_bands[i].q
        );
    }

    float m_sample_rate = 44100.0f;
    std::array<ParametricBandConfig, kMaxBands> m_bands;
    std::array<BiquadFilter, kMaxBands> m_filters;
};
