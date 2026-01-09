#include "audio.h"
#include <iostream>
#include <windows.h>
#include <stdio.h>

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

void InitializeAudio() {
    CoInitialize(NULL);
    DebugLog("Audio initialized");
}

void UninitializeAudio() {
    CoUninitialize();
}

// Helper to apply mute state to a device collection
void ApplyMuteToCollection(IMMDeviceCollection* pCollection, bool mute) {
    UINT count = 0;
    pCollection->GetCount(&count);
    DebugLog("ApplyMuteToCollection: Found %d devices, setting mute=%d", count, mute);

    for (UINT i = 0; i < count; i++) {
        IMMDevice* pDevice = NULL;
        pCollection->Item(i, &pDevice);
        if (pDevice) {
            IAudioEndpointVolume* pEndpointVolume = NULL;
            pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pEndpointVolume);
            if (pEndpointVolume) {
                HRESULT hr = pEndpointVolume->SetMute(mute, NULL);
                DebugLog("  Device %d: SetMute(%d) returned hr=0x%08X", i, mute, hr);
                if (SUCCEEDED(hr)) {
                    // Small delay to let Windows process the state change
                    Sleep(50);
                    // Verify mute was actually set
                    BOOL actualMute = FALSE;
                    pEndpointVolume->GetMute(&actualMute);
                    DebugLog("  Device %d: After SetMute, actual mute state = %d", i, actualMute);
                }
                pEndpointVolume->Release();
            }
            pDevice->Release();
        }
    }
}

// Set Mute for ALL Capture devices
void SetMuteAll(bool mute) {
    DebugLog("SetMuteAll called with mute=%d", mute);
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
    } else {
        DebugLog("SetMuteAll: Failed to create device enumerator, hr=0x%08X", hr);
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
    DebugLog("IsDefaultMicMuted: returning %d", isMuted);
    return isMuted;
}

// Toggles based on the DEFAULT microphone state, then applies that new state to ALL microphones.
bool ToggleMuteAll() {
    DebugLog("=== ToggleMuteAll START ===");
    bool currentMute = IsDefaultMicMuted();
    bool newMute = !currentMute;
    DebugLog("ToggleMuteAll: currentMute=%d, will set to newMute=%d", currentMute, newMute);
    SetMuteAll(newMute);
    
    // Verify after setting
    Sleep(100);
    bool verifyMute = IsDefaultMicMuted();
    DebugLog("ToggleMuteAll: After 100ms, verified mute state = %d", verifyMute);
    DebugLog("=== ToggleMuteAll END ===");
    return newMute;
}

bool IsAnyMicMuted() {
    return IsDefaultMicMuted();
}

