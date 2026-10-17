#include "WasapiRenderer.h"
#include <avrt.h>
#include <xmmintrin.h>
#include <cstring>
#include <cassert>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "avrt.lib")

// AUDCLNT_STREAMFLAGS_EVENTCALLBACK
#ifndef AUDCLNT_STREAMFLAGS_EVENTCALLBACK
#define AUDCLNT_STREAMFLAGS_EVENTCALLBACK 0x00040000
#endif

WasapiRenderer::WasapiRenderer() {}

WasapiRenderer::~WasapiRenderer() {
    if (m_running.load()) {
        Stop();
    }
    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }
}

bool WasapiRenderer::Start(const RendererConfig& cfg, std::wstring& error_out) {
    if (m_running.load()) { Stop(); }
    if (!cfg.ring_buffer) { error_out = L"No ring buffer provided"; return false; }

    m_ring = cfg.ring_buffer;
    m_is_exclusive = (cfg.mode == HP_EXCLUSIVE);
    m_source_rate = cfg.source_rate;
    m_source_sample_accum = 0;
    m_stop_req.store(false);
    m_paused.store(false);
    m_flushing.store(false);
    m_rendered_samples.store(0);
    m_underruns.store(0);
    m_last_hresult.store(0);
    m_buffer_gen.store(m_ring ? m_ring->get_read_gen() : 0, std::memory_order_release);

    if (!m_device) { error_out = L"IMMDevice not set"; return false; }

    UINT32 buffer_frames = 0;
    m_buffer_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_buffer_event) { error_out = L"CreateEvent failed"; return false; }

    bool ok;
    if (m_is_exclusive) {
        ok = InitializeExclusive(m_device, cfg, buffer_frames, m_buffer_event);
    } else {
        ok = InitializeShared(m_device, cfg, buffer_frames, m_buffer_event);
    }
    if (!ok) {
        error_out = m_last_error;
        CloseHandle(m_buffer_event); m_buffer_event = nullptr;
        return false;
    }

    HRESULT hr = m_audio_client->GetService(
        __uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(&m_render_client)
    );
    if (FAILED(hr)) {
        error_out = L"GetService(IAudioRenderClient) failed";
        m_last_hresult.store((int32_t)hr);
        Stop();
        return false;
    }

    // Pre-roll WASAPI buffer with silence before starting hardware DMA
    if (buffer_frames > 0) {
        BYTE* p_pre = nullptr;
        hr = m_render_client->GetBuffer(buffer_frames, &p_pre);
        if (SUCCEEDED(hr) && p_pre) {
            std::memset(p_pre, 0, (size_t)buffer_frames * m_bytes_per_frame);
            m_render_client->ReleaseBuffer(buffer_frames, 0);
        }
    }

    hr = m_audio_client->Start();
    if (FAILED(hr)) {
        error_out = L"IAudioClient::Start failed";
        m_last_hresult.store((int32_t)hr);
        Stop();
        return false;
    }

    m_running.store(true);
    m_render_thread = std::thread([this, buffer_frames]() {
        this->RenderThreadProc();
    });

    return true;
}

void WasapiRenderer::Stop() {
    m_stop_req.store(true);
    HANDLE hEvt = m_buffer_event;
    if (hEvt && hEvt != INVALID_HANDLE_VALUE) {
        SetEvent(hEvt);
    }
    if (m_render_thread.joinable()) {
        m_render_thread.join();
    }

    std::lock_guard<std::mutex> lk(m_render_mutex);
    if (m_audio_client) {
        if (m_running.load()) {
            m_audio_client->Stop();
        }
        m_audio_client->Release();
        m_audio_client = nullptr;
    }
    if (m_render_client) {
        m_render_client->Release();
        m_render_client = nullptr;
    }
    if (hEvt && hEvt != INVALID_HANDLE_VALUE) {
        CloseHandle(hEvt);
        m_buffer_event = nullptr;
    }
    m_running.store(false);
}

bool WasapiRenderer::InitializeShared(
    IMMDevice* device, const RendererConfig& cfg,
    UINT32& out_buffer_frames, HANDLE& event_handle
) {
    if (!device) {
        m_last_error = L"No audio device provided for Shared mode";
        return false;
    }

    if (m_audio_client) {
        m_audio_client->Release();
        m_audio_client = nullptr;
    }

    HRESULT hr = device->Activate(
        __uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr,
        reinterpret_cast<void**>(&m_audio_client)
    );
    if (FAILED(hr) || !m_audio_client) {
        m_last_error = L"Activate IAudioClient (shared) failed";
        m_last_hresult.store((int32_t)hr);
        return false;
    }

    // Get mix format
    WAVEFORMATEX* pMixFmt = nullptr;
    hr = m_audio_client->GetMixFormat(&pMixFmt);
    if (FAILED(hr) || !pMixFmt) {
        m_last_error = L"GetMixFormat failed";
        m_last_hresult.store((int32_t)hr);
        return false;
    }

    // Check if standard stereo float32 is supported at the device's mix sample rate
    WAVEFORMATEXTENSIBLE stereo_wfex = AudioFormatNegotiator::BuildSharedFloat32Format(pMixFmt->nSamplesPerSec, 2);
    WAVEFORMATEX* pFormatToUse = pMixFmt;
    HRESULT hr_stereo = m_audio_client->IsFormatSupported(
        AUDCLNT_SHAREMODE_SHARED,
        reinterpret_cast<WAVEFORMATEX*>(&stereo_wfex),
        nullptr
    );
    if (SUCCEEDED(hr_stereo)) {
        pFormatToUse = reinterpret_cast<WAVEFORMATEX*>(&stereo_wfex);
    }

    // Record actual output format
    if (pFormatToUse->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        m_actual_wfex = *reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pFormatToUse);
    } else {
        std::memset(&m_actual_wfex, 0, sizeof(m_actual_wfex));
        m_actual_wfex.Format = *pFormatToUse;
    }
    m_output_rate = pFormatToUse->nSamplesPerSec;
    m_bytes_per_frame = pFormatToUse->nBlockAlign;

    // Initialize with ~200ms buffer
    REFERENCE_TIME requested = 2000000; // 200ms in 100-ns units
    hr = m_audio_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        requested, 0,
        pFormatToUse,
        nullptr
    );
    CoTaskMemFree(pMixFmt);

    if (FAILED(hr)) {
        m_last_error = L"IAudioClient::Initialize (shared) failed";
        m_last_hresult.store((int32_t)hr);
        m_audio_client->Release();
        m_audio_client = nullptr;
        return false;
    }

    hr = m_audio_client->SetEventHandle(event_handle);
    if (FAILED(hr)) {
        m_last_error = L"SetEventHandle failed";
        m_last_hresult.store((int32_t)hr);
        m_audio_client->Release();
        m_audio_client = nullptr;
        return false;
    }

    m_audio_client->GetBufferSize(&out_buffer_frames);
    return true;
}

bool WasapiRenderer::InitializeExclusive(
    IMMDevice* device, const RendererConfig& cfg,
    UINT32& out_buffer_frames, HANDLE& event_handle
) {
    if (!device) {
        m_last_error = L"No audio device provided for Exclusive mode";
        return false;
    }

    // Probe exclusive format using a temporary client
    IAudioClient* probe_client = nullptr;
    HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void**>(&probe_client));
    if (FAILED(hr) || !probe_client) {
        m_last_error = L"Failed to activate probe client";
        m_last_hresult.store((int32_t)hr);
        return false;
    }

    auto result = AudioFormatNegotiator::ProbeExclusive(
        probe_client, cfg.source_rate, cfg.source_bits, cfg.source_channels
    );
    probe_client->Release();
    probe_client = nullptr;

    if (!result.success) {
        m_last_error  = L"Exclusive format not supported: " + result.reason;
        m_last_hresult.store(result.hresult);
        return false;
    }

    m_actual_wfex  = result.wfex;
    m_output_rate  = result.wfex.Format.nSamplesPerSec;
    m_bytes_per_frame = result.wfex.Format.nBlockAlign;

    if (m_audio_client) {
        m_audio_client->Release();
        m_audio_client = nullptr;
    }

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void**>(&m_audio_client));
    if (FAILED(hr) || !m_audio_client) {
        m_last_error = L"Activate IAudioClient failed";
        m_last_hresult.store((int32_t)hr);
        return false;
    }

    // Determine hardware device period
    REFERENCE_TIME defPeriod = 0, minPeriod = 0;
    m_audio_client->GetDevicePeriod(&defPeriod, &minPeriod);
    REFERENCE_TIME requested = (defPeriod > 0) ? defPeriod : minPeriod;
    if (requested == 0) requested = 100000; // 10ms fallback

    hr = m_audio_client->Initialize(
        AUDCLNT_SHAREMODE_EXCLUSIVE,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        requested, requested,
        reinterpret_cast<WAVEFORMATEX*>(&m_actual_wfex),
        nullptr
    );

    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        UINT32 aligned_frames = 0;
        m_audio_client->GetBufferSize(&aligned_frames);
        REFERENCE_TIME aligned_period = (REFERENCE_TIME)(
            10000000.0 * aligned_frames / m_output_rate + 0.5
        );

        // Microsoft requirement: must release and re-activate IAudioClient before re-initializing
        m_audio_client->Release();
        m_audio_client = nullptr;

        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void**>(&m_audio_client));
        if (FAILED(hr) || !m_audio_client) {
            m_last_error = L"Re-activate IAudioClient failed";
            m_last_hresult.store((int32_t)hr);
            return false;
        }

        hr = m_audio_client->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            aligned_period, aligned_period,
            reinterpret_cast<WAVEFORMATEX*>(&m_actual_wfex),
            nullptr
        );
    }

    if (FAILED(hr)) {
        m_last_error  = L"IAudioClient::Initialize (exclusive) failed";
        m_last_hresult.store((int32_t)hr);
        m_audio_client->Release();
        m_audio_client = nullptr;
        return false;
    }

    hr = m_audio_client->SetEventHandle(event_handle);
    if (FAILED(hr)) {
        m_last_error = L"SetEventHandle failed";
        m_last_hresult.store((int32_t)hr);
        m_audio_client->Release();
        m_audio_client = nullptr;
        return false;
    }

    m_audio_client->GetBufferSize(&out_buffer_frames);
    return true;
}

void WasapiRenderer::RenderThreadProc() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    _mm_setcsr(_mm_getcsr() | 0x8040); // FTZ & DAZ for zero CPU denormal traps
    // Set MMCSS audio priority
    DWORD task_index = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Audio", &task_index);

    constexpr DWORD kWaitTimeoutMs = 2000;

    while (!m_stop_req.load(std::memory_order_relaxed)) {
        DWORD wait_result = WaitForSingleObject(m_buffer_event, kWaitTimeoutMs);
        if (m_stop_req.load(std::memory_order_relaxed)) break;

        if (wait_result == WAIT_TIMEOUT) {
            if (!m_paused.load(std::memory_order_acquire)) {
                m_underruns.fetch_add(1, std::memory_order_relaxed);
            }
            continue;
        }
        if (wait_result != WAIT_OBJECT_0) break;
        if (m_paused.load(std::memory_order_acquire) || m_flushing.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        std::lock_guard<std::mutex> lk(m_render_mutex);
        if (m_stop_req.load(std::memory_order_relaxed) || m_paused.load(std::memory_order_acquire) || m_flushing.load(std::memory_order_acquire)) {
            continue;
        }

        // How many frames can we write?
        UINT32 padding = 0;
        HRESULT hr = S_OK;
        if (!m_is_exclusive) {
            hr = m_audio_client->GetCurrentPadding(&padding);
            if (FAILED(hr)) {
                if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == (HRESULT)0x88890004) {
                    m_last_hresult.store((int32_t)hr);
                    break;
                }
                continue;
            }
        }

        UINT32 buf_size = 0;
        hr = m_audio_client->GetBufferSize(&buf_size);
        if (FAILED(hr)) continue;

        UINT32 frames_avail = m_is_exclusive ? buf_size : (buf_size - padding);
        if (frames_avail == 0) continue;

        BYTE* p_data = nullptr;
        hr = m_render_client->GetBuffer(frames_avail, &p_data);
        if (FAILED(hr)) {
            if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == (HRESULT)0x88890004) {
                m_last_hresult.store((int32_t)hr);
                if (m_event_cb) m_event_cb(HP_EVT_ERROR, (int32_t)hr, m_rendered_samples.load());
                break;
            }
            continue;
        }

        size_t bytes_needed = (size_t)frames_avail * m_bytes_per_frame;
        uint64_t gen = m_buffer_gen.load(std::memory_order_acquire);
        size_t read = m_ring->read(p_data, bytes_needed, gen);

        DWORD flags = 0;
        if (read == 0) {
            flags = AUDCLNT_BUFFERFLAGS_SILENT;
            m_underruns.fetch_add(1, std::memory_order_relaxed);
            if (m_event_cb) m_event_cb(HP_EVT_UNDERRUN, 0, m_rendered_samples.load());
        } else if (m_dsp_cb) {
            // Apply DSP directly to the block being sent to WASAPI hardware (< 10 ms delay!)
            m_dsp_cb(p_data, frames_avail, m_actual_wfex);
        }

        m_render_client->ReleaseBuffer(frames_avail, flags);

        if (read > 0) {
            const UINT32 consumed_frames = static_cast<UINT32>(read / m_bytes_per_frame);
            if (m_source_rate > 0 && m_output_rate > 0) {
                m_source_sample_accum += static_cast<uint64_t>(consumed_frames) * m_source_rate;
                uint64_t adv = m_source_sample_accum / m_output_rate;
                m_source_sample_accum %= m_output_rate;
                m_rendered_samples.fetch_add(adv, std::memory_order_release);
            } else {
                m_rendered_samples.fetch_add(consumed_frames, std::memory_order_release);
            }
        }
    }

    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
    CoUninitialize();
}

void WasapiRenderer::SetPaused(bool paused) {
    std::lock_guard<std::mutex> lk(m_render_mutex);
    bool was_paused = m_paused.load(std::memory_order_acquire);
    if (was_paused == paused) return;

    if (paused) {
        // Pausing: mark paused first, stop WASAPI DMA, and reset/flush pending hardware frames
        m_paused.store(true, std::memory_order_release);
        if (m_audio_client) {
            m_audio_client->Stop();
            m_audio_client->Reset();
        }
    } else {
        // Resuming: pre-roll silence into hardware buffer so DAC ramps cleanly without buzz
        if (m_audio_client && m_render_client) {
            UINT32 buf_size = 0;
            if (SUCCEEDED(m_audio_client->GetBufferSize(&buf_size)) && buf_size > 0) {
                UINT32 padding = 0;
                if (!m_is_exclusive) {
                    m_audio_client->GetCurrentPadding(&padding);
                }
                UINT32 frames_avail = m_is_exclusive ? buf_size : (buf_size - padding);
                if (frames_avail > 0) {
                    BYTE* p_pre = nullptr;
                    if (SUCCEEDED(m_render_client->GetBuffer(frames_avail, &p_pre)) && p_pre) {
                        std::memset(p_pre, 0, (size_t)frames_avail * m_bytes_per_frame);
                        m_render_client->ReleaseBuffer(frames_avail, AUDCLNT_BUFFERFLAGS_SILENT);
                    }
                }
            }
            m_audio_client->Start();
        }
        m_paused.store(false, std::memory_order_release);
        if (m_buffer_event) {
            SetEvent(m_buffer_event);
        }
    }
}

void WasapiRenderer::Wakeup() {
    if (m_buffer_event) {
        SetEvent(m_buffer_event);
    }
}

void WasapiRenderer::Flush() {
    std::lock_guard<std::mutex> lk(m_render_mutex);
    m_flushing.store(true, std::memory_order_release);
    if (m_audio_client) {
        bool was_paused = m_paused.load(std::memory_order_acquire);
        if (!was_paused) {
            m_audio_client->Stop();
        }
        m_audio_client->Reset();
        if (!was_paused) {
            m_audio_client->Start();
        }
        if (m_buffer_event) {
            SetEvent(m_buffer_event);
        }
    }
    m_flushing.store(false, std::memory_order_release);
}

// ─── Device setup ────────────────────────────────────────────────────────────
void WasapiRenderer::SetDevice(IMMDevice* device) {
    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }
    m_device = device;
    if (m_device) {
        m_device->AddRef();
    }
}
