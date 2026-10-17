#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cstdint>

// Real-time Spectrum Analyzer & Stereo Correlation Monitor
// Features:
// - Windowed Radix-2 Cooley-Tukey 1024-point FFT with Hann window
// - Calibrated 32-band log-frequency binning (20 Hz - 20,000 Hz)
// - True interchannel stereo phase correlation calculation (-1.0 to +1.0)
// - Digital silence detector (bars decay to 0.0 on zero audio)
// - Lock-free ring buffer receives live PCM frames from WASAPI render thread
// - Satisfies PRD AUD-11, QA-11, and QA-13 requirements.
class SpectrumAnalyzer {
public:
    static constexpr size_t FFT_SIZE = 1024;
    static constexpr size_t RING_SIZE = 4096;

    SpectrumAnalyzer()
        : m_ring(RING_SIZE, 0.0f), m_write_pos(0), m_sample_rate(44100.0f)
    {
        m_hann.resize(FFT_SIZE);
        for (size_t i = 0; i < FFT_SIZE; ++i) {
            m_hann[i] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265358979323846f * i / (FFT_SIZE - 1)));
        }
        m_smoothed_bands.resize(64, 0.0f);
    }

    void SetSampleRate(float sr) noexcept {
        if (sr > 1000.0f) {
            m_sample_rate.store(sr, std::memory_order_relaxed);
        }
    }

    // Called on WASAPI render thread: writes stereo audio frames into circular buffer and calculates phase correlation
    void FeedStereoSamples(const float* left, const float* right, size_t count) noexcept {
        if (!left || !right || count == 0) return;

        size_t wp = m_write_pos.load(std::memory_order_relaxed);
        double s_lr = 0.0, s_ll = 0.0, s_rr = 0.0;

        for (size_t i = 0; i < count; ++i) {
            float l = left[i];
            float r = right[i];
            m_ring[wp] = 0.5f * (l + r);
            wp = (wp + 1) % RING_SIZE;

            s_lr += static_cast<double>(l) * r;
            s_ll += static_cast<double>(l) * l;
            s_rr += static_cast<double>(r) * r;
        }
        m_write_pos.store(wp, std::memory_order_release);

        // Compute normalized Pearson cross-correlation r in [-1.0, +1.0]
        double denom = std::sqrt(s_ll * s_rr);
        float r_block = (denom > 1e-9) ? static_cast<float>(std::clamp(s_lr / denom, -1.0, 1.0)) : 1.0f;

        float cur_r = m_correlation.load(std::memory_order_relaxed);
        float new_r = cur_r * 0.82f + r_block * 0.18f;
        m_correlation.store(new_r, std::memory_order_relaxed);
    }

    // Single mono buffer fallback
    void FeedSamples(const float* mono, size_t count) noexcept {
        if (!mono || count == 0) return;
        size_t wp = m_write_pos.load(std::memory_order_relaxed);
        for (size_t i = 0; i < count; ++i) {
            m_ring[wp] = mono[i];
            wp = (wp + 1) % RING_SIZE;
        }
        m_write_pos.store(wp, std::memory_order_release);
    }

    float GetStereoCorrelation() const noexcept {
        return m_correlation.load(std::memory_order_relaxed);
    }

    // Called on UI thread via hp_dsp_get_spectrum
    void GetBands(float* out_bands, uint32_t band_count) noexcept {
        if (!out_bands || band_count == 0) return;
        if (band_count > 64) band_count = 64;

        float in_real[FFT_SIZE];
        float in_imag[FFT_SIZE];

        size_t wp = m_write_pos.load(std::memory_order_acquire);
        size_t start = (wp + RING_SIZE - FFT_SIZE) % RING_SIZE;

        double sum_energy = 0.0;
        for (size_t i = 0; i < FFT_SIZE; ++i) {
            float s = m_ring[(start + i) % RING_SIZE];
            sum_energy += s * s;
            in_real[i] = s * m_hann[i];
            in_imag[i] = 0.0f;
        }

        // Digital silence check: if RMS is lower than -80 dB, decay to 0 immediately
        if (sum_energy < 1e-8) {
            for (uint32_t b = 0; b < band_count; ++b) {
                m_smoothed_bands[b] *= 0.5f;
                if (m_smoothed_bands[b] < 0.005f) m_smoothed_bands[b] = 0.0f;
                out_bands[b] = m_smoothed_bands[b];
            }
            return;
        }

        // Cooley-Tukey Radix-2 FFT
        ComputeFFT(in_real, in_imag, FFT_SIZE);

        constexpr size_t NUM_BINS = FFT_SIZE / 2;
        float mag[NUM_BINS];
        const float norm = 2.0f / static_cast<float>(FFT_SIZE);
        for (size_t i = 0; i < NUM_BINS; ++i) {
            float r = in_real[i];
            float im = in_imag[i];
            mag[i] = std::sqrt(r * r + im * im) * norm;
        }

        const float sr = m_sample_rate.load(std::memory_order_relaxed);
        const float bin_res = sr / static_cast<float>(FFT_SIZE);

        const float f_min = 20.0f;
        const float f_max = std::min(20000.0f, sr * 0.48f);
        const float log_min = std::log10(f_min);
        const float log_max = std::log10(f_max);

        for (uint32_t b = 0; b < band_count; ++b) {
            float f_start = std::pow(10.0f, log_min + (log_max - log_min) * (static_cast<float>(b) / band_count));
            float f_end   = std::pow(10.0f, log_min + (log_max - log_min) * (static_cast<float>(b + 1) / band_count));

            size_t k_start = static_cast<size_t>(std::clamp(f_start / bin_res, 0.0f, static_cast<float>(NUM_BINS - 1)));
            size_t k_end   = static_cast<size_t>(std::clamp(f_end / bin_res, static_cast<float>(k_start + 1), static_cast<float>(NUM_BINS)));

            float peak_mag = 0.0f;
            for (size_t k = k_start; k < k_end; ++k) {
                if (mag[k] > peak_mag) peak_mag = mag[k];
            }

            // Convert to dB: range -60 dB to 0 dB mapped to [0.0, 1.0]
            float db = 20.0f * std::log10(std::max(peak_mag, 1e-5f));
            float val = (db + 60.0f) / 60.0f;
            val = std::clamp(val, 0.0f, 1.0f);

            // Per-band peak hold / decay smoothing
            if (val > m_smoothed_bands[b]) {
                m_smoothed_bands[b] = val; // Instant attack
            } else {
                m_smoothed_bands[b] = m_smoothed_bands[b] * 0.82f; // Smooth decay
            }

            out_bands[b] = m_smoothed_bands[b];
        }
    }

private:
    static void ComputeFFT(float* real, float* imag, size_t n) noexcept {
        size_t j = 0;
        for (size_t i = 0; i < n - 1; ++i) {
            if (i < j) {
                std::swap(real[i], real[j]);
                std::swap(imag[i], imag[j]);
            }
            size_t k = n >> 1;
            while (k <= j) {
                j -= k;
                k >>= 1;
            }
            j += k;
        }

        for (size_t len = 2; len <= n; len <<= 1) {
            float angle = -2.0f * 3.14159265358979323846f / static_cast<float>(len);
            float wlen_r = std::cos(angle);
            float wlen_i = std::sin(angle);
            for (size_t i = 0; i < n; i += len) {
                float w_r = 1.0f;
                float w_i = 0.0f;
                for (size_t k = 0; k < len / 2; ++k) {
                    size_t u = i + k;
                    size_t v = i + k + len / 2;
                    float v_r = real[v] * w_r - imag[v] * w_i;
                    float v_i = real[v] * w_i + imag[v] * w_r;
                    real[v] = real[u] - v_r;
                    imag[v] = imag[u] - v_i;
                    real[u] += v_r;
                    imag[u] += v_i;
                    float next_w_r = w_r * wlen_r - w_i * wlen_i;
                    float next_w_i = w_r * wlen_i + w_i * wlen_r;
                    w_r = next_w_r;
                    w_i = next_w_i;
                }
            }
        }
    }

    std::vector<float> m_ring;
    std::atomic<size_t> m_write_pos;
    std::atomic<float> m_sample_rate;
    std::atomic<float> m_correlation{1.0f};
    std::vector<float> m_hann;
    std::vector<float> m_smoothed_bands;
};
