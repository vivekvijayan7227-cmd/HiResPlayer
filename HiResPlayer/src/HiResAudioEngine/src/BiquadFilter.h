#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

// Biquad filter types based on RBJ Audio EQ Cookbook
enum class BiquadType : uint32_t {
    Flat = 0,
    LowShelf,
    HighShelf,
    Peaking,
    LowPass,
    HighPass,
    Notch,
    BandPass
};

// Studio-grade 64-bit double precision Direct Form II Transposed biquad filter.
// Using double-precision internal coefficients and delay registers eliminates
// low-frequency quantization noise (31 Hz - 125 Hz) and maintains pristine phase response.
class BiquadFilter {
public:
    BiquadFilter() = default;

    void Reset() noexcept {
        m_s1[0] = m_s1[1] = 0.0;
        m_s2[0] = m_s2[1] = 0.0;
    }

    void Configure(BiquadType type, double sample_rate, double frequency, double gain_db, double q) noexcept {
        m_type = type;
        m_sample_rate = std::max(sample_rate, 8000.0);
        m_frequency = std::clamp(frequency, 10.0, m_sample_rate * 0.49); // Below Nyquist
        m_gain_db = std::clamp(gain_db, -24.0, +24.0);
        m_q = std::clamp(q, 0.1, 20.0);

        const bool is_flat_gain = (type == BiquadType::Peaking ||
                                   type == BiquadType::LowShelf ||
                                   type == BiquadType::HighShelf) && (std::abs(m_gain_db) < 0.001);
        if (type == BiquadType::Flat || is_flat_gain) {
            m_b0 = 1.0; m_b1 = 0.0; m_b2 = 0.0;
            m_a1 = 0.0; m_a2 = 0.0;
            return;
        }

        const double omega = 2.0 * 3.14159265358979323846 * m_frequency / m_sample_rate;
        const double sin_w = std::sin(omega);
        const double cos_w = std::cos(omega);
        const double alpha = sin_w / (2.0 * m_q);
        const double A = std::pow(10.0, m_gain_db / 40.0); // sqrt(gain)

        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a0 = 1.0, a1 = 0.0, a2 = 0.0;

        switch (type) {
        case BiquadType::Peaking: {
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * cos_w;
            b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A;
            a1 = -2.0 * cos_w;
            a2 = 1.0 - alpha / A;
            break;
        }
        case BiquadType::LowShelf: {
            const double a_plus_1  = A + 1.0;
            const double a_minus_1 = A - 1.0;
            const double sqrt_2_a_alpha = 2.0 * std::sqrt(A) * alpha;

            b0 =    A * (a_plus_1 - a_minus_1 * cos_w + sqrt_2_a_alpha);
            b1 = 2.0 * A * (a_minus_1 - a_plus_1 * cos_w);
            b2 =    A * (a_plus_1 - a_minus_1 * cos_w - sqrt_2_a_alpha);
            a0 =         a_plus_1 + a_minus_1 * cos_w + sqrt_2_a_alpha;
            a1 = -2.0 * (a_minus_1 + a_plus_1 * cos_w);
            a2 =         a_plus_1 + a_minus_1 * cos_w - sqrt_2_a_alpha;
            break;
        }
        case BiquadType::HighShelf: {
            const double a_plus_1  = A + 1.0;
            const double a_minus_1 = A - 1.0;
            const double sqrt_2_a_alpha = 2.0 * std::sqrt(A) * alpha;

            b0 =    A * (a_plus_1 + a_minus_1 * cos_w + sqrt_2_a_alpha);
            b1 = -2.0 * A * (a_minus_1 + a_plus_1 * cos_w);
            b2 =    A * (a_plus_1 + a_minus_1 * cos_w - sqrt_2_a_alpha);
            a0 =         a_plus_1 - a_minus_1 * cos_w + sqrt_2_a_alpha;
            a1 =  2.0 * (a_minus_1 - a_plus_1 * cos_w);
            a2 =         a_plus_1 - a_minus_1 * cos_w - sqrt_2_a_alpha;
            break;
        }
        case BiquadType::LowPass: {
            b0 = (1.0 - cos_w) * 0.5;
            b1 = 1.0 - cos_w;
            b2 = (1.0 - cos_w) * 0.5;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_w;
            a2 = 1.0 - alpha;
            break;
        }
        case BiquadType::HighPass: {
            b0 = (1.0 + cos_w) * 0.5;
            b1 = -(1.0 + cos_w);
            b2 = (1.0 + cos_w) * 0.5;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_w;
            a2 = 1.0 - alpha;
            break;
        }
        case BiquadType::Notch: {
            b0 = 1.0;
            b1 = -2.0 * cos_w;
            b2 = 1.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_w;
            a2 = 1.0 - alpha;
            break;
        }
        case BiquadType::BandPass: {
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_w;
            a2 = 1.0 - alpha;
            break;
        }
        default:
            b0 = 1.0; b1 = 0.0; b2 = 0.0;
            a0 = 1.0; a1 = 0.0; a2 = 0.0;
            break;
        }

        const double inv_a0 = 1.0 / a0;
        m_b0 = b0 * inv_a0;
        m_b1 = b1 * inv_a0;
        m_b2 = b2 * inv_a0;
        m_a1 = a1 * inv_a0;
        m_a2 = a2 * inv_a0;
    }

    // Process a single stereo sample frame in-place [Direct Form II Transposed in double precision]
    inline void ProcessFrame(float& left, float& right) noexcept {
        const double in_l = static_cast<double>(left);
        const double out_l = m_b0 * in_l + m_s1[0];
        m_s1[0] = m_b1 * in_l - m_a1 * out_l + m_s2[0];
        m_s2[0] = m_b2 * in_l - m_a2 * out_l;
        left = static_cast<float>(out_l);

        const double in_r = static_cast<double>(right);
        const double out_r = m_b0 * in_r + m_s1[1];
        m_s1[1] = m_b1 * in_r - m_a1 * out_r + m_s2[1];
        m_s2[1] = m_b2 * in_r - m_a2 * out_r;
        right = static_cast<float>(out_r);
    }

    // Calculate theoretical magnitude response in dB at specified frequency f
    float EvaluateMagnitudeDb(float f) const noexcept {
        if (m_type == BiquadType::Flat) return 0.0f;
        const double w = 2.0 * 3.14159265358979323846 * static_cast<double>(f) / m_sample_rate;
        const double cos_w = std::cos(w);
        const double cos_2w = std::cos(2.0 * w);
        const double sin_w = std::sin(w);
        const double sin_2w = std::sin(2.0 * w);

        const double num_re = m_b0 + m_b1 * cos_w + m_b2 * cos_2w;
        const double num_im = -m_b1 * sin_w - m_b2 * sin_2w;
        const double den_re = 1.0 + m_a1 * cos_w + m_a2 * cos_2w;
        const double den_im = -m_a1 * sin_w - m_a2 * sin_2w;

        const double num_sq = num_re * num_re + num_im * num_im;
        const double den_sq = den_re * den_re + den_im * den_im;
        if (den_sq <= 1e-15) return 0.0f;

        return static_cast<float>(10.0 * std::log10(num_sq / den_sq));
    }

private:
    BiquadType m_type = BiquadType::Flat;
    double m_sample_rate = 44100.0;
    double m_frequency = 1000.0;
    double m_gain_db = 0.0;
    double m_q = 0.707;

    double m_b0 = 1.0, m_b1 = 0.0, m_b2 = 0.0;
    double m_a1 = 0.0, m_a2 = 0.0;

    double m_s1[2] = { 0.0, 0.0 };
    double m_s2[2] = { 0.0, 0.0 };
};
