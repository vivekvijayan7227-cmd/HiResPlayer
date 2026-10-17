#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <propsys.h>
#include <propvarutil.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <string>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

static const PROPERTYKEY PKey_Device_FriendlyName = {
    { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14
};
static const PROPERTYKEY PKey_Device_DeviceDesc = {
    { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 2
};
static const PROPERTYKEY PKey_Device_EnumeratorName = {
    { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 24
};
static const PROPERTYKEY PKey_Device_InstanceId = {
    { 0x78c34fc8, 0x104a, 0x4aca, { 0x9e, 0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57 } }, 256
};
static const PROPERTYKEY PKey_Bluetooth_DeviceAddress = {
    { 0x2bd67d8b, 0x8beb, 0x48d5, { 0x87, 0xe0, 0x6c, 0xda, 0x34, 0x28, 0x04, 0x0a } }, 1
};
static const PROPERTYKEY PKey_AudioEngine_DeviceFormat = {
    { 0xf19f064d, 0x0824, 0x4e22, { 0x82, 0x22, 0x37, 0x72, 0x4e, 0x83, 0xc7, 0xb0 } }, 0
};

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator)
    );
    if (FAILED(hr) || !enumerator) {
        std::cout << "Failed to create MMDeviceEnumerator: " << std::hex << hr << std::endl;
        return 1;
    }

    IMMDeviceCollection* coll = nullptr;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED | DEVICE_STATE_DISABLED, &coll);
    if (FAILED(hr) || !coll) {
        std::cout << "EnumAudioEndpoints failed" << std::endl;
        return 1;
    }

    UINT count = 0;
    coll->GetCount(&count);
    std::cout << "Total Render Endpoints found: " << count << std::endl;

    for (UINT i = 0; i < count; ++i) {
        IMMDevice* dev = nullptr;
        if (FAILED(coll->Item(i, &dev)) || !dev) continue;

        LPWSTR id = nullptr;
        dev->GetId(&id);
        DWORD state = 0;
        dev->GetState(&state);

        IPropertyStore* props = nullptr;
        hr = dev->OpenPropertyStore(STGM_READ, &props);
        if (SUCCEEDED(hr) && props) {
            std::wcout << L"\n--- Endpoint " << i << L" (State=" << state << L") ---" << std::endl;
            if (id) std::wcout << L"  ID: " << id << std::endl;

            PROPVARIANT pv;
            PropVariantInit(&pv);

            if (SUCCEEDED(props->GetValue(PKey_Device_FriendlyName, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal) {
                std::wcout << L"  FriendlyName: " << pv.pwszVal << std::endl;
            }
            PropVariantClear(&pv);

            if (SUCCEEDED(props->GetValue(PKey_Device_DeviceDesc, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal) {
                std::wcout << L"  DeviceDesc: " << pv.pwszVal << std::endl;
            }
            PropVariantClear(&pv);

            if (SUCCEEDED(props->GetValue(PKey_Device_EnumeratorName, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal) {
                std::wcout << L"  Enumerator: " << pv.pwszVal << std::endl;
            }
            PropVariantClear(&pv);

            if (SUCCEEDED(props->GetValue(PKey_Device_InstanceId, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal) {
                std::wcout << L"  InstanceId: " << pv.pwszVal << std::endl;
            }
            PropVariantClear(&pv);

            if (SUCCEEDED(props->GetValue(PKey_Bluetooth_DeviceAddress, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal) {
                std::wcout << L"  BluetoothAddress: " << pv.pwszVal << std::endl;
            }
            PropVariantClear(&pv);

            if (SUCCEEDED(props->GetValue(PKey_AudioEngine_DeviceFormat, &pv)) && pv.vt == VT_BLOB && pv.blob.pBlobData) {
                const WAVEFORMATEX* wfx = reinterpret_cast<const WAVEFORMATEX*>(pv.blob.pBlobData);
                std::cout << "  DeviceFormat: " << wfx->nSamplesPerSec << " Hz, "
                          << wfx->wBitsPerSample << "-bit, "
                          << wfx->nChannels << " ch, tag=0x" << std::hex << wfx->wFormatTag << std::dec << std::endl;
            }
            PropVariantClear(&pv);

            props->Release();
        }
        if (id) CoTaskMemFree(id);
        dev->Release();
    }

    coll->Release();
    enumerator->Release();
    CoUninitialize();
    return 0;
}
