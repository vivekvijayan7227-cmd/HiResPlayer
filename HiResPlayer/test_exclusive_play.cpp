#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iostream>
#include <cmath>

#ifndef AUDCLNT_STREAMFLAGS_EVENTCALLBACK
#define AUDCLNT_STREAMFLAGS_EVENTCALLBACK 0x00040000
#endif

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* enumerator = nullptr;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                     __uuidof(IMMDeviceEnumerator), (void**)&enumerator);

    IMMDevice* device = nullptr;
    enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);

    WAVEFORMATEXTENSIBLE wf16 = {};
    wf16.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wf16.Format.nChannels = 2;
    wf16.Format.nSamplesPerSec = 44100;
    wf16.Format.wBitsPerSample = 16;
    wf16.Format.nBlockAlign = 4;
    wf16.Format.nAvgBytesPerSec = 44100 * 4;
    wf16.Format.cbSize = 22;
    wf16.Samples.wValidBitsPerSample = 16;
    wf16.dwChannelMask = 0x3;
    wf16.SubFormat = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

    IAudioClient* client = nullptr;
    device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr, (void**)&client);

    HANDLE hEvt = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    REFERENCE_TIME p = 500000; // 50 ms
    HRESULT hr = client->Initialize(
        AUDCLNT_SHAREMODE_EXCLUSIVE,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        p, p,
        (WAVEFORMATEX*)&wf16,
        nullptr
    );

    UINT32 buf = 0;
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        client->GetBufferSize(&buf);
        REFERENCE_TIME aligned = (REFERENCE_TIME)(10000000.0 * buf / 44100 + 0.5);
        client->Release();
        device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr, (void**)&client);
        hr = client->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            aligned, aligned,
            (WAVEFORMATEX*)&wf16,
            nullptr
        );
        client->GetBufferSize(&buf);
        std::cout << "Aligned init: hr=0x" << std::hex << hr << std::dec << ", buf=" << buf << " frames" << std::endl;
    }

    client->SetEventHandle(hEvt);

    IAudioRenderClient* render = nullptr;
    client->GetService(__uuidof(IAudioRenderClient), (void**)&render);

    // Pre-roll with silence
    BYTE* pData = nullptr;
    render->GetBuffer(buf, &pData);
    std::memset(pData, 0, buf * 4);
    render->ReleaseBuffer(buf, 0);

    std::cout << "Pre-roll done. Starting client..." << std::endl;
    hr = client->Start();
    std::cout << "client->Start: hr=0x" << std::hex << hr << std::dec << std::endl;

    // Play 500 ms of 440Hz sine wave (soft volume)
    int total_frames = 44100 / 2;
    int written_frames = 0;
    double phase = 0.0;
    double phase_inc = 2.0 * 3.1415926535 * 440.0 / 44100.0;

    while (written_frames < total_frames) {
        WaitForSingleObject(hEvt, 2000);
        render->GetBuffer(buf, &pData);
        int16_t* samples = (int16_t*)pData;
        for (UINT32 i = 0; i < buf; ++i) {
            int16_t val = (int16_t)(std::sin(phase) * 5000.0); // soft volume
            samples[i * 2] = val;
            samples[i * 2 + 1] = val;
            phase += phase_inc;
        }
        render->ReleaseBuffer(buf, 0);
        written_frames += buf;
    }

    client->Stop();
    render->Release();
    client->Release();
    device->Release();
    enumerator->Release();
    CloseHandle(hEvt);
    CoUninitialize();
    std::cout << "Finished playback test." << std::endl;
    return 0;
}
