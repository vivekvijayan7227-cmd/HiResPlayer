#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iostream>
#include <vector>
#include <iomanip>

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                                  __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) {
        std::cout << "Failed to create enumerator: " << std::hex << hr << std::endl;
        return 1;
    }

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) {
        std::cout << "Failed to get default endpoint: " << std::hex << hr << std::endl;
        return 1;
    }

    IAudioClient* client = nullptr;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr, (void**)&client);
    if (FAILED(hr)) {
        std::cout << "Failed to activate IAudioClient: " << std::hex << hr << std::endl;
        return 1;
    }

    WAVEFORMATEX* mix_format = nullptr;
    client->GetMixFormat(&mix_format);
    std::cout << "Mix Format: " << mix_format->nSamplesPerSec << " Hz, "
              << mix_format->wBitsPerSample << " bits, "
              << mix_format->nChannels << " channels, tag=0x" << std::hex << mix_format->wFormatTag << std::dec << std::endl;

    REFERENCE_TIME defPeriod = 0, minPeriod = 0;
    client->GetDevicePeriod(&defPeriod, &minPeriod);
    std::cout << "Device Period: default=" << defPeriod << " (" << (defPeriod / 10000.0) << " ms), min="
              << minPeriod << " (" << (minPeriod / 10000.0) << " ms)" << std::endl;

    // Test Exclusive formats
    uint32_t rates[] = { 44100, 48000, 88200, 96000, 192000 };
    for (uint32_t r : rates) {
        // 16-bit PCM (container 16)
        WAVEFORMATEXTENSIBLE wf16 = {};
        wf16.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wf16.Format.nChannels = 2;
        wf16.Format.nSamplesPerSec = r;
        wf16.Format.wBitsPerSample = 16;
        wf16.Format.nBlockAlign = 4;
        wf16.Format.nAvgBytesPerSec = r * 4;
        wf16.Format.cbSize = 22;
        wf16.Samples.wValidBitsPerSample = 16;
        wf16.dwChannelMask = 0x3;
        wf16.SubFormat = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

        HRESULT hr16 = client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, (WAVEFORMATEX*)&wf16, nullptr);
        std::cout << "Exclusive " << r << "Hz 16-bit: " << (hr16 == S_OK ? "SUPPORTED" : "UNSUPPORTED") << " (0x" << std::hex << hr16 << std::dec << ")" << std::endl;

        // 24-in-32 PCM
        WAVEFORMATEXTENSIBLE wf24in32 = {};
        wf24in32.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wf24in32.Format.nChannels = 2;
        wf24in32.Format.nSamplesPerSec = r;
        wf24in32.Format.wBitsPerSample = 32;
        wf24in32.Format.nBlockAlign = 8;
        wf24in32.Format.nAvgBytesPerSec = r * 8;
        wf24in32.Format.cbSize = 22;
        wf24in32.Samples.wValidBitsPerSample = 24;
        wf24in32.dwChannelMask = 0x3;
        wf24in32.SubFormat = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

        HRESULT hr24 = client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, (WAVEFORMATEX*)&wf24in32, nullptr);
        std::cout << "Exclusive " << r << "Hz 24-in-32: " << (hr24 == S_OK ? "SUPPORTED" : "UNSUPPORTED") << " (0x" << std::hex << hr24 << std::dec << ")" << std::endl;

        // 24-bit packed
        WAVEFORMATEXTENSIBLE wf24packed = {};
        wf24packed.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wf24packed.Format.nChannels = 2;
        wf24packed.Format.nSamplesPerSec = r;
        wf24packed.Format.wBitsPerSample = 24;
        wf24packed.Format.nBlockAlign = 6;
        wf24packed.Format.nAvgBytesPerSec = r * 6;
        wf24packed.Format.cbSize = 22;
        wf24packed.Samples.wValidBitsPerSample = 24;
        wf24packed.dwChannelMask = 0x3;
        wf24packed.SubFormat = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

        HRESULT hr24p = client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, (WAVEFORMATEX*)&wf24packed, nullptr);
        std::cout << "Exclusive " << r << "Hz 24-packed: " << (hr24p == S_OK ? "SUPPORTED" : "UNSUPPORTED") << " (0x" << std::hex << hr24p << std::dec << ")" << std::endl;

        // Standard WAVEFORMATEX (non-extensible) 16-bit
        WAVEFORMATEX wfx16 = {};
        wfx16.wFormatTag = WAVE_FORMAT_PCM;
        wfx16.nChannels = 2;
        wfx16.nSamplesPerSec = r;
        wfx16.wBitsPerSample = 16;
        wfx16.nBlockAlign = 4;
        wfx16.nAvgBytesPerSec = r * 4;
        wfx16.cbSize = 0;
        HRESULT hrwfx16 = client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &wfx16, nullptr);
        std::cout << "Exclusive " << r << "Hz std 16-bit: " << (hrwfx16 == S_OK ? "SUPPORTED" : "UNSUPPORTED") << " (0x" << std::hex << hrwfx16 << std::dec << ")" << std::endl;
    }

    client->Release();
    device->Release();
    enumerator->Release();
    CoUninitialize();
    return 0;
}
