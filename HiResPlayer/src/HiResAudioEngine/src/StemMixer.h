#pragma once
#include <cstdint>
#include <vector>
#include <atomic>
#include <algorithm>

// StemMixer mixes pre-extracted stems (e.g. vocals and accompaniment)
// from cached PCM buffers with independent volume controls, mute, and solo.
class StemMixer {
public:
    StemMixer() = default;

    void Configure(float vocal_volume, float inst_volume, bool mute_vocal, bool mute_inst) noexcept {
        m_vocal_vol = mute_vocal ? 0.0f : std::clamp(vocal_volume, 0.0f, 2.0f);
        m_inst_vol  = mute_inst  ? 0.0f : std::clamp(inst_volume,  0.0f, 2.0f);
        m_active    = true;
    }

    void Disable() noexcept {
        m_active = false;
    }

    bool IsActive() const noexcept {
        return m_active;
    }

    float GetVocalVol() const noexcept { return m_vocal_vol; }
    float GetInstVol() const noexcept { return m_inst_vol; }

    // Mix vocal and instrumental frames into left and right output
    inline void MixFrame(
        float vocal_l, float vocal_r,
        float inst_l,  float inst_r,
        float& out_l,  float& out_r
    ) const noexcept {
        if (!m_active) {
            out_l = vocal_l + inst_l;
            out_r = vocal_r + inst_r;
            return;
        }

        out_l = (vocal_l * m_vocal_vol) + (inst_l * m_inst_vol);
        out_r = (vocal_r * m_vocal_vol) + (inst_r * m_inst_vol);
    }

private:
    bool m_active = false;
    float m_vocal_vol = 1.0f;
    float m_inst_vol  = 1.0f;
};
