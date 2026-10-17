#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <cstdint>
#include <functional>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include "AudioFormatNegotiator.h"
#include "PcmRingBuffer.h"
#include "../include/hires_audio_api.h"

struct RendererConfig {
    std::wstring   endpoint_id;
    HP_PlaybackMode mode          = HP_SHARED;
    uint32_t       source_rate   = 0;
    uint32_t       source_bits   = 0;   // valid bits
    uint32_t       source_channels = 0;
    // Shared mode: ring buffer contains float32 interleaved at mix rate
    // Exclusive mode: ring buffer contains packed PCM at source rate
    PcmRingBuffer* ring_buffer   = nullptr; // not owned
};

using RendererEventCb = std::function<void(uint32_t event_type, int32_t hresult, uint64_t position)>;

// WASAPI renderer — shared and exclusive mode.
// Render thread runs with MMCSS priority.
class WasapiRenderer {
public:
    WasapiRenderer();
    ~WasapiRenderer();

    // Decoupled preparation: probes/initializes audio client, negotiates format, gets buffer client
    bool Prepare(const RendererConfig& cfg, std::wstring& error_out);

    // Starts audio stream DMA rendering after prebuffering is complete
    bool StartStream(std::wstring& error_out);

    // Initialize and start rendering (calls Prepare + StartStream). Returns false on failure.
    bool Start(const RendererConfig& cfg, std::wstring& error_out);

    // Stop render thread and release WASAPI resources.
    void Stop();

    using DspRenderCallback = std::function<void(uint8_t* pcm_data, UINT32 frames, const WAVEFORMATEXTENSIBLE& wfex)>;
    void SetDspCallback(DspRenderCallback cb) { m_dsp_cb = std::move(cb); }

    // Pause/resume with clean WASAPI hardware engine Stop/Start
    void SetPaused(bool paused);
    bool IsPaused() const       { return m_paused.load(std::memory_order_acquire); }

    bool IsRunning() const { return m_running.load(std::memory_order_acquire); }
    bool IsPrepared() const { return m_is_prepared; }

    // Current render position in source samples (per channel)
    uint64_t GetRenderedSamples() const {
        return m_rendered_samples.load(std::memory_order_acquire);
    }

    uint64_t GetUnderrunCount() const {
        return m_underruns.load(std::memory_order_acquire);
    }

    uint64_t GetFullUnderrunCount() const {
        return m_full_underruns.load(std::memory_order_acquire);
    }

    uint64_t GetPartialUnderrunCount() const {
        return m_partial_underruns.load(std::memory_order_acquire);
    }

    uint64_t GetUnderrunFrames() const {
        return m_underrun_frames.load(std::memory_order_acquire);
    }

    int32_t GetLastHResult() const {
        return m_last_hresult.load(std::memory_order_acquire);
    }

    // Actual negotiated output format
    WAVEFORMATEXTENSIBLE GetActualFormat() const { return m_actual_wfex; }
    uint32_t GetOutputRate() const { return m_output_rate; }
    uint32_t GetBytesPerFrame() const { return m_bytes_per_frame; }
    bool IsExclusive() const { return m_is_exclusive; }

    void SetEventCallback(RendererEventCb cb) { m_event_cb = std::move(cb); }
    void SetDevice(IMMDevice* device);
    void SetBufferGeneration(uint64_t gen) { m_buffer_gen.store(gen, std::memory_order_release); }
    uint64_t GetBufferGeneration() const { return m_buffer_gen.load(std::memory_order_acquire); }
    void SetRenderedSamples(uint64_t pos) { m_rendered_samples.store(pos, std::memory_order_release); }
    void Wakeup();
    void Flush();

private:
    void RenderThreadProc();
    bool InitializeShared(IMMDevice* device, const RendererConfig& cfg,
                          UINT32& out_buffer_frames, HANDLE& out_event);
    bool InitializeExclusive(IMMDevice* device, const RendererConfig& cfg,
                             UINT32& out_buffer_frames, HANDLE& out_event);

    // Fill WASAPI buffer with PCM from ring buffer.
    // Returns bytes consumed from ring buffer, or 0 on underrun.
    bool FillBuffer(IAudioRenderClient* render_client,
                    UINT32 frames_available,
                    uint64_t& gen);

    IMMDevice*          m_device        = nullptr;
    IAudioClient*       m_audio_client  = nullptr;
    IAudioRenderClient* m_render_client = nullptr;
    HANDLE              m_buffer_event  = nullptr;
    std::thread         m_render_thread;
    std::atomic<bool>   m_running  {false};
    std::atomic<bool>   m_paused   {false};
    std::atomic<bool>   m_stop_req {false};
    std::atomic<uint64_t> m_rendered_samples {0};
    std::atomic<uint64_t> m_underruns        {0};
    std::atomic<uint64_t> m_full_underruns   {0};
    std::atomic<uint64_t> m_partial_underruns{0};
    std::atomic<uint64_t> m_underrun_frames  {0};
    std::atomic<int32_t>  m_last_hresult     {0};
    std::atomic<bool>     m_is_prepared      {false};
    UINT32                m_prepared_buffer_frames = 0;

    std::mutex           m_render_mutex;
    std::atomic<bool>    m_flushing { false };
    uint32_t             m_source_rate = 0;
    uint64_t             m_source_sample_accum = 0;

    WAVEFORMATEXTENSIBLE m_actual_wfex = {};
    bool                 m_is_exclusive = false;
    uint32_t             m_bytes_per_frame = 0;
    uint32_t             m_output_rate = 0;
    std::atomic<uint64_t> m_buffer_gen { 0 };
    PcmRingBuffer*       m_ring = nullptr; // not owned
    RendererEventCb      m_event_cb;
    DspRenderCallback    m_dsp_cb;
    std::wstring         m_last_error;
};
