#include "PlaybackController.h"
#include "BluetoothDeviceService.h"
#include "PcmSamplePacker.h"
#include <iostream>
#include <chrono>
#include <xmmintrin.h>

static inline bool IsFormatIeeeFloat(const WAVEFORMATEXTENSIBLE& wfex) noexcept {
    if (wfex.Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
    if (wfex.Format.wFormatTag == WAVE_FORMAT_PCM) return false;
    if (wfex.Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        static const GUID kFloat = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
        return std::memcmp(&wfex.SubFormat, &kFloat, sizeof(GUID)) == 0;
    }
    return false;
}

PlaybackController::PlaybackController()
    : m_ring_buffer(1024 * 1024)
{
    m_device_mgr = new AudioDeviceManager();
    m_device_mgr->Initialize();
    m_device_mgr->SetEventCallback([this](uint32_t evt, const std::wstring& endpoint_id) {
        if (m_dev_cb) {
            m_dev_cb(m_dev_user_data, evt, endpoint_id.c_str());
        }
    });

    m_renderer.SetEventCallback([this](uint32_t evt, int32_t hr, uint64_t pos) {
        FirePlaybackEvent(evt, m_state.load(), hr, pos);
    });

    // Real-time DSP & Spectrum callback: processes the audio block directly in the WASAPI render thread
    m_renderer.SetDspCallback([this](uint8_t* p_data, UINT32 frames, const WAVEFORMATEXTENSIBLE& wfex) {
        if (!p_data || frames == 0) return;

        const bool dsp_active = m_dsp_graph.IsDspActive();
        const uint32_t channels = wfex.Format.nChannels;
        if (channels == 0) return;

        // Zero-allocation preallocated scratch buffer to prevent audio thread glitches
        constexpr size_t kMaxScratchFrames = 8192;
        thread_local float left_buf[kMaxScratchFrames];
        thread_local float right_buf[kMaxScratchFrames];
        const UINT32 proc_frames = (frames <= kMaxScratchFrames) ? frames : static_cast<UINT32>(kMaxScratchFrames);

        // Path 1: 32-bit IEEE float (strict tag and subtype checking)
        if (IsFormatIeeeFloat(wfex)) {
            float* f = reinterpret_cast<float*>(p_data);
            if (channels == 1) {
                for (UINT32 i = 0; i < proc_frames; ++i) {
                    float l = f[i];
                    float r = l;
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(l, r);
                        f[i] = 0.5f * (l + r);
                    }
                    left_buf[i]  = l;
                    right_buf[i] = r;
                }
            } else if (channels == 2) {
                for (UINT32 i = 0; i < proc_frames; ++i) {
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(f[i * 2], f[i * 2 + 1]);
                    }
                    left_buf[i]  = f[i * 2];
                    right_buf[i] = f[i * 2 + 1];
                }
            } else {
                for (UINT32 i = 0; i < proc_frames; ++i) {
                    const size_t base = i * channels;
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(f[base], f[base + 1]);
                    }
                    left_buf[i]  = f[base];
                    right_buf[i] = f[base + 1];
                }
            }
        }
        // Path 2: Exclusive mode 24-in-32 container
        else if (wfex.Format.wBitsPerSample == 32 && wfex.Samples.wValidBitsPerSample == 24) {
            int32_t* s = reinterpret_cast<int32_t*>(p_data);
            if (channels == 1) {
                for (UINT32 i = 0; i < proc_frames; ++i) {
                    float l = static_cast<float>(s[i] >> 8) / 8388608.0f;
                    float r = l;
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(l, r);
                        float mono_out = 0.5f * (l + r);
                        int32_t is = std::clamp(static_cast<int32_t>(std::round(mono_out < 0 ? mono_out * 8388608.0f : mono_out * 8388607.0f)), -8388608, 8388607);
                        s[i] = is << 8;
                    }
                    left_buf[i]  = l;
                    right_buf[i] = r;
                }
            } else if (channels == 2) {
                for (UINT32 i = 0; i < proc_frames; ++i) {
                    float l = static_cast<float>(s[i * 2] >> 8) / 8388608.0f;
                    float r = static_cast<float>(s[i * 2 + 1] >> 8) / 8388608.0f;
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(l, r);
                        int32_t il = std::clamp(static_cast<int32_t>(std::round(l < 0 ? l * 8388608.0f : l * 8388607.0f)), -8388608, 8388607);
                        int32_t ir = std::clamp(static_cast<int32_t>(std::round(r < 0 ? r * 8388608.0f : r * 8388607.0f)), -8388608, 8388607);
                        s[i * 2]     = il << 8;
                        s[i * 2 + 1] = ir << 8;
                    }
                    left_buf[i]  = l;
                    right_buf[i] = r;
                }
            } else {
                for (UINT32 i = 0; i < proc_frames; ++i) {
                    const size_t base = i * channels;
                    float l = static_cast<float>(s[base] >> 8) / 8388608.0f;
                    float r = static_cast<float>(s[base + 1] >> 8) / 8388608.0f;
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(l, r);
                        int32_t il = std::clamp(static_cast<int32_t>(std::round(l < 0 ? l * 8388608.0f : l * 8388607.0f)), -8388608, 8388607);
                        int32_t ir = std::clamp(static_cast<int32_t>(std::round(r < 0 ? r * 8388608.0f : r * 8388607.0f)), -8388608, 8388607);
                        s[base]     = il << 8;
                        s[base + 1] = ir << 8;
                    }
                    left_buf[i]  = l;
                    right_buf[i] = r;
                }
            }
        }
        // Path 3: Exclusive mode 16-in-32 container
        else if (wfex.Format.wBitsPerSample == 32 && wfex.Samples.wValidBitsPerSample == 16) {
            int32_t* s = reinterpret_cast<int32_t*>(p_data);
            if (channels == 1) {
                for (UINT32 i = 0; i < proc_frames; ++i) {
                    float l = static_cast<float>(s[i] >> 16) / 32768.0f;
                    float r = l;
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(l, r);
                        float mono_out = 0.5f * (l + r);
                        int32_t is = std::clamp(static_cast<int32_t>(std::round(mono_out < 0 ? mono_out * 32768.0f : mono_out * 32767.0f)), -32768, 32767);
                        s[i] = is << 16;
                    }
                    left_buf[i]  = l;
                    right_buf[i] = r;
                }
            } else if (channels == 2) {
                for (UINT32 i = 0; i < proc_frames; ++i) {
                    float l = static_cast<float>(s[i * 2] >> 16) / 32768.0f;
                    float r = static_cast<float>(s[i * 2 + 1] >> 16) / 32768.0f;
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(l, r);
                        int32_t il = std::clamp(static_cast<int32_t>(std::round(l < 0 ? l * 32768.0f : l * 32767.0f)), -32768, 32767);
                        int32_t ir = std::clamp(static_cast<int32_t>(std::round(r < 0 ? r * 32768.0f : r * 32767.0f)), -32768, 32767);
                        s[i * 2]     = il << 16;
                        s[i * 2 + 1] = ir << 16;
                    }
                    left_buf[i]  = l;
                    right_buf[i] = r;
                }
            } else {
                for (UINT32 i = 0; i < proc_frames; ++i) {
                    const size_t base = i * channels;
                    float l = static_cast<float>(s[base] >> 16) / 32768.0f;
                    float r = static_cast<float>(s[base + 1] >> 16) / 32768.0f;
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(l, r);
                        int32_t il = std::clamp(static_cast<int32_t>(std::round(l < 0 ? l * 32768.0f : l * 32767.0f)), -32768, 32767);
                        int32_t ir = std::clamp(static_cast<int32_t>(std::round(r < 0 ? r * 32768.0f : r * 32767.0f)), -32768, 32767);
                        s[base]     = il << 16;
                        s[base + 1] = ir << 16;
                    }
                    left_buf[i]  = l;
                    right_buf[i] = r;
                }
            }
        }
        // Path 4: True 32-bit integer PCM
        else if (wfex.Format.wBitsPerSample == 32) {
            int32_t* s = reinterpret_cast<int32_t*>(p_data);
            for (UINT32 i = 0; i < proc_frames; ++i) {
                float l = static_cast<float>(s[i * channels]) / 2147483648.0f;
                float r = (channels >= 2) ? (static_cast<float>(s[i * channels + 1]) / 2147483648.0f) : l;
                if (dsp_active) {
                    m_dsp_graph.ProcessFrame(l, r);
                    s[i * channels] = static_cast<int32_t>(std::clamp(static_cast<double>(l) * 2147483647.0, -2147483648.0, 2147483647.0));
                    if (channels >= 2) {
                        s[i * channels + 1] = static_cast<int32_t>(std::clamp(static_cast<double>(r) * 2147483647.0, -2147483648.0, 2147483647.0));
                    }
                }
                left_buf[i]  = l;
                right_buf[i] = r;
            }
        }
        // Path 5: Exclusive mode 24-bit packed (3 bytes per sample)
        else if (wfex.Format.wBitsPerSample == 24) {
            const size_t frame_bytes = channels * 3;
            for (UINT32 i = 0; i < proc_frames; ++i) {
                uint8_t* p = p_data + i * frame_bytes;
                int32_t sl = (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16));
                if (sl & 0x800000) sl |= 0xFF000000;
                float l = static_cast<float>(sl) / 8388608.0f;
                float r = l;

                if (channels >= 2) {
                    int32_t sr = (int32_t)((uint32_t)p[3] | ((uint32_t)p[4] << 8) | ((uint32_t)p[5] << 16));
                    if (sr & 0x800000) sr |= 0xFF000000;
                    r = static_cast<float>(sr) / 8388608.0f;
                }

                if (dsp_active) {
                    m_dsp_graph.ProcessFrame(l, r);
                    int32_t il = std::clamp(static_cast<int32_t>(std::round(l < 0 ? l * 8388608.0f : l * 8388607.0f)), -8388608, 8388607);
                    p[0] = static_cast<uint8_t>(il & 0xFF);
                    p[1] = static_cast<uint8_t>((il >> 8) & 0xFF);
                    p[2] = static_cast<uint8_t>((il >> 16) & 0xFF);
                    if (channels >= 2) {
                        int32_t ir = std::clamp(static_cast<int32_t>(std::round(r < 0 ? r * 8388608.0f : r * 8388607.0f)), -8388608, 8388607);
                        p[3] = static_cast<uint8_t>(ir & 0xFF);
                        p[4] = static_cast<uint8_t>((ir >> 8) & 0xFF);
                        p[5] = static_cast<uint8_t>((ir >> 16) & 0xFF);
                    }
                }
                left_buf[i]  = l;
                right_buf[i] = r;
            }
        }
        // Path 6: Exclusive mode 16-bit packed
        else if (wfex.Format.wBitsPerSample == 16) {
            int16_t* s = reinterpret_cast<int16_t*>(p_data);
            if (channels == 1) {
                for (UINT32 i = 0; i < proc_frames; ++i) {
                    float l = static_cast<float>(s[i]) / 32768.0f;
                    float r = l;
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(l, r);
                        float mono_out = 0.5f * (l + r);
                        s[i] = static_cast<int16_t>(std::clamp(static_cast<int32_t>(std::round(mono_out < 0 ? mono_out * 32768.0f : mono_out * 32767.0f)), -32768, 32767));
                    }
                    left_buf[i]  = l;
                    right_buf[i] = r;
                }
            } else if (channels == 2) {
                for (UINT32 i = 0; i < proc_frames; ++i) {
                    float l = static_cast<float>(s[i * 2]) / 32768.0f;
                    float r = static_cast<float>(s[i * 2 + 1]) / 32768.0f;
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(l, r);
                        s[i * 2]     = static_cast<int16_t>(std::clamp(static_cast<int32_t>(std::round(l < 0 ? l * 32768.0f : l * 32767.0f)), -32768, 32767));
                        s[i * 2 + 1] = static_cast<int16_t>(std::clamp(static_cast<int32_t>(std::round(r < 0 ? r * 32768.0f : r * 32767.0f)), -32768, 32767));
                    }
                    left_buf[i]  = l;
                    right_buf[i] = r;
                }
            } else {
                for (UINT32 i = 0; i < proc_frames; ++i) {
                    const size_t base = i * channels;
                    float l = static_cast<float>(s[base]) / 32768.0f;
                    float r = static_cast<float>(s[base + 1]) / 32768.0f;
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(l, r);
                        s[base]     = static_cast<int16_t>(std::clamp(static_cast<int32_t>(std::round(l < 0 ? l * 32768.0f : l * 32767.0f)), -32768, 32767));
                        s[base + 1] = static_cast<int16_t>(std::clamp(static_cast<int32_t>(std::round(r < 0 ? r * 32768.0f : r * 32767.0f)), -32768, 32767));
                    }
                    left_buf[i]  = l;
                    right_buf[i] = r;
                }
            }
        }

        m_spectrum_analyzer.FeedStereoSamples(left_buf, right_buf, proc_frames);
    });
}

PlaybackController::~PlaybackController() {
    Stop();
    if (m_device_mgr) {
        m_device_mgr->Shutdown();
        m_device_mgr->Release();
        m_device_mgr = nullptr;
    }
}


HP_Result PlaybackController::OpenFlac(const std::wstring& path) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    Stop();

    std::string err;
    if (!m_decoder.Open(path, err)) {
        return HP_INVALID_ARGUMENT;
    }

    m_current_file = path;
    const auto& meta = m_decoder.metadata();
    m_dsp_graph.SetSampleRate(static_cast<float>(meta.sample_rate));
    m_spectrum_analyzer.SetSampleRate(static_cast<float>(meta.sample_rate));
    m_dsp_graph.ResetFilterStates();
    m_resampler.Reset();
    m_ring_buffer.set_frame_size(meta.channels * ((meta.bits_per_sample == 24 ? 32 : meta.bits_per_sample) / 8));

    UpdateState(HP_STATE_STOPPED);
    return HP_OK;
}

HP_Result PlaybackController::SelectDevice(const std::wstring& endpoint_id) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const bool was_playing = (m_state.load() == HP_STATE_PLAYING);
    const bool was_paused  = (m_state.load() == HP_STATE_PAUSED);
    uint64_t pos = 0;
    if (was_playing || was_paused) {
        pos = m_renderer.GetRenderedSamples();
        Stop();
    }

    m_selected_device_id = endpoint_id;
    m_selected_device_name.clear();

    const auto devices = m_device_mgr->EnumerateDevices();
    for (const auto& dev : devices) {
        if (dev.id == endpoint_id) {
            m_selected_device_name = dev.friendly_name;
            break;
        }
    }

    if (was_playing || was_paused) {
        if (pos > 0) {
            m_seek_target.store(pos);
            m_seek_requested.store(true);
        }
        Play();
        if (was_paused) {
            Pause();
        }
    }
    return HP_OK;
}

HP_Result PlaybackController::SetMode(HP_PlaybackMode mode) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_requested_mode == mode) return HP_OK;
    m_requested_mode = mode;
    const bool was_playing = (m_state.load() == HP_STATE_PLAYING);
    const bool was_paused  = (m_state.load() == HP_STATE_PAUSED);
    if (was_playing || was_paused) {
        const uint64_t pos = m_renderer.GetRenderedSamples();
        Stop();
        if (pos > 0) {
            m_seek_target.store(pos);
            m_seek_requested.store(true);
        }
        Play();
        if (was_paused) {
            Pause();
        }
    }
    return HP_OK;
}

HP_Result PlaybackController::Play() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_current_file.empty()) return HP_INVALID_ARGUMENT;

    if (m_state.load() == HP_STATE_PAUSED) {
        m_renderer.SetPaused(false);
        UpdateState(HP_STATE_PLAYING);
        return HP_OK;
    }

    if (m_state.load() == HP_STATE_PLAYING) {
        return HP_OK;
    }

    // Safely join any finished or previous decode thread to prevent std::terminate()
    JoinDecodeThread();

    // Determine target endpoint ID
    std::wstring dev_id = m_selected_device_id;
    if (dev_id.empty()) {
        dev_id = m_device_mgr->GetDefaultDeviceId();
        m_selected_device_id = dev_id;
    }

    IMMDevice* dev = nullptr;
    HRESULT hr_dev = m_device_mgr->GetDevice(dev_id, &dev);
    if (SUCCEEDED(hr_dev) && dev) {
        m_renderer.SetDevice(dev);
        dev->Release();
    } else {
        FirePlaybackEvent(HP_EVT_ERROR, HP_STATE_STOPPED, hr_dev, 0);
        return HP_DEVICE_UNAVAILABLE;
    }

    const auto& meta = m_decoder.metadata();

    RendererConfig cfg;
    cfg.endpoint_id = dev_id;
    cfg.mode = m_requested_mode;
    cfg.source_rate = meta.sample_rate;
    cfg.source_bits = meta.bits_per_sample;
    cfg.source_channels = meta.channels;
    cfg.ring_buffer = &m_ring_buffer;
    m_decode_stop.store(false);
    m_current_gen = m_ring_buffer.reset(m_current_gen + 1);
    m_renderer.SetBufferGeneration(m_current_gen);

    std::wstring err;
    if (!m_renderer.Prepare(cfg, err)) {
        // Fallback: If exclusive mode was requested and failed, try shared mode
        if (m_requested_mode == HP_EXCLUSIVE) {
            cfg.mode = HP_SHARED;
            m_renderer.Stop();
            if (!m_renderer.Prepare(cfg, err)) {
                FirePlaybackEvent(HP_EVT_ERROR, HP_STATE_STOPPED, m_renderer.GetLastHResult(), 0);
                return HP_INTERNAL_ERROR;
            }
            m_actual_mode = HP_SHARED;
        } else {
            FirePlaybackEvent(HP_EVT_ERROR, HP_STATE_STOPPED, m_renderer.GetLastHResult(), 0);
            return HP_INTERNAL_ERROR;
        }
    } else {
        m_actual_mode = cfg.mode;
    }

    const auto actual_wfex = m_renderer.GetActualFormat();
    const size_t frame_bytes = (actual_wfex.Format.nBlockAlign > 0)
        ? actual_wfex.Format.nBlockAlign
        : (meta.channels * ((meta.bits_per_sample == 24 ? 32 : meta.bits_per_sample) / 8));
    m_ring_buffer.set_frame_size(frame_bytes);

    if (m_actual_mode == HP_SHARED && actual_wfex.Format.nSamplesPerSec > 0) {
        m_resampler.Reset(meta.sample_rate, actual_wfex.Format.nSamplesPerSec);
    } else {
        m_resampler.Reset(meta.sample_rate, meta.sample_rate);
    }

    const uint32_t dsp_sample_rate = (actual_wfex.Format.nSamplesPerSec > 0)
        ? actual_wfex.Format.nSamplesPerSec
        : meta.sample_rate;
    m_dsp_graph.SetSampleRate(static_cast<float>(dsp_sample_rate));
    m_spectrum_analyzer.SetSampleRate(static_cast<float>(dsp_sample_rate));

    // Seek or prepare decoder position
    std::string seek_err;
    if (m_seek_requested.load()) {
        uint64_t target = m_seek_target.load();
        m_decoder.SeekToSample(target, seek_err);
        m_renderer.SetRenderedSamples(target);
        m_seek_requested.store(false);
    } else {
        m_decoder.SeekToSample(0, seek_err);
        m_renderer.SetRenderedSamples(0);
    }

    // Launch decoding thread to begin prebuffering PCM
    m_decode_thread = std::thread([this]() {
        this->DecodeThreadProc();
    });

    // Frame-aligned prebuffering loop: wait until ~100ms of PCM is buffered before engaging DAC
    const size_t target_prebuffer_bytes = std::clamp(
        static_cast<size_t>((static_cast<uint64_t>(dsp_sample_rate) * frame_bytes * 100) / 1000),
        frame_bytes * 16,
        m_ring_buffer.capacity() / 2
    );

    auto prebuffer_start = std::chrono::steady_clock::now();
    while (!m_decode_stop.load() && m_ring_buffer.available_read() < target_prebuffer_bytes) {
        if (m_decoder.HasError() || !m_decode_thread.joinable() || m_state.load() == HP_STATE_STOPPED) {
            break;
        }
        if (std::chrono::steady_clock::now() - prebuffer_start > std::chrono::milliseconds(500)) {
            break; // Timeout safeguard
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    // Engage hardware WASAPI stream render thread
    if (!m_renderer.StartStream(err)) {
        FirePlaybackEvent(HP_EVT_ERROR, HP_STATE_STOPPED, m_renderer.GetLastHResult(), 0);
        Stop();
        return HP_INTERNAL_ERROR;
    }

    UpdateState(HP_STATE_PLAYING);
    return HP_OK;
}

HP_Result PlaybackController::Pause() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_state.load() == HP_STATE_PLAYING) {
        m_renderer.SetPaused(true);
        UpdateState(HP_STATE_PAUSED);
    }
    return HP_OK;
}

HP_Result PlaybackController::Stop() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    JoinDecodeThread();
    m_renderer.Stop();
    UpdateState(HP_STATE_STOPPED);
    return HP_OK;
}

HP_Result PlaybackController::Seek(uint64_t sample_pos) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_current_file.empty()) return HP_INVALID_ARGUMENT;

    const auto& meta = m_decoder.metadata();
    if (meta.total_samples > 0 && sample_pos >= meta.total_samples) {
        sample_pos = (meta.total_samples > 1000) ? (meta.total_samples - 1000) : 0;
    }

    const uint32_t state = m_state.load();
    if (state == HP_STATE_STOPPED) {
        m_seek_target.store(sample_pos);
        m_seek_requested.store(true);
        m_renderer.SetRenderedSamples(sample_pos);
        FirePlaybackEvent(HP_EVT_STATE_CHANGED, HP_STATE_STOPPED, 0, sample_pos);
        return HP_OK;
    }

    // 1. Tell decode thread to stop and abort libFLAC decoding safely
    JoinDecodeThread();

    // 2. Perform seek on the decoder while it is completely idle
    std::string err;
    m_decoder.SeekToSample(sample_pos, err);

    // 3. Reset ring buffer with new generation to flush old buffered frames
    m_current_gen = m_ring_buffer.reset(m_current_gen + 1);
    m_renderer.SetBufferGeneration(m_current_gen);
    m_renderer.SetRenderedSamples(sample_pos);

    // 4. Flush WASAPI renderer & reset streaming histories (AUD-14)
    m_renderer.Flush();
    m_resampler.Reset();
    m_dsp_graph.ResetFilterStates();

    // 5. Restart decode thread from new position
    m_decode_stop.store(false);
    m_decode_thread = std::thread([this]() {
        this->DecodeThreadProc();
    });

    if (state == HP_STATE_PLAYING) {
        for (int i = 0; i < 25; ++i) {
            if (m_ring_buffer.available_read() > 16384) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        m_renderer.Wakeup();
    }

    FirePlaybackEvent(HP_EVT_STATE_CHANGED, state, 0, sample_pos);
    return HP_OK;
}

HP_Result PlaybackController::SetVolume(float volume) {
    std::wstring dev_id = m_selected_device_id;
    if (dev_id.empty() && m_device_mgr) {
        dev_id = m_device_mgr->GetDefaultDeviceId();
    }
    if (!m_device_mgr) return HP_DEVICE_UNAVAILABLE;
    HRESULT hr = m_device_mgr->SetMasterVolume(dev_id, volume);
    return SUCCEEDED(hr) ? HP_OK : HP_DEVICE_UNAVAILABLE;
}

HP_Result PlaybackController::GetVolume(float* out_volume) {
    if (!out_volume) return HP_INVALID_ARGUMENT;
    std::wstring dev_id = m_selected_device_id;
    if (dev_id.empty() && m_device_mgr) {
        dev_id = m_device_mgr->GetDefaultDeviceId();
    }
    if (!m_device_mgr) return HP_DEVICE_UNAVAILABLE;
    HRESULT hr = m_device_mgr->GetMasterVolume(dev_id, out_volume);
    return SUCCEEDED(hr) ? HP_OK : HP_DEVICE_UNAVAILABLE;
}

void PlaybackController::JoinDecodeThread() noexcept {
    if (m_decode_thread.joinable()) {
        if (m_decode_thread.get_id() == std::this_thread::get_id()) {
            return;
        }
        m_decode_stop.store(true);
        m_decoder.RequestStop();
        try {
            m_decode_thread.join();
        } catch (...) {}
    }
    m_decoder.SetPcmCallback(nullptr);
}

void PlaybackController::DecodeThreadProc() {
    _mm_setcsr(_mm_getcsr() | 0x8040); // FTZ & DAZ for zero CPU denormal traps

    struct CallbackCleanup {
        FlacDecoder& d;
        ~CallbackCleanup() { d.SetPcmCallback(nullptr); }
    } cleanup{m_decoder};

    const auto& meta = m_decoder.metadata();
    const auto actual_wfex = m_renderer.GetActualFormat();

    // 1. Determine actual negotiated target format properties
    const uint32_t target_channels = (actual_wfex.Format.nChannels > 0) ? actual_wfex.Format.nChannels : meta.channels;

    bool is_output_float = false;
    if (actual_wfex.Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        static const GUID kGuidFloat = {
            0x00000003, 0x0000, 0x0010,
            { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 }
        };
        is_output_float = (std::memcmp(&actual_wfex.SubFormat, &kGuidFloat, sizeof(GUID)) == 0);
    } else if (actual_wfex.Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        is_output_float = true;
    } else if (m_actual_mode == HP_SHARED && actual_wfex.Format.wBitsPerSample == 32) {
        static const GUID kGuidPCM = {
            0x00000001, 0x0000, 0x0010,
            { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 }
        };
        if (std::memcmp(&actual_wfex.SubFormat, &kGuidPCM, sizeof(GUID)) != 0) {
            is_output_float = true;
        }
    }

    const uint32_t target_container_bits = (actual_wfex.Format.wBitsPerSample > 0)
        ? actual_wfex.Format.wBitsPerSample
        : (is_output_float ? 32 : ((meta.bits_per_sample == 24) ? 32 : meta.bits_per_sample));

    const uint32_t target_valid_bits = (actual_wfex.Samples.wValidBitsPerSample > 0)
        ? actual_wfex.Samples.wValidBitsPerSample
        : (is_output_float ? 32 : meta.bits_per_sample);

    const uint32_t target_block_align = (actual_wfex.Format.nBlockAlign > 0)
        ? actual_wfex.Format.nBlockAlign
        : (target_channels * (target_container_bits / 8));

    PackParams pack_params;
    pack_params.valid_bits = target_valid_bits;
    pack_params.container_bits = target_container_bits;
    pack_params.output_float = is_output_float;

    // Buffer scratchpads
    std::vector<float> stereo_float_buf;
    std::vector<float> resample_scratch;
    std::vector<uint8_t> out_scratch;

    if (m_actual_mode == HP_SHARED && m_resampler.NeedsResampling()) {
        resample_scratch.resize(65536 * 4);
    }

    m_decoder.SetPcmCallback([&](const int32_t* const* samples, uint32_t channels, uint32_t samples_per_ch, uint32_t bits) {
        if (m_decode_stop.load()) return;

        // Path A: Bit-perfect Exclusive Mode (matching channels and integer PCM)
        if (m_actual_mode == HP_EXCLUSIVE && channels == target_channels && !is_output_float) {
            size_t total_bytes = samples_per_ch * target_block_align;
            if (out_scratch.size() < total_bytes) {
                out_scratch.resize(total_bytes);
            }
            pack_frames(out_scratch.data(), samples, channels, samples_per_ch, pack_params);

            size_t written = 0;
            while (written < total_bytes && !m_decode_stop.load()) {
                if (m_current_gen != m_ring_buffer.get_write_gen()) break;
                size_t w = m_ring_buffer.write(out_scratch.data() + written, total_bytes - written, m_current_gen);
                written += w;
                if (written < total_bytes) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
            return;
        }

        // Path B: Shared Mode or format-converting pipeline
        // Convert input samples to normalized stereo float [-1.0f, 1.0f)
        stereo_float_buf.resize(samples_per_ch * 2);
        const float scale = 1.0f / static_cast<float>(1LL << (bits - 1));
        for (uint32_t i = 0; i < samples_per_ch; ++i) {
            float l = static_cast<float>(samples[0][i]) * scale;
            float r = (channels >= 2) ? (static_cast<float>(samples[1][i]) * scale) : l;
            stereo_float_buf[i * 2]     = l;
            stereo_float_buf[i * 2 + 1] = r;
        }

        const float* float_data = stereo_float_buf.data();
        size_t effective_frames = samples_per_ch;

        if (m_actual_mode == HP_SHARED && m_resampler.NeedsResampling()) {
            size_t max_out = resample_scratch.size() / 2;
            effective_frames = m_resampler.Process(
                stereo_float_buf.data(),
                samples_per_ch,
                resample_scratch.data(),
                max_out
            );
            float_data = resample_scratch.data();
        }

        // Map and pack effective_frames into target_channels and target sample representation
        const size_t total_out_bytes = effective_frames * target_block_align;
        if (out_scratch.size() < total_out_bytes) {
            out_scratch.resize(total_out_bytes);
        }
        uint8_t* dst = out_scratch.data();

        if (is_output_float && target_channels == 2) {
            // Fast-path: standard stereo float32
            std::memcpy(dst, float_data, total_out_bytes);
        } else if (is_output_float) {
            // Multi-channel or mono float32
            for (size_t f = 0; f < effective_frames; ++f) {
                float fl = float_data[f * 2];
                float fr = float_data[f * 2 + 1];
                uint8_t* frame_dst = dst + f * target_block_align;
                if (target_channels == 1) {
                    float mono = 0.5f * (fl + fr);
                    std::memcpy(frame_dst, &mono, 4);
                } else {
                    std::memcpy(frame_dst, &fl, 4);
                    std::memcpy(frame_dst + 4, &fr, 4);
                    for (uint32_t ch = 2; ch < target_channels; ++ch) {
                        float zero = 0.0f;
                        std::memcpy(frame_dst + ch * 4, &zero, 4);
                    }
                }
            }
        } else {
            // Integer PCM target (16-bit, 24-bit packed, or 24-bit in 32-bit container)
            const uint32_t bytes_per_sample = target_container_bits / 8;
            const double max_val = static_cast<double>((1LL << (target_valid_bits - 1)) - 1);
            for (size_t f = 0; f < effective_frames; ++f) {
                float fl = float_data[f * 2];
                float fr = float_data[f * 2 + 1];
                uint8_t* frame_dst = dst + f * target_block_align;
                if (target_channels == 1) {
                    float mono = 0.5f * (fl + fr);
                    int32_t s = static_cast<int32_t>(std::clamp(static_cast<double>(mono) * max_val, -max_val - 1.0, max_val));
                    pack_sample(frame_dst, s, pack_params);
                } else {
                    int32_t sl = static_cast<int32_t>(std::clamp(static_cast<double>(fl) * max_val, -max_val - 1.0, max_val));
                    pack_sample(frame_dst, sl, pack_params);

                    int32_t sr = static_cast<int32_t>(std::clamp(static_cast<double>(fr) * max_val, -max_val - 1.0, max_val));
                    pack_sample(frame_dst + bytes_per_sample, sr, pack_params);

                    for (uint32_t ch = 2; ch < target_channels; ++ch) {
                        pack_sample(frame_dst + ch * bytes_per_sample, 0, pack_params);
                    }
                }
            }
        }

        size_t written = 0;
        while (written < total_out_bytes && !m_decode_stop.load()) {
            if (m_current_gen != m_ring_buffer.get_write_gen()) break;
            size_t w = m_ring_buffer.write(dst + written, total_out_bytes - written, m_current_gen);
            written += w;
            if (written < total_out_bytes) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
    });

    std::string err;
    bool decode_ok = m_decoder.DecodeAll(err);

    // Wait until the audio renderer has drained all buffered PCM to the hardware DAC
    while (!m_decode_stop.load() && m_ring_buffer.available_read() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (!m_decode_stop.load()) {
        if (!decode_ok || m_decoder.HasError()) {
            m_state.store(HP_STATE_STOPPED);
            FirePlaybackEvent(HP_EVT_ERROR, HP_STATE_STOPPED, -1, 0);
        } else {
            // Allow hardware DMA buffer (typically 10-50 ms) to finish sounding
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            m_state.store(HP_STATE_STOPPED);
            FirePlaybackEvent(HP_EVT_TRACK_END, HP_STATE_STOPPED, 0, meta.total_samples);
        }
    }
}

void PlaybackController::GetStatus(HP_StatusV1* out) {
    if (!out) return;
    const auto& meta = m_decoder.metadata();
    const auto actual_wfex = m_renderer.GetActualFormat();

    const bool dsp_disabled = m_dsp_graph.IsDspDisabled();

    uint64_t rendered = m_renderer.GetRenderedSamples();
    if (meta.total_samples > 0 && rendered > meta.total_samples) {
        rendered = meta.total_samples;
    }

    // Derive bit-perfect eligibility and PCM passthrough status from active processing state
    const bool rate_match = (meta.sample_rate == actual_wfex.Format.nSamplesPerSec) && actual_wfex.Format.nSamplesPerSec != 0;
    const bool bits_match = (meta.bits_per_sample == actual_wfex.Samples.wValidBitsPerSample) && actual_wfex.Samples.wValidBitsPerSample != 0;
    const bool ch_match = (meta.channels == actual_wfex.Format.nChannels) && actual_wfex.Format.nChannels != 0;
    const bool decoder_no_errors = !m_decoder.HasError();
    const bool exclusive_active = (m_state.load() == HP_STATE_STOPPED) ? (m_requested_mode == HP_EXCLUSIVE) : m_renderer.IsExclusive();
    const bool pcm_pass = exclusive_active && dsp_disabled && !m_resampler.NeedsResampling() && decoder_no_errors;

    m_diagnostics.Populate(
        *out,
        m_state.load(),
        meta.sample_rate,
        meta.bits_per_sample,
        meta.channels,
        meta.total_samples,
        actual_wfex.Format.nSamplesPerSec,
        actual_wfex.Samples.wValidBitsPerSample,
        actual_wfex.Format.wBitsPerSample,
        actual_wfex.Format.nChannels,
        m_actual_mode,
        decoder_no_errors,
        exclusive_active,
        dsp_disabled,
        pcm_pass,
        rendered,
        m_renderer.GetUnderrunCount(),
        m_renderer.GetLastHResult(),
        m_selected_device_id,
        m_selected_device_name
    );
}

std::wstring PlaybackController::GetStatusMessage() {
    HP_StatusV1 st;
    GetStatus(&st);
    return AudioDiagnostics::StatusMessageForId(st.status_message_id);
}

void PlaybackController::UpdateState(HP_State new_state) {
    m_state.store(new_state);
    FirePlaybackEvent(HP_EVT_STATE_CHANGED, new_state, 0, m_renderer.GetRenderedSamples());
}

void PlaybackController::FirePlaybackEvent(uint32_t evt, uint32_t state, int32_t hr, uint64_t pos) {
    if (m_play_cb) {
        m_play_cb(m_play_user_data, evt, state, hr, pos);
    }
}

HP_Result PlaybackController::GetBluetoothDeviceInfo(const std::wstring& endpoint_id, HP_BluetoothDeviceInfoV1* out_info) {
    if (!out_info) return HP_INVALID_ARGUMENT;
    std::wstring dev_id = endpoint_id.empty() ? m_selected_device_id : endpoint_id;
    if (dev_id.empty() && m_device_mgr) {
        dev_id = m_device_mgr->GetDefaultDeviceId();
    }
    if (!m_device_mgr) return HP_DEVICE_UNAVAILABLE;

    bool ok = m_device_mgr->GetBluetoothDeviceInfo(dev_id, out_info);
    return ok ? HP_OK : HP_DEVICE_UNAVAILABLE;
}

HP_Result PlaybackController::SetBluetoothFidelityMode(bool enabled) {
    m_bluetooth_fidelity_mode.store(enabled);
    return HP_OK;
}

HP_Result PlaybackController::GetBluetoothFidelityStatus(HP_BluetoothFidelityStatusV1* out) {
    if (!out) return HP_INVALID_ARGUMENT;
    std::memset(out, 0, sizeof(HP_BluetoothFidelityStatusV1));
    out->structure_size = sizeof(HP_BluetoothFidelityStatusV1);

    std::wstring dev_id = m_selected_device_id;
    if (dev_id.empty() && m_device_mgr) {
        dev_id = m_device_mgr->GetDefaultDeviceId();
    }

    HP_BluetoothDeviceInfoV1 dev_info = {};
    if (m_device_mgr) {
        m_device_mgr->GetBluetoothDeviceInfo(dev_id, &dev_info);
    }

    const auto& meta = m_decoder.metadata();
    const auto actual_wfex = m_renderer.GetActualFormat();
    const bool is_bt = (dev_info.is_bluetooth == 1);
    const bool fidelity_enabled = m_bluetooth_fidelity_mode.load();

    out->fidelity_mode_enabled = fidelity_enabled ? 1 : 0;
    out->active_codec = dev_info.detected_codec;
    out->codec_confidence = dev_info.codec_confidence;
    out->is_hfp_warning = (dev_info.bt_profile == HP_BT_PROFILE_HFP) ? 1 : 0;
    out->underrun_count = m_renderer.GetUnderrunCount();

    if (dev_info.detected_codec == HP_BT_CODEC_LDAC) {
        out->estimated_bitrate_kbps = 990;
    } else if (dev_info.detected_codec == HP_BT_CODEC_APTX_HD) {
        out->estimated_bitrate_kbps = 576;
    } else if (dev_info.detected_codec == HP_BT_CODEC_APTX) {
        out->estimated_bitrate_kbps = 352;
    } else if (dev_info.detected_codec == HP_BT_CODEC_AAC) {
        out->estimated_bitrate_kbps = 256;
    } else if (dev_info.detected_codec == HP_BT_CODEC_SBC) {
        out->estimated_bitrate_kbps = 328;
    } else {
        out->estimated_bitrate_kbps = 0;
    }

    const bool dsp_disabled = m_dsp_graph.IsDspDisabled();
    out->pcm_transparent = dsp_disabled ? 1 : 0;

    uint32_t match_state = 1;
    if (actual_wfex.Format.nSamplesPerSec > 0 && meta.sample_rate > 0 &&
        actual_wfex.Format.nSamplesPerSec != meta.sample_rate) {
        match_state = 2;
    }
    out->output_format_matched = match_state;

    if (is_bt) {
        out->wireless_lossless_state = 0;
    } else {
        out->wireless_lossless_state = (out->pcm_transparent == 1 && match_state == 1 && m_renderer.IsExclusive()) ? 1 : 0;
    }

    BluetoothDeviceService::BuildFidelitySummary(
        dev_info,
        meta.sample_rate,
        meta.bits_per_sample,
        out->pcm_transparent == 1,
        match_state,
        out->status_summary,
        sizeof(out->status_summary) / sizeof(wchar_t)
    );

    return HP_OK;
}
