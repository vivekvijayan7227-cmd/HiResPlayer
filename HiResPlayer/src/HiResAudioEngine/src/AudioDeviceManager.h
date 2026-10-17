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
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <mutex>
#include "../include/hires_audio_api.h"

struct DeviceInfo {
    std::wstring id;
    std::wstring friendly_name;
    uint32_t     state = HP_DEVICE_NOTPRESENT;
    bool         is_default_render = false;
};

using DeviceEventCb = std::function<void(uint32_t event_type, const std::wstring& endpoint_id)>;

// Manages IMMDeviceEnumerator, endpoint listing, and change notifications.
// Must be created on a thread with COM initialized (MTA).
class AudioDeviceManager : public IMMNotificationClient {
public:
    AudioDeviceManager();
    virtual ~AudioDeviceManager();

    bool Initialize();
    void Shutdown();

    // Get current list of active render endpoints
    std::vector<DeviceInfo> EnumerateDevices();

    // Open IAudioClient for a specific endpoint. Caller Release()s it.
    HRESULT OpenAudioClient(const std::wstring& endpoint_id, IAudioClient** out_client);

    // Get IMMDevice for a specific endpoint. Caller Release()s it.
    HRESULT GetDevice(const std::wstring& endpoint_id, IMMDevice** out_device);

    // Inspect Bluetooth capabilities for an endpoint
    bool GetBluetoothDeviceInfo(const std::wstring& endpoint_id, HP_BluetoothDeviceInfoV1* out_info);

    // Hardware endpoint volume control (scalar 0.0 to 1.0)
    HRESULT SetMasterVolume(const std::wstring& endpoint_id, float volume);
    HRESULT GetMasterVolume(const std::wstring& endpoint_id, float* out_volume);

    // Get default render endpoint ID
    std::wstring GetDefaultDeviceId();

    // Set callback (called on control thread, not MM notification thread)
    void SetEventCallback(DeviceEventCb cb) {
        std::lock_guard<std::mutex> lk(m_cb_mutex);
        m_event_cb = std::move(cb);
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef()  override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG ref = InterlockedDecrement(&m_ref);
        if (ref == 0) {
            delete this;
        }
        return ref;
    }

    // IMMNotificationClient
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
    STDMETHODIMP OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
    STDMETHODIMP OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) override;
    STDMETHODIMP OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override { return S_OK; }

private:
    void FireEvent(uint32_t type, const std::wstring& id);

    IMMDeviceEnumerator* m_enumerator = nullptr;
    volatile LONG        m_ref        = 1;
    std::mutex           m_cb_mutex;
    DeviceEventCb        m_event_cb;
    bool                 m_initialized = false;
};
