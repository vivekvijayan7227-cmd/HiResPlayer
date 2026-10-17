#include "AudioDeviceManager.h"
#include "BluetoothDeviceService.h"
#include <propvarutil.h>
#include <propkey.h>
#include <cstring>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

AudioDeviceManager::AudioDeviceManager() {}

AudioDeviceManager::~AudioDeviceManager() {
    Shutdown();
}

bool AudioDeviceManager::Initialize() {
    if (m_initialized) return true;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&m_enumerator)
    );
    if (FAILED(hr)) return false;

    m_enumerator->RegisterEndpointNotificationCallback(this);
    m_initialized = true;
    return true;
}

void AudioDeviceManager::Shutdown() {
    if (m_enumerator) {
        m_enumerator->UnregisterEndpointNotificationCallback(this);
        m_enumerator->Release();
        m_enumerator = nullptr;
    }
    m_initialized = false;
    CoUninitialize();
}

std::vector<DeviceInfo> AudioDeviceManager::EnumerateDevices() {
    std::vector<DeviceInfo> result;
    if (!m_enumerator) return result;

    // Get default device ID first
    std::wstring default_id = GetDefaultDeviceId();

    IMMDeviceCollection* coll = nullptr;
    HRESULT hr = m_enumerator->EnumAudioEndpoints(
        eRender, DEVICE_STATE_ACTIVE,
        &coll
    );
    if (FAILED(hr) || !coll) return result;

    UINT count = 0;
    coll->GetCount(&count);

    for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        if (FAILED(coll->Item(i, &device)) || !device) continue;

        DeviceInfo info;

        // Get endpoint ID
        LPWSTR pwstrId = nullptr;
        if (SUCCEEDED(device->GetId(&pwstrId)) && pwstrId) {
            info.id = pwstrId;
            CoTaskMemFree(pwstrId);
        }

        // Get state
        DWORD state = 0;
        device->GetState(&state);
        info.state = (uint32_t)state;

        // Get friendly name from property store
        IPropertyStore* props = nullptr;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props)) && props) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &pv)) &&
                pv.vt == VT_LPWSTR && pv.pwszVal) {
                info.friendly_name = pv.pwszVal;
            }
            PropVariantClear(&pv);
            props->Release();
        }

        if (info.friendly_name.empty()) {
            info.friendly_name = info.id.empty() ? L"Unknown Device" : info.id;
        }

        info.is_default_render = (!info.id.empty() && info.id == default_id);
        result.push_back(std::move(info));
        device->Release();
    }

    coll->Release();
    return result;
}

HRESULT AudioDeviceManager::GetDevice(
    const std::wstring& endpoint_id, IMMDevice** out_device
) {
    if (!m_enumerator || !out_device) return E_INVALIDARG;
    if (endpoint_id.empty()) {
        return m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, out_device);
    }
    return m_enumerator->GetDevice(endpoint_id.c_str(), out_device);
}

HRESULT AudioDeviceManager::SetMasterVolume(const std::wstring& endpoint_id, float volume) {
    if (!m_enumerator) return E_FAIL;
    IMMDevice* device = nullptr;
    HRESULT hr = GetDevice(endpoint_id, &device);
    if (FAILED(hr) || !device) return hr;

    IAudioEndpointVolume* epVol = nullptr;
    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void**>(&epVol));
    if (SUCCEEDED(hr) && epVol) {
        if (volume < 0.0f) volume = 0.0f;
        if (volume > 1.0f) volume = 1.0f;
        epVol->SetMasterVolumeLevelScalar(volume, nullptr);
        epVol->Release();
    }
    device->Release();
    return hr;
}

HRESULT AudioDeviceManager::GetMasterVolume(const std::wstring& endpoint_id, float* out_volume) {
    if (!out_volume) return E_INVALIDARG;
    *out_volume = 1.0f;
    if (!m_enumerator) return E_FAIL;
    IMMDevice* device = nullptr;
    HRESULT hr = GetDevice(endpoint_id, &device);
    if (FAILED(hr) || !device) return hr;

    IAudioEndpointVolume* epVol = nullptr;
    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void**>(&epVol));
    if (SUCCEEDED(hr) && epVol) {
        epVol->GetMasterVolumeLevelScalar(out_volume);
        epVol->Release();
    }
    device->Release();
    return hr;
}

HRESULT AudioDeviceManager::OpenAudioClient(
    const std::wstring& endpoint_id, IAudioClient** out_client
) {
    if (!m_enumerator || !out_client) return E_INVALIDARG;

    IMMDevice* device = nullptr;
    HRESULT hr = GetDevice(endpoint_id, &device);
    if (FAILED(hr) || !device) return hr;

    hr = device->Activate(
        __uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr,
        reinterpret_cast<void**>(out_client)
    );
    device->Release();
    return hr;
}

std::wstring AudioDeviceManager::GetDefaultDeviceId() {
    if (!m_enumerator) return {};
    IMMDevice* dev = nullptr;
    if (FAILED(m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) || !dev)
        return {};
    LPWSTR id = nullptr;
    std::wstring result;
    if (SUCCEEDED(dev->GetId(&id)) && id) {
        result = id;
        CoTaskMemFree(id);
    }
    dev->Release();
    return result;
}

void AudioDeviceManager::FireEvent(uint32_t type, const std::wstring& id) {
    std::lock_guard<std::mutex> lk(m_cb_mutex);
    if (m_event_cb) m_event_cb(type, id);
}

STDMETHODIMP AudioDeviceManager::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
        *ppv = static_cast<IMMNotificationClient*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP AudioDeviceManager::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD) {
    FireEvent(HP_EVT_DEVICE_STATE_CHANGED, pwstrDeviceId ? pwstrDeviceId : L"");
    return S_OK;
}

STDMETHODIMP AudioDeviceManager::OnDeviceAdded(LPCWSTR pwstrDeviceId) {
    FireEvent(HP_EVT_DEVICE_ADDED, pwstrDeviceId ? pwstrDeviceId : L"");
    return S_OK;
}

STDMETHODIMP AudioDeviceManager::OnDeviceRemoved(LPCWSTR pwstrDeviceId) {
    FireEvent(HP_EVT_DEVICE_REMOVED, pwstrDeviceId ? pwstrDeviceId : L"");
    return S_OK;
}

STDMETHODIMP AudioDeviceManager::OnDefaultDeviceChanged(
    EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId
) {
    if (flow == eRender && role == eConsole) {
        FireEvent(HP_EVT_DEFAULT_DEVICE_CHANGED,
                  pwstrDefaultDeviceId ? pwstrDefaultDeviceId : L"");
    }
    return S_OK;
}

bool AudioDeviceManager::GetBluetoothDeviceInfo(
    const std::wstring& endpoint_id, HP_BluetoothDeviceInfoV1* out_info
) {
    if (!out_info) return false;
    std::wstring dev_id = endpoint_id.empty() ? GetDefaultDeviceId() : endpoint_id;
    if (dev_id.empty()) return false;

    IMMDevice* dev = nullptr;
    HRESULT hr = GetDevice(dev_id, &dev);
    if (FAILED(hr) || !dev) return false;

    IAudioClient* client = nullptr;
    OpenAudioClient(dev_id, &client);

    bool ok = BluetoothDeviceService::InspectEndpoint(dev, client, out_info);

    if (client) client->Release();
    dev->Release();
    return ok;
}
