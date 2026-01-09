#include "audio.h"
#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <map>
#include <Audioclient.h>
#include <audiopolicy.h>

// GUID for IAudioMeterInformation if not defined
DEFINE_GUID(IID_IAudioMeterInformation, 0xC02216F6, 0x8C67, 0x4B5B, 0x9D, 0x00, 0xD0, 0x08, 0xE7, 0x3E, 0x00, 0x64);

// Debug logging helper
void DebugLog(const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, format, args);
    va_end(args);
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
}

// Store original volumes to restore later
static std::map<UINT, float> originalVolumes;
static bool isMutedByVolume = false;

void InitializeAudio() {
    CoInitialize(NULL);
    DebugLog("Audio initialized");
}

void UninitializeAudio() {
    CoUninitialize();
}

// NEW APPROACH: Set volume to 0 instead of using SetMute
// This bypasses software that monitors only the mute state
void ApplyVolumeToCollection(IMMDeviceCollection* pCollection, bool mute) {
    UINT count = 0;
    pCollection->GetCount(&count);
    DebugLog("ApplyVolumeToCollection: Found %d devices, mute=%d", count, mute);

    for (UINT i = 0; i < count; i++) {
        IMMDevice* pDevice = NULL;
        pCollection->Item(i, &pDevice);
        if (pDevice) {
            IAudioEndpointVolume* pEndpointVolume = NULL;
            pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pEndpointVolume);
            if (pEndpointVolume) {
                if (mute) {
                    // Save current volume and set to 0
                    float currentVolume = 0.0f;
                    pEndpointVolume->GetMasterVolumeLevelScalar(&currentVolume);
                    
                    // Only save if not already 0 (to preserve original volume)
                    if (currentVolume > 0.01f) {
                        originalVolumes[i] = currentVolume;
                        DebugLog("  Device %d: Saved original volume %.2f", i, currentVolume);
                    }
                    
                    // Set volume to 0
                    HRESULT hr = pEndpointVolume->SetMasterVolumeLevelScalar(0.0f, NULL);
                    DebugLog("  Device %d: SetVolume(0) returned hr=0x%08X", i, hr);
                    
                    // Also try SetMute as backup
                    pEndpointVolume->SetMute(TRUE, NULL);
                } else {
                    // Restore original volume
                    float restoreVolume = 1.0f;  // Default to full if we don't have saved
                    if (originalVolumes.find(i) != originalVolumes.end()) {
                        restoreVolume = originalVolumes[i];
                    }
                    
                    HRESULT hr = pEndpointVolume->SetMasterVolumeLevelScalar(restoreVolume, NULL);
                    DebugLog("  Device %d: SetVolume(%.2f) returned hr=0x%08X", i, restoreVolume, hr);
                    
                    // Also unmute
                    pEndpointVolume->SetMute(FALSE, NULL);
                }
                
                // Verify
                Sleep(50);
                float newVolume = 0.0f;
                pEndpointVolume->GetMasterVolumeLevelScalar(&newVolume);
                DebugLog("  Device %d: After set, actual volume = %.2f", i, newVolume);
                
                pEndpointVolume->Release();
            }
            pDevice->Release();
        }
    }
}

// Set Mute for ALL Capture devices using volume approach
void SetMuteAll(bool mute) {
    DebugLog("SetMuteAll called with mute=%d (using volume approach)", mute);
    IMMDeviceEnumerator* pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    
    if (SUCCEEDED(hr)) {
        IMMDeviceCollection* pCollection = NULL;
        pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
        if (pCollection) {
            ApplyVolumeToCollection(pCollection, mute);
            pCollection->Release();
        }
        pEnumerator->Release();
        isMutedByVolume = mute;
    } else {
        DebugLog("SetMuteAll: Failed to create device enumerator, hr=0x%08X", hr);
    }
}

// Check if mic is muted (by volume = 0 OR actual mute)
bool IsDefaultMicMuted() {
    bool isMuted = false;
    IMMDeviceEnumerator* pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    
    if (SUCCEEDED(hr)) {
        IMMDevice* pDevice = NULL;
        pEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pDevice);
        if (pDevice) {
            IAudioEndpointVolume* pEndpointVolume = NULL;
            pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pEndpointVolume);
            if (pEndpointVolume) {
                // Check both mute state AND volume level
                BOOL muted = FALSE;
                pEndpointVolume->GetMute(&muted);
                
                float volume = 1.0f;
                pEndpointVolume->GetMasterVolumeLevelScalar(&volume);
                
                // Consider muted if: actual mute OR volume near zero OR we set it
                isMuted = muted || (volume < 0.01f) || isMutedByVolume;
                
                DebugLog("IsDefaultMicMuted: mute=%d, volume=%.2f, isMutedByVolume=%d, returning %d", 
                         muted, volume, isMutedByVolume, isMuted);
                
                pEndpointVolume->Release();
            }
            pDevice->Release();
        }
        pEnumerator->Release();
    }
    return isMuted;
}

// Toggles mute state
bool ToggleMuteAll() {
    DebugLog("=== ToggleMuteAll START ===");
    
    // Use our internal state instead of checking actual mute
    bool newMute = !isMutedByVolume;
    DebugLog("ToggleMuteAll: isMutedByVolume=%d, will set to newMute=%d", isMutedByVolume, newMute);
    
    SetMuteAll(newMute);
    
    DebugLog("=== ToggleMuteAll END ===");
    return newMute;
}

bool IsAnyMicMuted() {
    return isMutedByVolume || IsDefaultMicMuted();
}

// Get microphone audio level (0.0 to 1.0)
float GetMicLevel() {
    float level = 0.0f;
    IMMDeviceEnumerator* pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, 
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    
    if (SUCCEEDED(hr) && pEnumerator) {
        IMMDevice* pDevice = NULL;
        hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pDevice);
        if (SUCCEEDED(hr) && pDevice) {
            IAudioMeterInformation* pMeter = NULL;
            hr = pDevice->Activate(IID_IAudioMeterInformation, CLSCTX_ALL, NULL, (void**)&pMeter);
            if (SUCCEEDED(hr) && pMeter) {
                hr = pMeter->GetPeakValue(&level);
                pMeter->Release();
            }
            pDevice->Release();
        }
        pEnumerator->Release();
    }
    return level;
}

