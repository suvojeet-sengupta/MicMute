#include "audio.h"
#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <map>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

// Link against required libs
#pragma comment(lib, "ole32.lib")

// ==========================================
// Utils: Safe COM Pointer
// ==========================================
template <class T>
class SafeComPtr {
public:
    T* ptr;
    SafeComPtr() : ptr(NULL) {}
    SafeComPtr(T* p) : ptr(p) { if(ptr) ptr->AddRef(); }
    ~SafeComPtr() { Release(); }
    
    void Release() {
        if (ptr) {
            ptr->Release();
            ptr = NULL;
        }
    }
    
    T** operator&() { Release(); return &ptr; }
    T* operator->() { return ptr; }
    operator bool() { return ptr != NULL; }
    bool operator!() { return ptr == NULL; }
};

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
// AudioManager Class
// ==========================================
class AudioManager {
private:
    SafeComPtr<IMMDeviceEnumerator> pEnumerator;
    SafeComPtr<IMMDevice> pDefaultDevice;
    SafeComPtr<IAudioMeterInformation> pMeterInfo;
    
    std::atomic<float> currentPeakLevel;
    std::atomic<bool> isMutedGlobal;
    std::mutex deviceMutex;
    
    std::thread pollerThread;
    std::atomic<bool> stopThread;

    void InitializeCOM() {
        if (!pEnumerator) {
            CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, 
                __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
        }
    }

    // Returns true if successful
    bool UpdateDefaultDevice() {
        std::lock_guard<std::mutex> lock(deviceMutex);
        InitializeCOM();
        if (!pEnumerator) return false;

        // Try to get default device
        SafeComPtr<IMMDevice> pNewDevice;
        HRESULT hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pNewDevice);
        
        if (FAILED(hr)) {
            // Device might be lost
            pDefaultDevice.Release();
            pMeterInfo.Release();
            return false;
        }

        // Check if device changed (simple check: if we didn't have one before)
        // ideally we check ID, but for now just ensure we have a valid pDevice
        if (!pDefaultDevice) {
            pDefaultDevice = pNewDevice.ptr;
            pDefaultDevice.ptr->AddRef(); // Manually AddRef since we copied pointer
        }
        
        // Refresh Meter Info if needed
        if (pDefaultDevice && !pMeterInfo) {
            pDefaultDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, NULL, (void**)&pMeterInfo);
        }

        return (pDefaultDevice && pMeterInfo);
    }

    void PollerLoop() {
        CoInitialize(NULL);
        while (!stopThread) {
            if (UpdateDefaultDevice()) {
                float peak = 0.0f;
                // Thread-safe lock not strictly needed for just calling GetPeakValue 
                // on a localized COM interface, but good practice if we swap pMeterInfo
                std::lock_guard<std::mutex> lock(deviceMutex);
                if (pMeterInfo) {
                    pMeterInfo->GetPeakValue(&peak);
                }
                currentPeakLevel = peak;
            } else {
                currentPeakLevel = 0.0f;
            }
            Sleep(50); // Poll every 50ms
        }
        
        // Cleanup thread-local COM
        {
            std::lock_guard<std::mutex> lock(deviceMutex);
            pMeterInfo.Release();
            pDefaultDevice.Release();
            pEnumerator.Release();
        }
        CoUninitialize();
    }

public:
    AudioManager() : currentPeakLevel(0.0f), isMutedGlobal(false), stopThread(false) {}

    void Start() {
        stopThread = false;
        pollerThread = std::thread(&AudioManager::PollerLoop, this);
    }

    void Stop() {
        stopThread = true;
        if (pollerThread.joinable()) {
            pollerThread.join();
        }
    }

    float GetCurrentLevel() {
        return currentPeakLevel;
    }

    bool GetMuteState() {
        // We trust our internal state first, but verifying with hardware is good occasionally
        // For high freq calls, return internal state
        return isMutedGlobal;
    }
    
    // Mute/Unmute Logic with persistence
    // Returns new mute state
    bool SetMute(bool mute) {
        // This runs on MAIN THREAD usually (triggered by user action)
        // So we need to init COM on this thread if not already
        // But we shouldn't share COM objects across threads without marshalling.
        // EASIEST: Just create a temporary enumerator here. 
        // Logic changes infrequently, so creation overhead is acceptable here (unlike GetMicLevel).
        
        HRESULT hr;
        IMMDeviceEnumerator* pEnum = NULL;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, 
            __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
            
        if (FAILED(hr)) return isMutedGlobal;

        IMMDeviceCollection* pCollection = NULL;
        pEnum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
        
        if (pCollection) {
            UINT count = 0;
            pCollection->GetCount(&count);
            
            for (UINT i = 0; i < count; i++) {
                IMMDevice* pDevice = NULL;
                pCollection->Item(i, &pDevice);
                if (pDevice) {
                    LPWSTR wstrId = NULL;
                    pDevice->GetId(&wstrId);
                    std::wstring deviceId = wstrId ? wstrId : L"Unknown";
                    if (wstrId) CoTaskMemFree(wstrId);

                    IAudioEndpointVolume* pVolume = NULL;
                    pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pVolume);
                    
                    if (pVolume) {
                        if (mute) {
                            // MUTE OPERATION
                            float currentVol = 0.0f;
                            pVolume->GetMasterVolumeLevelScalar(&currentVol);
                            
                            // Save if > 0
                            if (currentVol > 0.01f) {
                                RegWriteVolume(deviceId, currentVol);
                            }
                            
                            // Mute using Volume 0 AND Native Mute
                            pVolume->SetMasterVolumeLevelScalar(0.0f, NULL);
                            pVolume->SetMute(TRUE, NULL);
                            
                        } else {
                            // UNMUTE OPERATION
                            float savedVol = RegReadVolume(deviceId);
                            
                            // Safety clamp: if not found or weird, default to 50%
                            if (savedVol < 0.0f || savedVol > 1.0f) savedVol = 0.5f;
                            
                            // Set Volume
                            pVolume->SetMasterVolumeLevelScalar(savedVol, NULL);
                            pVolume->SetMute(FALSE, NULL);
                        }
                        pVolume->Release();
                    }
                    pDevice->Release();
                }
            }
            pCollection->Release();
        }
        
        if (pEnum) pEnum->Release();
        
        isMutedGlobal = mute;
        return mute;
    }

    bool CheckIfMuted() {
         // Check actual hardware state of default device
        HRESULT hr;
        IMMDeviceEnumerator* pEnum = NULL;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, 
            __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
            
        bool result = false;
        if (SUCCEEDED(hr)) {
            IMMDevice* pDevice = NULL;
            pEnum->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pDevice);
            if (pDevice) {
                IAudioEndpointVolume* pVol = NULL;
                pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pVol);
                if (pVol) {
                    BOOL bMute;
                    float fVol;
                    pVol->GetMute(&bMute);
                    pVol->GetMasterVolumeLevelScalar(&fVol);
                    // Muted if switch is on OR volume is zero
                    result = (bMute == TRUE) || (fVol < 0.01f);
                    pVol->Release();
                }
                pDevice->Release();
            }
            pEnum->Release();
        }
        
        // Sync global state
        isMutedGlobal = result;
        return result;
    }
};

// ==========================================
// Global Wrapper Functions
// ==========================================
// Singleton instance
static AudioManager* g_Audio = NULL;

void InitializeAudio() {
    if (!g_Audio) {
        g_Audio = new AudioManager();
        g_Audio->Start();
        // Initial state check
        g_Audio->CheckIfMuted();
    }
}

void UninitializeAudio() {
    if (g_Audio) {
        g_Audio->Stop();
        delete g_Audio;
        g_Audio = NULL;
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
