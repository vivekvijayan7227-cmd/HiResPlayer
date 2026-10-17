#include "PlaybackController.h"
#include "BluetoothDeviceService.h"
#include "PcmSamplePacker.h"
#include <iostream>
#include <chrono>
#include <xmmintrin.h>

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

    // Real-time DSP & Spectrum callback: processes the audio block directly in the WASAPI render thread (< 10 ms delay!)
    // Real-time DSP & Spectrum callback: processes the audio block directly in the WASAPI render thread (< 10 ms delay!)
    m_renderer.SetDspCallback([this](uint8_t* p_data, UINT32 frames, const WAVEFORMATEXTENSIBLE& wfex) {
        if (!p_data || frames == 0) return;

        const bool dsp_active = m_dsp_graph.IsDspActive();
        const uint32_t channels = wfex.Format.nChannels;
        if (channels == 0) return;

        thread_local std::vector<float> left_buf;
        thread_local std::vector<float> right_buf;
        if (left_buf.size() < frames) {
            left_buf.resize(frames);
            right_buf.resize(frames);
        }

        // Shared mode: 32-bit IEEE float
        if (wfex.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT ||
            wfex.Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
            (wfex.Format.wBitsPerSample == 32 && (wfex.Samples.wValidBitsPerSample == 32 || wfex.Samples.wValidBitsPerSample == 0))) {
            float* f = reinterpret_cast<float*>(p_data);
            if (channels == 1) {
                for (UINT32 i = 0; i < frames; ++i) {
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
                for (UINT32 i = 0; i < frames; ++i) {
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(f[i * 2], f[i * 2 + 1]);
                    }
                    left_buf[i]  = f[i * 2];
                    right_buf[i] = f[i * 2 + 1];
                }
            } else {
                for (UINT32 i = 0; i < frames; ++i) {
                    const size_t base = i * channels;
                    if (dsp_active) {
                        m_dsp_graph.ProcessFrame(f[base], f[base + 1]);
                    }
                    left_buf[i]  = f[base];
                    right_buf[i] = f[base + 1];
                }
            }
        }
        // Exclusive mode: 24-in-32 container
        else if (wfex.Format.wBitsPerSample == 32 && wfex.Samples.wValidBitsPerSample == 24) {
            int32_t* s = reinterpret_cast<int32_t*>(p_data);
            if (channels == 1) {
                for (UINT32 i = 0; i < frames; ++i) {
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
                for (UINT32 i = 0; i < frames; ++i) {
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
                for (UINT32 i = 0; i < frames; ++i) {
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
        // Exclusive mode: 16-in-32 container
        else if (wfex.Format.wBitsPerSample == 32 && wfex.Samples.wValidBitsPerSample == 16) {
            int32_t* s = reinterpret_cast<int32_t*>(p_data);
            if (channels == 1) {
                for (UINT32 i = 0; i < frames; ++i) {
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
                for (UINT32 i = 0; i < frames; ++i) {
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
                for (UINT32 i = 0; i < frames; ++i) {
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
        // Exclusive mode: 24-bit packed (3 bytes per sample)
        else if (wfex.Format.wBitsPerSample == 24) {
            const size_t frame_bytes = channels * 3;
            for (UINT32 i = 0; i < frames; ++i) {
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
        // Exclusive mode: 16-bit packed
        else if (wfex.Format.wBitsPerSample == 16) {
            int16_t* s = reinterpret_cast<int16_t*>(p_data);
            if (channels == 1) {
                for (UINT32 i = 0; i < frames; ++i) {
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
                for (UINT32 i = 0; i < frames; ++i) {
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
                for (UINT32 i = 0; i < frames; ++i) {
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

        m_spectrum_analyzer.FeedStereoSamples(left_buf.data(), right_buf.data(), frames);
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
    if (!m_renderer.Start(cfg, err)) {
        // Fallback: If exclusive mode was requested and failed, try shared mode
        if (m_requested_mode == HP_EXCLUSIVE) {
            cfg.mode = HP_SHARED;
            m_renderer.Stop();
            if (!m_renderer.Start(cfg, err)) {
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

    // Start decoding thread
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

    m_decode_thread = std::thread([this]() {
        this->DecodeThreadProc();
    });

    // Brief pre-roll buffer fill to guarantee hardware stream never underruns on startup
    for (int i = 0; i < 25; ++i) {
        if (m_ring_buffer.available_read() > 16384) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
    m_decode_stop.store(true);
    m_decoder.RequestStop();

    if (m_decode_thread.joinable()) {
        m_decode_thread.join();
    }
    m_decoder.SetPcmCallback(nullptr);

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
    m_decode_stop.store(true);
    m_decoder.RequestStop();
    if (m_decode_thread.joinable()) {
        m_decode_thread.join();
    }
    m_decoder.SetPcmCallback(nullptr);

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

void PlaybackController::DecodeThreadProc() {
    _mm_setcsr(_mm_getcsr() | 0x8040); // FTZ & DAZ for zero CPU denormal traps

    struct CallbackCleanup {
        FlacDecoder& d;
        ~CallbackCleanup() { d.SetPcmCallback(nullptr); }
    } cleanup{m_decoder};

    const auto& meta = m_decoder.metadata();
    const auto actual_wfex = m_renderer.GetActualFormat();
    PackParams pack_params;
    pack_params.valid_bits = meta.bits_per_sample;
    pack_params.output_float = (m_actual_mode == HP_SHARED);
    if (m_actual_mode == HP_SHARED) {
        pack_params.container_bits = 32;
    } else {
        pack_params.container_bits = (actual_wfex.Format.wBitsPerSample > 0)
            ? actual_wfex.Format.wBitsPerSample
            : ((meta.bits_per_sample == 24) ? 32 : meta.bits_per_sample);
    }

    const size_t bytes_per_sample = pack_params.output_float ? 4 : (pack_params.container_bits / 8);
    const size_t bytes_per_frame = meta.channels * bytes_per_sample;

    std::vector<uint8_t> pcm_scratch(65536 * bytes_per_frame);
    std::vector<float> resample_scratch;
    if (m_actual_mode == HP_SHARED && m_resampler.NeedsResampling()) {
        resample_scratch.resize(65536 * 4);
    }

    m_decoder.SetPcmCallback([&](const int32_t* const* samples, uint32_t channels, uint32_t samples_per_ch, uint32_t bits) {
        if (m_decode_stop.load()) return;

        uint32_t active_channels = channels;
        if (m_actual_mode == HP_SHARED && channels == 1) {
            const int32_t* mono_ptrs[2] = { samples[0], samples[0] };
            pack_frames(pcm_scratch.data(), mono_ptrs, 2, samples_per_ch, pack_params);
            active_channels = 2;
        } else {
            pack_frames(pcm_scratch.data(), samples, channels, samples_per_ch, pack_params);
        }

        const uint8_t* write_ptr = pcm_scratch.data();
        size_t total_bytes = samples_per_ch * active_channels * bytes_per_sample;

        if (m_actual_mode == HP_SHARED && m_resampler.NeedsResampling() && pack_params.output_float && active_channels == 2) {
            size_t max_out_frames = resample_scratch.size() / 2;
            size_t out_frames = m_resampler.Process(
                reinterpret_cast<const float*>(pcm_scratch.data()),
                samples_per_ch,
                resample_scratch.data(),
                max_out_frames
            );
            write_ptr = reinterpret_cast<const uint8_t*>(resample_scratch.data());
            total_bytes = out_frames * 2 * sizeof(float);
        }

        size_t written = 0;
        while (written < total_bytes && !m_decode_stop.load()) {
            if (m_current_gen != m_ring_buffer.get_write_gen()) break;
            size_t w = m_ring_buffer.write(write_ptr + written, total_bytes - written, m_current_gen);
            written += w;
            if (written < total_bytes) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
    });

    std::string err;
    m_decoder.DecodeAll(err);

    // Wait until the audio renderer has drained all buffered PCM to the hardware DAC
    while (!m_decode_stop.load() && m_ring_buffer.available_read() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (!m_decode_stop.load()) {
        // Allow hardware DMA buffer (typically 10-50 ms) to finish sounding
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        m_state.store(HP_STATE_STOPPED);
        FirePlaybackEvent(HP_EVT_TRACK_END, HP_STATE_STOPPED, 0, meta.total_samples);
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
        !m_decoder.HasError(),
        m_renderer.IsExclusive(),
        dsp_disabled,
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
