#include "AudioFormatNegotiator.h"
#include <cstring>
#include <cmath>

// {00000001-0000-0010-8000-00AA00389B71} KSDATAFORMAT_SUBTYPE_PCM
static const GUID kSubtypePCM = {
    0x00000001, 0x0000, 0x0010,
    { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 }
};

// {00000003-0000-0010-8000-00AA00389B71} KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
static const GUID kSubtypeFloat = {
    0x00000003, 0x0000, 0x0010,
    { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 }
};

DWORD AudioFormatNegotiator::ChannelMask(uint32_t channels) {
    if (channels == 1) return KSAUDIO_SPEAKER_MONO;    // 0x4
    if (channels == 2) return KSAUDIO_SPEAKER_STEREO;  // 0x3
    return 0;
}

WAVEFORMATEXTENSIBLE AudioFormatNegotiator::BuildExclusiveFormat(
    uint32_t sample_rate, uint32_t valid_bits, uint32_t channels
) {
    WAVEFORMATEXTENSIBLE wfex = {};
    // Use 32-bit container for 24-bit samples (most compatible with exclusive drivers)
    uint32_t container_bits = (valid_bits == 24) ? 32 : valid_bits;

    wfex.Format.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.nChannels       = (WORD)channels;
    wfex.Format.nSamplesPerSec  = sample_rate;
    wfex.Format.wBitsPerSample  = (WORD)container_bits;
    wfex.Format.nBlockAlign     = (WORD)(channels * container_bits / 8);
    wfex.Format.nAvgBytesPerSec = sample_rate * wfex.Format.nBlockAlign;
    wfex.Format.cbSize          = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfex.Samples.wValidBitsPerSample = (WORD)valid_bits;
    wfex.dwChannelMask           = ChannelMask(channels);
    wfex.SubFormat               = kSubtypePCM;
    return wfex;
}

WAVEFORMATEXTENSIBLE AudioFormatNegotiator::BuildSharedFloat32Format(
    uint32_t sample_rate, uint32_t channels
) {
    WAVEFORMATEXTENSIBLE wfex = {};
    wfex.Format.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.nChannels       = (WORD)channels;
    wfex.Format.nSamplesPerSec  = sample_rate;
    wfex.Format.wBitsPerSample  = 32;
    wfex.Format.nBlockAlign     = (WORD)(channels * 4);
    wfex.Format.nAvgBytesPerSec = sample_rate * wfex.Format.nBlockAlign;
    wfex.Format.cbSize          = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfex.Samples.wValidBitsPerSample = 32;
    wfex.dwChannelMask           = ChannelMask(channels);
    wfex.SubFormat               = kSubtypeFloat;
    return wfex;
}

FormatResult AudioFormatNegotiator::ProbeExclusive(
    IAudioClient* client,
    uint32_t sample_rate, uint32_t valid_bits, uint32_t channels
) {
    FormatResult result;
    result.hresult = S_OK;

    // Try 1: valid_bits in 32-bit container (most compatible for 24-bit)
    WAVEFORMATEXTENSIBLE wfex32 = BuildExclusiveFormat(sample_rate, valid_bits, channels);
    WAVEFORMATEX* closest = nullptr;
    HRESULT hr = client->IsFormatSupported(
        AUDCLNT_SHAREMODE_EXCLUSIVE,
        reinterpret_cast<WAVEFORMATEX*>(&wfex32),
        nullptr // exclusive mode: ppClosestMatch must be NULL
    );
    if (SUCCEEDED(hr) || hr == S_OK) {
        result.success             = true;
        result.exclusive_supported = true;
        result.wfex                = wfex32;
        result.hresult             = (int32_t)hr;
        result.reason              = L"Exclusive 24-in-32 format supported";
        return result;
    }

    // Try 2: packed 24-bit (3-byte) container if valid_bits == 24
    if (valid_bits == 24) {
        WAVEFORMATEXTENSIBLE wfex24 = {};
        wfex24.Format.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
        wfex24.Format.nChannels       = (WORD)channels;
        wfex24.Format.nSamplesPerSec  = sample_rate;
        wfex24.Format.wBitsPerSample  = 24;
        wfex24.Format.nBlockAlign     = (WORD)(channels * 3);
        wfex24.Format.nAvgBytesPerSec = sample_rate * wfex24.Format.nBlockAlign;
        wfex24.Format.cbSize          = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wfex24.Samples.wValidBitsPerSample = 24;
        wfex24.dwChannelMask           = ChannelMask(channels);
        wfex24.SubFormat               = kSubtypePCM;

        hr = client->IsFormatSupported(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            reinterpret_cast<WAVEFORMATEX*>(&wfex24),
            nullptr
        );
        if (SUCCEEDED(hr) || hr == S_OK) {
            result.success             = true;
            result.exclusive_supported = true;
            result.wfex                = wfex24;
            result.hresult             = (int32_t)hr;
            result.reason              = L"Exclusive packed-24 format supported";
            return result;
        }
    }

    // Try 3: 16 valid bits in 32-bit container if valid_bits == 16 (for USB DACs requiring 32-bit slot)
    if (valid_bits == 16) {
        WAVEFORMATEXTENSIBLE wfex16_32 = {};
        wfex16_32.Format.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
        wfex16_32.Format.nChannels       = (WORD)channels;
        wfex16_32.Format.nSamplesPerSec  = sample_rate;
        wfex16_32.Format.wBitsPerSample  = 32;
        wfex16_32.Format.nBlockAlign     = (WORD)(channels * 4);
        wfex16_32.Format.nAvgBytesPerSec = sample_rate * wfex16_32.Format.nBlockAlign;
        wfex16_32.Format.cbSize          = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wfex16_32.Samples.wValidBitsPerSample = 16;
        wfex16_32.dwChannelMask           = ChannelMask(channels);
        wfex16_32.SubFormat               = kSubtypePCM;

        hr = client->IsFormatSupported(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            reinterpret_cast<WAVEFORMATEX*>(&wfex16_32),
            nullptr
        );
        if (SUCCEEDED(hr) || hr == S_OK) {
            result.success             = true;
            result.exclusive_supported = true;
            result.wfex                = wfex16_32;
            result.hresult             = (int32_t)hr;
            result.reason              = L"Exclusive 16-in-32 format supported";
            return result;
        }
    }

    // All failed
    result.success             = false;
    result.exclusive_supported = false;
    result.hresult             = (int32_t)hr;
    result.reason              = L"Exclusive format not supported by this endpoint";
    return result;
}

FormatResult AudioFormatNegotiator::GetSharedMixFormat(IAudioClient* client) {
    FormatResult result;
    WAVEFORMATEX* pMixFmt = nullptr;
    HRESULT hr = client->GetMixFormat(&pMixFmt);
    if (FAILED(hr) || !pMixFmt) {
        result.success = false;
        result.hresult = (int32_t)hr;
        result.reason  = L"GetMixFormat failed";
        return result;
    }

    result.success     = true;
    result.mix_wfx     = *pMixFmt;
    result.hresult     = S_OK;
    result.needs_conversion = false; // Caller determines based on source rate
    CoTaskMemFree(pMixFmt);
    return result;
}
