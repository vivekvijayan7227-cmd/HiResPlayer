#pragma once
#include <string>
#include <memory>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "hires_audio_api.h"
#include "hires_dsp_api.h"
#include "AudioDeviceManager.h"
#include "FlacDecoder.h"
#include "WasapiRenderer.h"
#include "PcmRingBuffer.h"
#include "AudioDiagnostics.h"
#include "DspGraph.h"
#include "AudioResampler.h"
#include "SpectrumAnalyzer.h"

class PlaybackController {
public:
    PlaybackController();
    ~PlaybackController();

    HP_Result OpenFlac(const std::wstring& path);
    HP_Result SelectDevice(const std::wstring& endpoint_id);
    HP_Result SetMode(HP_PlaybackMode mode);

    HP_Result Play();
    HP_Result Pause();
    HP_Result Stop();
    HP_Result Seek(uint64_t sample_pos);
    HP_Result SetVolume(float volume);
    HP_Result GetVolume(float* out_volume);

    void GetStatus(HP_StatusV1* out_status);
    std::wstring GetStatusMessage();

    // Bluetooth Fidelity Extensions
    HP_Result GetBluetoothDeviceInfo(const std::wstring& endpoint_id, HP_BluetoothDeviceInfoV1* out_info);
    HP_Result SetBluetoothFidelityMode(bool enabled);
    HP_Result GetBluetoothFidelityStatus(HP_BluetoothFidelityStatusV1* out_status);

    AudioDeviceManager& GetDeviceManager() { return *m_device_mgr; }
    DspGraph& GetDspGraph() { return m_dsp_graph; }
    void GetSpectrum(float* out_bands, uint32_t band_count) {
        m_spectrum_analyzer.GetBands(out_bands, band_count);
    }
    float GetStereoCorrelation() const noexcept {
        return m_spectrum_analyzer.GetStereoCorrelation();
    }

    void SetDeviceEventCallback(HP_DeviceEventCallback cb, void* user_data) {
        m_dev_cb = cb;
        m_dev_user_data = user_data;
    }

    void SetPlaybackEventCallback(HP_PlaybackEventCallback cb, void* user_data) {
        m_play_cb = cb;
        m_play_user_data = user_data;
    }

private:
    void DecodeThreadProc();
    void JoinDecodeThread() noexcept;
    void FirePlaybackEvent(uint32_t evt, uint32_t state, int32_t hr, uint64_t pos);
    void UpdateState(HP_State new_state);

    AudioDeviceManager* m_device_mgr = nullptr;
    FlacDecoder         m_decoder;
    WasapiRenderer      m_renderer;
    PcmRingBuffer       m_ring_buffer;
    DspGraph            m_dsp_graph;
    AudioDiagnostics    m_diagnostics;
    AudioResampler      m_resampler;
    SpectrumAnalyzer    m_spectrum_analyzer;

    std::wstring        m_current_file;
    std::wstring        m_selected_device_id;
    std::wstring        m_selected_device_name;
    HP_PlaybackMode     m_requested_mode = HP_SHARED;
    HP_PlaybackMode     m_actual_mode = HP_SHARED;
    std::atomic<uint32_t> m_state { HP_STATE_STOPPED };
    std::atomic<bool>   m_bluetooth_fidelity_mode { true };

    std::thread         m_decode_thread;
    std::atomic<bool>   m_decode_stop { false };
    std::atomic<bool>   m_is_preparing { false };
    std::atomic<bool>   m_decoder_finished { false };
    std::atomic<uint64_t> m_seek_target { 0 };
    std::atomic<bool>   m_seek_requested { false };
    uint64_t            m_current_gen = 1;

    HP_DeviceEventCallback   m_dev_cb = nullptr;
    void*                    m_dev_user_data = nullptr;
    HP_PlaybackEventCallback m_play_cb = nullptr;
    void*                    m_play_user_data = nullptr;

    std::recursive_mutex m_mutex;
};
