#include "audio.h"
#include "resource.h"
#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <map>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functiondiscoverykeys_devpkey.h>

// Link against required libs
#pragma comment(lib, "ole32.lib")

// Link against required libs
#pragma comment(lib, "ole32.lib")

// ==========================================
// Utils: Registry Helpers
// ==========================================
static const char* REG_PATH_VOLUMES = "Software\\MicMute-S\\DeviceVolumes";

void RegWriteVolume(const std::wstring& deviceId, float volume) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MicMute-S\\DeviceVolumes", 0, NULL, 
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, deviceId.c_str(), 0, REG_DWORD, (const BYTE*)&volume, sizeof(float));
        RegCloseKey(hKey);
    }
}

float RegReadVolume(const std::wstring& deviceId) {
    HKEY hKey;
    float volume = -1.0f; // Indicator for "not found"
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\MicMute-S\\DeviceVolumes", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(float);
        DWORD type = REG_DWORD;
        RegQueryValueExW(hKey, deviceId.c_str(), NULL, &type, (BYTE*)&volume, &size);
        RegCloseKey(hKey);
    }
    return volume;
}

// ==========================================
// Audio Callback Class
// ==========================================
class AudioVolumeCallback : public IAudioEndpointVolumeCallback {
    LONG _cRef;
public:
    AudioVolumeCallback() : _cRef(1) {}
    
    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IAudioEndpointVolumeCallback)) {
            *ppv = static_cast<IAudioEndpointVolumeCallback*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&_cRef);
    }
    STDMETHODIMP_(ULONG) Release() {
        LONG ref = InterlockedDecrement(&_cRef);
        if (ref == 0) delete this;
        return ref;
    }

    // Callback method
    STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) {
        // Trigger UI update on main thread
        // We use PostMessage because this runs on a high-priority RPC thread provided by Windows
        extern HWND hMainWnd;
        if (hMainWnd) {
            PostMessage(hMainWnd, WM_APP_MUTE_CHANGED, 0, 0);
        }
        return S_OK;
    }
};

// ==========================================
// AudioManager Class
// ==========================================
class AudioManager {
private:
    ComPtr<IMMDeviceEnumerator> pEnumerator;
    ComPtr<IMMDevice> pDefaultDevice;
    ComPtr<IAudioMeterInformation> pMeterInfo;
    ComPtr<IAudioEndpointVolume> pEndpointVolume;

    ComPtr<IMMDevice> pSpeakerDevice;
    ComPtr<IAudioMeterInformation> pSpeakerMeterInfo;
    
    std::atomic<float> currentPeakLevel;
    std::atomic<float> currentSpeakerLevel;
    std::atomic<bool> isMutedGlobal;
    std::mutex deviceMutex;
    
    ComPtr<AudioVolumeCallback> pVolumeCallback;

    void InitializeCOM() {
        if (!pEnumerator) {
            CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, 
                IID_PPV_ARGS(&pEnumerator));
        }
    }

    // Returns true if successful
    bool UpdateDevices() {
        std::lock_guard<std::mutex> lock(deviceMutex);
        InitializeCOM();
        if (!pEnumerator) return false;

        bool deviceChanged = false;

        // 1. MIC (Capture)
        ComPtr<IMMDevice> pNewRecDevice;
        HRESULT hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pNewRecDevice);
        
        if (SUCCEEDED(hr)) {
            // Check identity to see if we need to re-register callback
            LPWSTR currentId = NULL;
            LPWSTR newId = NULL;
            
            if (pDefaultDevice) pDefaultDevice->GetId(&currentId);
            if (pNewRecDevice) pNewRecDevice->GetId(&newId);
            
            bool different = false;
            if (!currentId && newId) different = true;
            else if (currentId && !newId) different = true;
            else if (currentId && newId && wcscmp(currentId, newId) != 0) different = true;
            
            if (currentId) CoTaskMemFree(currentId);
            if (newId) CoTaskMemFree(newId);

            if (different || !pDefaultDevice) {
                // Unregister old callback if exists
                if (pEndpointVolume && pVolumeCallback) {
                    pEndpointVolume->UnregisterControlChangeNotify(pVolumeCallback.Get());
                }

                pDefaultDevice = pNewRecDevice;
                pEndpointVolume.Reset();
                pMeterInfo.Reset();

                pDefaultDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, NULL, (void**)&pMeterInfo);
                pDefaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pEndpointVolume);

                // Register new callback
                if (pEndpointVolume && !pVolumeCallback) {
                    pVolumeCallback = new AudioVolumeCallback(); // ComPtr will manage RefCount
                }
                if (pEndpointVolume && pVolumeCallback) {
                    pEndpointVolume->RegisterControlChangeNotify(pVolumeCallback.Get());
                }
                deviceChanged = true;
            } else {
                 // Even if same device, make sure interfaces are alive
                 if (!pMeterInfo) pDefaultDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, NULL, (void**)&pMeterInfo);
                 if (!pEndpointVolume) {
                     pDefaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pEndpointVolume);
                     if (pEndpointVolume && pVolumeCallback) {
                         pEndpointVolume->RegisterControlChangeNotify(pVolumeCallback.Get());
                     }
                 }
            }
        } else {
             if (pEndpointVolume && pVolumeCallback) {
                 pEndpointVolume->UnregisterControlChangeNotify(pVolumeCallback.Get());
             }
             pDefaultDevice.Reset();
             pMeterInfo.Reset();
             pEndpointVolume.Reset();
        }

        // 2. SPEAKER (Render) (Just for meter)
        ComPtr<IMMDevice> pNewRenderDevice;
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pNewRenderDevice);
        
        if (SUCCEEDED(hr)) {
            if (!pSpeakerDevice) {
                pSpeakerDevice = pNewRenderDevice;
                pSpeakerDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, NULL, (void**)&pSpeakerMeterInfo);
            }
        } else {
             pSpeakerDevice.Reset();
             pSpeakerMeterInfo.Reset();
        }
        
        // Initial sync of mute state
        if (deviceChanged && pEndpointVolume) {
            BOOL bMute;
            pEndpointVolume->GetMute(&bMute);
            isMutedGlobal = (bMute == TRUE);
        }

        return (pDefaultDevice && pMeterInfo); 
    }

public:
    AudioManager() : currentPeakLevel(0.0f), currentSpeakerLevel(0.0f), isMutedGlobal(false) {}
    
    ~AudioManager() {
        Stop();
    }

    void Start() {
        // Just UpdateDevices once to hook everything up
        UpdateDevices();
    }

    void Stop() {
        std::lock_guard<std::mutex> lock(deviceMutex);
        if (pEndpointVolume && pVolumeCallback) {
            pEndpointVolume->UnregisterControlChangeNotify(pVolumeCallback.Get());
        }
        pEndpointVolume.Reset();
        pVolumeCallback.Reset();
        pMeterInfo.Reset();
        pDefaultDevice.Reset();
        pSpeakerMeterInfo.Reset();
        pSpeakerDevice.Reset();
        pEnumerator.Reset();
    }

    float GetCurrentLevel() {
        // Meter still needs to be polled, but ONLY when requested (by UI timer)
        // We do it on demand here instead of a background thread
        // Ideally we would optimize this to not Activate() every time if possible (cached above)
        
        float peak = 0.0f;
        std::lock_guard<std::mutex> lock(deviceMutex);
        if (pMeterInfo) {
            pMeterInfo->GetPeakValue(&peak);
        }
        return peak;
    }

    float GetCurrentSpeakerLevel() {
        float peak = 0.0f;
        std::lock_guard<std::mutex> lock(deviceMutex);
        if (pSpeakerMeterInfo) {
            pSpeakerMeterInfo->GetPeakValue(&peak);
        }
        return peak;
    }

    std::wstring GetDeviceName() {
        std::lock_guard<std::mutex> lock(deviceMutex);
        if (!pDefaultDevice) return L"No Microphone Found";
        
        ComPtr<IPropertyStore> pProps;
        if (SUCCEEDED(pDefaultDevice->OpenPropertyStore(STGM_READ, &pProps))) {
            PROPVARIANT varName;
            PropVariantInit(&varName);
            if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))) {
                std::wstring name = varName.pwszVal;
                PropVariantClear(&varName);
                return name;
            }
        }
        return L"Unknown Device";
    }

    bool GetMuteState() {
        return isMutedGlobal;
    }
    
    // Mute/Unmute Logic
    bool SetMute(bool mute) {
        std::lock_guard<std::mutex> lock(deviceMutex);
        
        if (pEndpointVolume) {
             LPWSTR wstrId = NULL;
             if (pDefaultDevice) pDefaultDevice->GetId(&wstrId);
             std::wstring deviceId = wstrId ? wstrId : L"Unknown";
             if (wstrId) CoTaskMemFree(wstrId);

            if (mute) {
                // MUTE OPERATION
                float currentVol = 0.0f;
                pEndpointVolume->GetMasterVolumeLevelScalar(&currentVol);
                
                if (currentVol > 0.01f) {
                    RegWriteVolume(deviceId, currentVol);
                }
                
                pEndpointVolume->SetMasterVolumeLevelScalar(0.0f, NULL);
                pEndpointVolume->SetMute(TRUE, NULL);
            } else {
                // UNMUTE OPERATION
                float savedVol = RegReadVolume(deviceId);
                if (savedVol < 0.0f || savedVol > 1.0f) savedVol = 1.0f;
                
                pEndpointVolume->SetMasterVolumeLevelScalar(savedVol, NULL);
                pEndpointVolume->SetMute(FALSE, NULL);
            }
            
            isMutedGlobal = mute;
        } else {
            // Try updating devices and retry?
            // simplified: just ignored if no device
        }
        return mute;
    }

    bool CheckIfMuted() {
        std::lock_guard<std::mutex> lock(deviceMutex);
        // Only refresh if we lost device or startup
        if (!pEndpointVolume) UpdateDevices(); 
        
        if (pEndpointVolume) {
            BOOL bMute;
            float fVol;
            pEndpointVolume->GetMute(&bMute);
            pEndpointVolume->GetMasterVolumeLevelScalar(&fVol);
            bool result = (bMute == TRUE) || (fVol < 0.01f);
            isMutedGlobal = result;
            return result;
        }
        return false;
    }
};

// ==========================================
// Global Wrapper Functions
// ==========================================
// Singleton instance
static std::unique_ptr<AudioManager> g_Audio;

void InitializeAudio() {
    if (!g_Audio) {
        g_Audio = std::make_unique<AudioManager>();
        g_Audio->Start();
        // Initial state check
        g_Audio->CheckIfMuted();
    }
}

void UninitializeAudio() {
    if (g_Audio) {
        g_Audio->Stop();
        g_Audio.reset();
    }
}

bool ToggleMuteAll() {
    if (!g_Audio) return false;
    bool current = g_Audio->GetMuteState();
    return g_Audio->SetMute(!current);
}

void SetMuteAll(bool mute) {
    if (g_Audio) g_Audio->SetMute(mute);
}

bool IsAnyMicMuted() {
    if (g_Audio) return g_Audio->GetMuteState();
    return false;
}

bool IsDefaultMicMuted() {
    // If we want real-time hardware check:
    if (g_Audio) return g_Audio->CheckIfMuted();
    return false;
}

float GetMicLevel() {
    if (g_Audio) return g_Audio->GetCurrentLevel();
    return 0.0f;
}

float GetSpeakerLevel() {
    if (g_Audio) return g_Audio->GetCurrentSpeakerLevel();
    return 0.0f;
}

std::wstring GetMicDeviceName() {
    if (g_Audio) return g_Audio->GetDeviceName();
    return L"No Audio System";
}
