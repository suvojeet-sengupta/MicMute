#include "WasapiRecorder.h"
#include <fstream>
#include <iostream>
#include <mmreg.h>
#include <ksmedia.h>
#include <functiondiscoverykeys_devpkey.h>
#include <cstdio>
#include <cmath>

#pragma comment(lib, "ole32.lib")

// Helper template for COM cleanup
template <class T> void SafeRelease(T **ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

WasapiRecorder::WasapiRecorder() : isRecording(false), isPaused(false), pwfxMic(NULL), pwfxLoopback(NULL) {
}

WasapiRecorder::~WasapiRecorder() {
    Stop();
    if (pwfxMic) CoTaskMemFree(pwfxMic);
    if (pwfxLoopback) CoTaskMemFree(pwfxLoopback);
}

bool WasapiRecorder::Start() {
    if (isRecording) {
        if (isPaused) {
            Resume();
            return true;
        }
        return false; // Already running
    }

    // Clear previous buffers if starting fresh
    Clear();

    isRecording = true;
    isPaused = false;
    
    // Start both capture threads
    micThread = std::thread(&WasapiRecorder::MicrophoneLoop, this);
    loopbackThread = std::thread(&WasapiRecorder::LoopbackLoop, this);
    
    return true;
}

void WasapiRecorder::Stop() {
    isRecording = false; // Signals threads to stop
    isPaused = false;
    
    if (micThread.joinable()) {
        micThread.join();
    }
    if (loopbackThread.joinable()) {
        loopbackThread.join();
    }
}

void WasapiRecorder::Pause() {
    if (isRecording) isPaused = true;
}

void WasapiRecorder::Resume() {
    if (isRecording) isPaused = false;
}

void WasapiRecorder::Clear() {
    {
        std::lock_guard<std::mutex> lock(micBufferMutex);
        micBuffer.clear();
    }
    {
        std::lock_guard<std::mutex> lock(loopbackBufferMutex);
        loopbackBuffer.clear();
    }
}

// Microphone capture loop (user's voice)
void WasapiRecorder::MicrophoneLoop() {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioClient *pAudioClient = NULL;
    IAudioCaptureClient *pCaptureClient = NULL;

    CoInitialize(NULL);

    // Get Default Capture Device (Microphone)
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, 
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) goto Exit;

    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pDevice);
    if (FAILED(hr)) goto Exit;

    // Log Device Name
    {
        IPropertyStore *pProps = NULL;
        if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
            PROPVARIANT varName; PropVariantInit(&varName);
            if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))) {
                char buffer[512];
                snprintf(buffer, sizeof(buffer), "[WasapiRecorder] Mic Device: %ws\n", varName.pwszVal);
                OutputDebugStringA(buffer);
                PropVariantClear(&varName);
            }
            pProps->Release();
        }
    }

    // Activate Audio Client
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) goto Exit;

    // Get Mix Format
    if (pwfxMic) CoTaskMemFree(pwfxMic);
    hr = pAudioClient->GetMixFormat(&pwfxMic);
    if (FAILED(hr)) goto Exit;

    // Initialize
    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, pwfxMic, NULL);
    if (FAILED(hr)) goto Exit;

    // Get Capture Service
    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    if (FAILED(hr)) goto Exit;

    // Start
    hr = pAudioClient->Start();
    if (FAILED(hr)) goto Exit;

    UINT32 packetLength = 0;
    UINT32 numFramesAvailable;
    BYTE *pData;
    DWORD flags;

    while (isRecording) {
        if (isPaused) {
            Sleep(10);
            continue;
        }

        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        while (packetLength != 0) {
            hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
            if (FAILED(hr)) break;

            std::lock_guard<std::mutex> lock(micBufferMutex);
            int bytesToCopy = numFramesAvailable * pwfxMic->nBlockAlign;
            size_t currentSize = micBuffer.size();
            micBuffer.resize(currentSize + bytesToCopy);
            
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                memset(micBuffer.data() + currentSize, 0, bytesToCopy);
            } else {
                memcpy(micBuffer.data() + currentSize, pData, bytesToCopy);
            }

            pCaptureClient->ReleaseBuffer(numFramesAvailable);
            hr = pCaptureClient->GetNextPacketSize(&packetLength);
        }
        
        Sleep(10);
    }

    pAudioClient->Stop();

Exit:
    SafeRelease(&pCaptureClient);
    SafeRelease(&pAudioClient);
    SafeRelease(&pDevice);
    SafeRelease(&pEnumerator);
    CoUninitialize();
}

// Loopback capture loop (system audio - CX voice)
void WasapiRecorder::LoopbackLoop() {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioClient *pAudioClient = NULL;
    IAudioCaptureClient *pCaptureClient = NULL;

    CoInitialize(NULL);

    // Get Default RENDER Device (Speaker/Output) for loopback
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, 
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) goto Exit;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    if (FAILED(hr)) goto Exit;

    // Log Device Name
    {
        IPropertyStore *pProps = NULL;
        if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
            PROPVARIANT varName; PropVariantInit(&varName);
            if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))) {
                char buffer[512];
                snprintf(buffer, sizeof(buffer), "[WasapiRecorder] Loopback Device: %ws\n", varName.pwszVal);
                OutputDebugStringA(buffer);
                PropVariantClear(&varName);
            }
            pProps->Release();
        }
    }

    // Activate Audio Client
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) goto Exit;

    // Get Mix Format
    if (pwfxLoopback) CoTaskMemFree(pwfxLoopback);
    hr = pAudioClient->GetMixFormat(&pwfxLoopback);
    if (FAILED(hr)) goto Exit;

    // Initialize with LOOPBACK flag - this captures what's being played
    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0, pwfxLoopback, NULL);
    if (FAILED(hr)) goto Exit;

    // Get Capture Service
    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    if (FAILED(hr)) goto Exit;

    // Start
    hr = pAudioClient->Start();
    if (FAILED(hr)) goto Exit;

    UINT32 packetLength = 0;
    UINT32 numFramesAvailable;
    BYTE *pData;
    DWORD flags;

    while (isRecording) {
        if (isPaused) {
            Sleep(10);
            continue;
        }

        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        while (packetLength != 0) {
            hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
            if (FAILED(hr)) break;

            std::lock_guard<std::mutex> lock(loopbackBufferMutex);
            int bytesToCopy = numFramesAvailable * pwfxLoopback->nBlockAlign;
            size_t currentSize = loopbackBuffer.size();
            loopbackBuffer.resize(currentSize + bytesToCopy);
            
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                memset(loopbackBuffer.data() + currentSize, 0, bytesToCopy);
            } else {
                memcpy(loopbackBuffer.data() + currentSize, pData, bytesToCopy);
            }

            pCaptureClient->ReleaseBuffer(numFramesAvailable);
            hr = pCaptureClient->GetNextPacketSize(&packetLength);
        }
        
        Sleep(10);
    }

    pAudioClient->Stop();

Exit:
    SafeRelease(&pCaptureClient);
    SafeRelease(&pAudioClient);
    SafeRelease(&pDevice);
    SafeRelease(&pEnumerator);
    CoUninitialize();
}

// Helper: Convert float sample to 16-bit PCM
static inline short FloatToShort(float sample) {
    sample = sample * 32767.0f;
    if (sample > 32767.0f) sample = 32767.0f;
    if (sample < -32768.0f) sample = -32768.0f;
    return (short)sample;
}

// Mix both buffers into stereo output (mic=left+right mixed with loopback)
std::vector<BYTE> WasapiRecorder::MixBuffers() {
    std::vector<BYTE> output;
    
    // Lock both buffers
    std::lock_guard<std::mutex> lockMic(micBufferMutex);
    std::lock_guard<std::mutex> lockLoop(loopbackBufferMutex);
    
    if (!pwfxMic || !pwfxLoopback) return output;
    
    // Calculate number of samples in each buffer
    bool micIsFloat = false;
    bool loopIsFloat = false;
    
    if (pwfxMic->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) micIsFloat = true;
    if (pwfxMic->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE *pEx = (WAVEFORMATEXTENSIBLE*)pwfxMic;
        if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat)) micIsFloat = true;
    }
    
    if (pwfxLoopback->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) loopIsFloat = true;
    if (pwfxLoopback->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE *pEx = (WAVEFORMATEXTENSIBLE*)pwfxLoopback;
        if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat)) loopIsFloat = true;
    }
    
    // Calculate frame counts
    size_t micFrames = micBuffer.size() / pwfxMic->nBlockAlign;
    size_t loopFrames = loopbackBuffer.size() / pwfxLoopback->nBlockAlign;
    size_t maxFrames = (micFrames > loopFrames) ? micFrames : loopFrames;
    
    // Output: 16-bit stereo
    output.resize(maxFrames * 4); // 2 channels * 2 bytes per sample
    short* outPtr = (short*)output.data();
    
    for (size_t i = 0; i < maxFrames; i++) {
        float micSample = 0.0f;
        float loopSample = 0.0f;
        
        // Get mic sample (mono mix of all channels)
        if (i < micFrames) {
            if (micIsFloat) {
                float* micPtr = (float*)(micBuffer.data() + i * pwfxMic->nBlockAlign);
                for (int ch = 0; ch < pwfxMic->nChannels; ch++) {
                    micSample += micPtr[ch];
                }
                micSample /= pwfxMic->nChannels;
            } else {
                short* micPtr = (short*)(micBuffer.data() + i * pwfxMic->nBlockAlign);
                for (int ch = 0; ch < pwfxMic->nChannels; ch++) {
                    micSample += micPtr[ch] / 32768.0f;
                }
                micSample /= pwfxMic->nChannels;
            }
        }
        
        // Get loopback sample (mono mix of all channels)
        if (i < loopFrames) {
            if (loopIsFloat) {
                float* loopPtr = (float*)(loopbackBuffer.data() + i * pwfxLoopback->nBlockAlign);
                for (int ch = 0; ch < pwfxLoopback->nChannels; ch++) {
                    loopSample += loopPtr[ch];
                }
                loopSample /= pwfxLoopback->nChannels;
            } else {
                short* loopPtr = (short*)(loopbackBuffer.data() + i * pwfxLoopback->nBlockAlign);
                for (int ch = 0; ch < pwfxLoopback->nChannels; ch++) {
                    loopSample += loopPtr[ch] / 32768.0f;
                }
                loopSample /= pwfxLoopback->nChannels;
            }
        }
        
        // Mix both into stereo (both sources in both channels)
        float mixed = (micSample + loopSample) * 0.5f;
        short outSample = FloatToShort(mixed);
        
        outPtr[i * 2] = outSample;      // Left
        outPtr[i * 2 + 1] = outSample;  // Right
    }
    
    return output;
}

void WasapiRecorder::WriteWavHeader(std::ofstream& file, int totalDataLen, int sampleRate, int channels, int bitsPerSample) {
    int byteRate = sampleRate * channels * bitsPerSample / 8;
    int blockAlign = channels * bitsPerSample / 8;
    
    file.write("RIFF", 4);
    int chunkSize = 36 + totalDataLen;
    file.write((char*)&chunkSize, 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    int subChunk1Size = 16;
    file.write((char*)&subChunk1Size, 4);
    
    short audioFormat = 1; // PCM
    file.write((char*)&audioFormat, 2);
    file.write((char*)&channels, 2);
    file.write((char*)&sampleRate, 4);
    file.write((char*)&byteRate, 4);
    file.write((char*)&blockAlign, 2);
    file.write((char*)&bitsPerSample, 2);
    
    file.write("data", 4);
    file.write((char*)&totalDataLen, 4);
}

bool WasapiRecorder::SaveToFile(const std::string& filename) {
    // Mix both buffers
    std::vector<BYTE> mixedAudio = MixBuffers();
    
    if (mixedAudio.empty()) return false;

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    // Use output format: 48kHz stereo 16-bit (or use actual sample rate from loopback)
    int sampleRate = pwfxLoopback ? pwfxLoopback->nSamplesPerSec : OUTPUT_SAMPLE_RATE;
    
    WriteWavHeader(file, (int)mixedAudio.size(), sampleRate, OUTPUT_CHANNELS, OUTPUT_BITS);
    
    file.write((char*)mixedAudio.data(), mixedAudio.size());
    file.close();
    return true;
}
