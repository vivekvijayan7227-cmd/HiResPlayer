#include "hires_audio_api.h"
#include "hires_dsp_api.h"
#include "PlaybackController.h"
#include "FlacDecoder.h"
#include "PcmSamplePacker.h"
#include <cstring>
#include <vector>

HP_API uint32_t HP_CALL hp_api_version(void) {
    return 1;
}

HP_API HP_Handle HP_CALL hp_create(void) {
    auto* ctrl = new (std::nothrow) PlaybackController();
    return reinterpret_cast<HP_Handle>(ctrl);
}

HP_API void HP_CALL hp_destroy(HP_Handle h) {
    if (!h) return;
    try {
        auto* ctrl = reinterpret_cast<PlaybackController*>(h);
        delete ctrl;
    } catch (...) {}
}

HP_API HP_Result HP_CALL hp_open_flac(HP_Handle h, const wchar_t* path) {
    if (!h || !path) return HP_INVALID_ARGUMENT;
    return reinterpret_cast<PlaybackController*>(h)->OpenFlac(path);
}

HP_API HP_Result HP_CALL hp_select_device(HP_Handle h, const wchar_t* endpoint_id) {
    if (!h || !endpoint_id) return HP_INVALID_ARGUMENT;
    return reinterpret_cast<PlaybackController*>(h)->SelectDevice(endpoint_id);
}

HP_API HP_Result HP_CALL hp_set_mode(HP_Handle h, HP_PlaybackMode mode) {
    if (!h) return HP_INVALID_ARGUMENT;
    return reinterpret_cast<PlaybackController*>(h)->SetMode(mode);
}

HP_API HP_Result HP_CALL hp_play(HP_Handle h) {
    if (!h) return HP_INVALID_ARGUMENT;
    return reinterpret_cast<PlaybackController*>(h)->Play();
}

HP_API HP_Result HP_CALL hp_pause(HP_Handle h) {
    if (!h) return HP_INVALID_ARGUMENT;
    return reinterpret_cast<PlaybackController*>(h)->Pause();
}

HP_API HP_Result HP_CALL hp_stop(HP_Handle h) {
    if (!h) return HP_INVALID_ARGUMENT;
    return reinterpret_cast<PlaybackController*>(h)->Stop();
}

HP_API HP_Result HP_CALL hp_seek_sample(HP_Handle h, uint64_t sample_position) {
    if (!h) return HP_INVALID_ARGUMENT;
    return reinterpret_cast<PlaybackController*>(h)->Seek(sample_position);
}

HP_API HP_Result HP_CALL hp_set_volume(HP_Handle h, float volume) {
    if (!h) return HP_INVALID_ARGUMENT;
    return reinterpret_cast<PlaybackController*>(h)->SetVolume(volume);
}

HP_API HP_Result HP_CALL hp_get_volume(HP_Handle h, float* out_volume) {
    if (!h || !out_volume) return HP_INVALID_ARGUMENT;
    return reinterpret_cast<PlaybackController*>(h)->GetVolume(out_volume);
}

HP_API HP_Result HP_CALL hp_get_status(HP_Handle h, HP_StatusV1* out_status) {
    if (!h || !out_status) return HP_INVALID_ARGUMENT;
    reinterpret_cast<PlaybackController*>(h)->GetStatus(out_status);
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_enumerate_devices(
    HP_Handle       h,
    HP_DeviceInfoV1* buf,
    uint32_t        buf_count,
    uint32_t*       out_count
) {
    if (!h || !out_count) return HP_INVALID_ARGUMENT;
    auto* ctrl = reinterpret_cast<PlaybackController*>(h);
    const auto devices = ctrl->GetDeviceManager().EnumerateDevices();

    *out_count = static_cast<uint32_t>(devices.size());
    if (!buf || buf_count == 0) return HP_OK;

    const uint32_t to_copy = std::min(buf_count, *out_count);
    for (uint32_t i = 0; i < to_copy; ++i) {
        std::memset(&buf[i], 0, sizeof(HP_DeviceInfoV1));
        buf[i].structure_size = sizeof(HP_DeviceInfoV1);
        wcsncpy_s(buf[i].id, devices[i].id.c_str(), _TRUNCATE);
        wcsncpy_s(buf[i].friendly_name, devices[i].friendly_name.c_str(), _TRUNCATE);
        buf[i].state = devices[i].state;
        buf[i].is_default_render = devices[i].is_default_render ? 1 : 0;
    }
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_query_format(
    HP_Handle      h,
    const wchar_t* endpoint_id,
    HP_PlaybackMode mode,
    HP_FormatCapV1* fmt
) {
    if (!h || !endpoint_id || !fmt) return HP_INVALID_ARGUMENT;
    auto* ctrl = reinterpret_cast<PlaybackController*>(h);

    IAudioClient* client = nullptr;
    HRESULT hr = ctrl->GetDeviceManager().OpenAudioClient(endpoint_id, &client);
    if (FAILED(hr) || !client) {
        fmt->probe_hresult = hr;
        return HP_DEVICE_UNAVAILABLE;
    }

    if (mode == HP_EXCLUSIVE) {
        auto res = AudioFormatNegotiator::ProbeExclusive(client, fmt->sample_rate, fmt->valid_bits, fmt->channels);
        fmt->exclusive_supported = res.exclusive_supported ? 1 : 0;
        fmt->probe_hresult = res.hresult;
    } else {
        auto res = AudioFormatNegotiator::GetSharedMixFormat(client);
        fmt->shared_mix_rate = res.mix_wfx.nSamplesPerSec;
        fmt->shared_mix_bits = res.mix_wfx.wBitsPerSample;
        fmt->probe_hresult = res.hresult;
    }

    client->Release();
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_set_device_event_callback(
    HP_Handle h,
    HP_DeviceEventCallback cb,
    void* user_data
) {
    if (!h) return HP_INVALID_ARGUMENT;
    reinterpret_cast<PlaybackController*>(h)->SetDeviceEventCallback(cb, user_data);
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_set_playback_event_callback(
    HP_Handle h,
    HP_PlaybackEventCallback cb,
    void* user_data
) {
    if (!h) return HP_INVALID_ARGUMENT;
    reinterpret_cast<PlaybackController*>(h)->SetPlaybackEventCallback(cb, user_data);
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_get_status_message(
    HP_Handle h,
    wchar_t*  buf,
    uint32_t  buf_wchar_count
) {
    if (!h || !buf || buf_wchar_count == 0) return HP_INVALID_ARGUMENT;
    const std::wstring msg = reinterpret_cast<PlaybackController*>(h)->GetStatusMessage();
    if (buf_wchar_count <= msg.size()) return HP_BUFFER_TOO_SMALL;
    const size_t len = std::min(static_cast<size_t>(buf_wchar_count - 1), msg.size());
    std::memcpy(buf, msg.c_str(), len * sizeof(wchar_t));
    buf[len] = L'\0';
    return HP_OK;
}

// ─── DSP Extensions ──────────────────────────────────────────────────────────

HP_API HP_Result HP_CALL hp_dsp_set_preamp(HP_Handle h, float preamp_db) {
    if (!h) return HP_INVALID_ARGUMENT;
    reinterpret_cast<PlaybackController*>(h)->GetDspGraph().SetPreampDb(preamp_db);
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_set_eq_bypass(HP_Handle h, uint32_t bypass) {
    if (!h) return HP_INVALID_ARGUMENT;
    reinterpret_cast<PlaybackController*>(h)->GetDspGraph().SetEqBypass(bypass != 0);
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_set_graphic_eq_band(HP_Handle h, uint32_t band_index, float gain_db) {
    if (!h) return HP_INVALID_ARGUMENT;
    reinterpret_cast<PlaybackController*>(h)->GetDspGraph().SetGraphicBand(band_index, gain_db);
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_reset_graphic_eq(HP_Handle h) {
    if (!h) return HP_INVALID_ARGUMENT;
    reinterpret_cast<PlaybackController*>(h)->GetDspGraph().ResetGraphicEq();
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_set_parametric_eq_band(
    HP_Handle h,
    uint32_t band_index,
    uint32_t enabled,
    uint32_t filter_type,
    float frequency,
    float gain_db,
    float q
) {
    if (!h) return HP_INVALID_ARGUMENT;
    ParametricBandConfig cfg;
    cfg.enabled = (enabled != 0);
    cfg.type = static_cast<BiquadType>(filter_type);
    cfg.frequency = frequency;
    cfg.gain_db = gain_db;
    cfg.q = q;
    reinterpret_cast<PlaybackController*>(h)->GetDspGraph().SetParametricBand(band_index, cfg);
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_reset_parametric_eq(HP_Handle h) {
    if (!h) return HP_INVALID_ARGUMENT;
    reinterpret_cast<PlaybackController*>(h)->GetDspGraph().ResetParametricEq();
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_evaluate_curve(HP_Handle h, float freq_hz, float* out_db) {
    if (!h || !out_db) return HP_INVALID_ARGUMENT;
    auto& dsp = reinterpret_cast<PlaybackController*>(h)->GetDspGraph();
    *out_db = dsp.EvaluateMagnitudeDb(freq_hz);
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_evaluate_center_curve(HP_Handle h, float freq_hz, float* out_db) {
    if (!h || !out_db) return HP_INVALID_ARGUMENT;
    auto& dsp = reinterpret_cast<PlaybackController*>(h)->GetDspGraph();
    *out_db = dsp.EvaluateCenterMagnitudeDb(freq_hz);
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_get_peaks(HP_Handle h, float* out_peak_l, float* out_peak_r, uint32_t* out_clipped) {
    if (!h || !out_peak_l || !out_peak_r || !out_clipped) return HP_INVALID_ARGUMENT;
    auto& dsp = reinterpret_cast<PlaybackController*>(h)->GetDspGraph();
    *out_peak_l = dsp.GetPeakLeft();
    *out_peak_r = dsp.GetPeakRight();
    *out_clipped = dsp.HasClipped() ? 1 : 0;
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_reset_peaks(HP_Handle h) {
    if (!h) return HP_INVALID_ARGUMENT;
    reinterpret_cast<PlaybackController*>(h)->GetDspGraph().ResetPeaks();
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_set_center_processor(HP_Handle h, uint32_t mode, float amount) {
    if (!h) return HP_INVALID_ARGUMENT;
    reinterpret_cast<PlaybackController*>(h)->GetDspGraph().SetCenterProcessor(mode, amount);
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_set_stem_mixer(
    HP_Handle h,
    float vocal_volume,
    float inst_volume,
    uint32_t mute_vocal,
    uint32_t mute_inst
) {
    if (!h) return HP_INVALID_ARGUMENT;
    reinterpret_cast<PlaybackController*>(h)->GetDspGraph().SetStemMixer(
        vocal_volume, inst_volume, mute_vocal != 0, mute_inst != 0
    );
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_disable_stem_mixer(HP_Handle h) {
    if (!h) return HP_INVALID_ARGUMENT;
    reinterpret_cast<PlaybackController*>(h)->GetDspGraph().DisableStemMixer();
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_get_spectrum(HP_Handle h, float* out_bands, uint32_t band_count) {
    if (!h || !out_bands || band_count == 0) return HP_INVALID_ARGUMENT;
    reinterpret_cast<PlaybackController*>(h)->GetSpectrum(out_bands, band_count);
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_get_stereo_correlation(HP_Handle h, float* out_correlation) {
    if (!h || !out_correlation) return HP_INVALID_ARGUMENT;
    *out_correlation = reinterpret_cast<PlaybackController*>(h)->GetStereoCorrelation();
    return HP_OK;
}

HP_API HP_Result HP_CALL hp_dsp_get_detailed_meters(
    HP_Handle h,
    float* out_in_peak_l,
    float* out_in_peak_r,
    float* out_out_peak_l,
    float* out_out_peak_r,
    float* out_limiter_gr,
    uint32_t* out_clipped
) {
    if (!h || !out_in_peak_l || !out_in_peak_r || !out_out_peak_l || !out_out_peak_r || !out_limiter_gr || !out_clipped)
        return HP_INVALID_ARGUMENT;
    auto& dsp = reinterpret_cast<PlaybackController*>(h)->GetDspGraph();
    *out_in_peak_l  = dsp.GetInPeakLeft();
    *out_in_peak_r  = dsp.GetInPeakRight();
    *out_out_peak_l = dsp.GetPeakLeft();
    *out_out_peak_r = dsp.GetPeakRight();
    *out_limiter_gr = dsp.GetLimiterGainReduction();
    *out_clipped    = dsp.HasClipped() ? 1 : 0;
    return HP_OK;
}

// ─── Bluetooth Extensions ────────────────────────────────────────────────────

HP_API HP_Result HP_CALL hp_get_bluetooth_device_info(
    HP_Handle                 h,
    const wchar_t*            endpoint_id,
    HP_BluetoothDeviceInfoV1* out_info
) {
    if (!h || !out_info) return HP_INVALID_ARGUMENT;
    return reinterpret_cast<PlaybackController*>(h)->GetBluetoothDeviceInfo(endpoint_id ? endpoint_id : L"", out_info);
}

HP_API HP_Result HP_CALL hp_set_bluetooth_fidelity_mode(
    HP_Handle h,
    uint32_t  enabled
) {
    if (!h) return HP_INVALID_ARGUMENT;
    return reinterpret_cast<PlaybackController*>(h)->SetBluetoothFidelityMode(enabled != 0);
}

HP_API HP_Result HP_CALL hp_get_bluetooth_fidelity_status(
    HP_Handle                     h,
    HP_BluetoothFidelityStatusV1* out_status
) {
    if (!h || !out_status) return HP_INVALID_ARGUMENT;
    return reinterpret_cast<PlaybackController*>(h)->GetBluetoothFidelityStatus(out_status);
}

// ─── Reference PCM Verification ──────────────────────────────────────────────
HP_API HP_Result HP_CALL hp_verify_flac_pcm_exact(
    const wchar_t* flac_path,
    const wchar_t* wav_path,
    uint32_t*      out_mismatches,
    uint64_t*      out_compared_samples
) {
    if (!flac_path || !wav_path || !out_mismatches || !out_compared_samples) {
        return HP_INVALID_ARGUMENT;
    }
    *out_mismatches = 0;
    *out_compared_samples = 0;

    // 1. Read and parse reference WAV file
    FILE* fw = _wfopen(wav_path, L"rb");
    if (!fw) return HP_FILE_ERROR;

    uint8_t header[12];
    if (fread(header, 1, 12, fw) != 12 || std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) {
        fclose(fw);
        return HP_UNSUPPORTED_SOURCE;
    }

    uint16_t wav_channels = 0;
    uint32_t wav_sample_rate = 0;
    uint16_t wav_bits = 0;
    std::vector<uint8_t> wav_data;

    while (!feof(fw)) {
        char chunk_id[4];
        uint32_t chunk_size = 0;
        if (fread(chunk_id, 1, 4, fw) != 4) break;
        if (fread(&chunk_size, 4, 1, fw) != 1) break;

        if (std::memcmp(chunk_id, "fmt ", 4) == 0) {
            uint16_t audio_format = 0;
            if (fread(&audio_format, 2, 1, fw) != 1) { fclose(fw); return HP_FILE_ERROR; }
            if (fread(&wav_channels, 2, 1, fw) != 1) { fclose(fw); return HP_FILE_ERROR; }
            if (fread(&wav_sample_rate, 4, 1, fw) != 1) { fclose(fw); return HP_FILE_ERROR; }
            uint32_t byte_rate = 0;
            if (fread(&byte_rate, 4, 1, fw) != 1) { fclose(fw); return HP_FILE_ERROR; }
            uint16_t block_align = 0;
            if (fread(&block_align, 2, 1, fw) != 1) { fclose(fw); return HP_FILE_ERROR; }
            if (fread(&wav_bits, 2, 1, fw) != 1) { fclose(fw); return HP_FILE_ERROR; }
            if (chunk_size > 16) {
                _fseeki64(fw, chunk_size - 16, SEEK_CUR);
            }
        } else if (std::memcmp(chunk_id, "data", 4) == 0) {
            wav_data.resize(chunk_size);
            size_t bytes_read = fread(wav_data.data(), 1, chunk_size, fw);
            if (bytes_read != chunk_size) {
                wav_data.resize(bytes_read);
            }
            break;
        } else {
            _fseeki64(fw, chunk_size, SEEK_CUR);
        }
    }
    fclose(fw);

    if (wav_data.empty() || wav_channels == 0 || wav_bits == 0) {
        return HP_FILE_ERROR;
    }

    // 2. Open FLAC file and verify metadata match
    FlacDecoder decoder;
    std::string err;
    if (!decoder.Open(flac_path, err)) {
        return HP_FILE_ERROR;
    }
    const auto& meta = decoder.metadata();
    if (meta.channels != wav_channels || meta.sample_rate != wav_sample_rate || meta.bits_per_sample != wav_bits) {
        return HP_UNSUPPORTED_OUTPUT_FORMAT;
    }

    // 3. Decode FLAC and accumulate packed PCM frames
    PackParams pack_params;
    pack_params.valid_bits = meta.bits_per_sample;
    pack_params.container_bits = (meta.bits_per_sample == 24) ? 24 : meta.bits_per_sample;
    pack_params.output_float = false;

    const uint32_t bytes_per_sample = pack_params.container_bits / 8;
    const uint32_t bytes_per_frame = meta.channels * bytes_per_sample;

    std::vector<uint8_t> flac_pcm;
    flac_pcm.reserve(wav_data.size());

    std::vector<uint8_t> block_buf;
    decoder.SetPcmCallback([&](const int32_t* const* samples, uint32_t ch, uint32_t spc, uint32_t) {
        size_t block_bytes = spc * bytes_per_frame;
        if (block_buf.size() < block_bytes) {
            block_buf.resize(block_bytes);
        }
        pack_frames(block_buf.data(), samples, ch, spc, pack_params);
        flac_pcm.insert(flac_pcm.end(), block_buf.data(), block_buf.data() + block_bytes);
    });

    if (!decoder.DecodeAll(err) || decoder.HasError()) {
        return HP_DECODER_ERROR;
    }

    // 4. Sample-by-sample bit-exact verification
    size_t compare_bytes = (flac_pcm.size() < wav_data.size()) ? flac_pcm.size() : wav_data.size();
    uint64_t total_samples = compare_bytes / bytes_per_sample;
    uint32_t mismatches = 0;

    for (size_t i = 0; i < compare_bytes; i += bytes_per_sample) {
        if (std::memcmp(flac_pcm.data() + i, wav_data.data() + i, bytes_per_sample) != 0) {
            mismatches++;
        }
    }

    if (flac_pcm.size() != wav_data.size()) {
        mismatches += static_cast<uint32_t>(std::abs(static_cast<int64_t>(flac_pcm.size()) - static_cast<int64_t>(wav_data.size())) / bytes_per_sample);
    }

    *out_compared_samples = total_samples;
    *out_mismatches = mismatches;
    return HP_OK;
}

