#include "audio.h"
#include <iostream>

void InitializeAudio() {
    CoInitialize(NULL);
}

void UninitializeAudio() {
    CoUninitialize();
}

// Helper to apply mute state to a device collection
void ApplyMuteToCollection(IMMDeviceCollection* pCollection, bool mute) {
    UINT count = 0;
    pCollection->GetCount(&count);

    for (UINT i = 0; i < count; i++) {
        IMMDevice* pDevice = NULL;
        pCollection->Item(i, &pDevice);
        if (pDevice) {
            IAudioEndpointVolume* pEndpointVolume = NULL;
            pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pEndpointVolume);
            if (pEndpointVolume) {
                HRESULT hr = pEndpointVolume->SetMute(mute, NULL);
                if (SUCCEEDED(hr)) {
                    // Small delay to let Windows process the state change
                    Sleep(50);
                }
                pEndpointVolume->Release();
            }
            pDevice->Release();
        }
    }
}

// Set Mute for ALL Capture devices
void SetMuteAll(bool mute) {
    IMMDeviceEnumerator* pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    
    if (SUCCEEDED(hr)) {
        IMMDeviceCollection* pCollection = NULL;
        // Enumerate ACTIVE capture devices
        pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
        if (pCollection) {
            ApplyMuteToCollection(pCollection, mute);
            pCollection->Release();
        }
        pEnumerator->Release();
    }
}

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
                BOOL muted = FALSE;
                pEndpointVolume->GetMute(&muted);
                isMuted = muted ? true : false;
                pEndpointVolume->Release();
            }
            pDevice->Release();
        }
        pEnumerator->Release();
    }
    return isMuted;
}

// Toggles based on the DEFAULT microphone state, then applies that new state to ALL microphones.
bool ToggleMuteAll() {
    bool currentMute = IsDefaultMicMuted();
    bool newMute = !currentMute;
    SetMuteAll(newMute);
    return newMute;
}

bool IsAnyMicMuted() {
    return IsDefaultMicMuted();
}
