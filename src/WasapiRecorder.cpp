#include "WasapiRecorder.h"
#include <fstream>
#include <iostream>
#include <mmreg.h>
#include <ksmedia.h>

#pragma comment(lib, "ole32.lib")

// Helper template for COM cleanup
template <class T> void SafeRelease(T **ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

WasapiRecorder::WasapiRecorder() : isRecording(false), isPaused(false), pwfx(NULL) {
}

WasapiRecorder::~WasapiRecorder() {
    Stop();
    if (pwfx) CoTaskMemFree(pwfx);
}

bool WasapiRecorder::Start() {
    if (isRecording) {
        if (isPaused) {
            Resume();
            return true;
        }
        return false; // Already running
    }

    // Clear previous buffer if starting fresh
    Clear();

    isRecording = true;
    isPaused = false;
    recordingThread = std::thread(&WasapiRecorder::RecordingLoop, this);
    return true;
}

void WasapiRecorder::Stop() {
    isRecording = false; // Signals thread to stop
    isPaused = false;
    if (recordingThread.joinable()) {
        recordingThread.join();
    }
}

void WasapiRecorder::Pause() {
    if (isRecording) isPaused = true;
}

void WasapiRecorder::Resume() {
    if (isRecording) isPaused = false;
}

void WasapiRecorder::Clear() {
    std::lock_guard<std::mutex> lock(bufferMutex);
    audioBuffer.clear();
}

void WasapiRecorder::RecordingLoop() {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioClient *pAudioClient = NULL;
    IAudioCaptureClient *pCaptureClient = NULL;

    CoInitialize(NULL);

    // 1. Get Default Capture Device
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, 
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) goto Exit;

    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDevice);
    if (FAILED(hr)) goto Exit;

    // 2. Activate Audio Client
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) goto Exit;

    // 3. Get Mix Format
    if (pwfx) CoTaskMemFree(pwfx);
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) goto Exit;

    // Force 16-bit PCM if float (common in WASAPI) to make WAV saving easier
    if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE *pEx = (WAVEFORMATEXTENSIBLE*)pwfx;
        if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat)) {
             // We'll stick to the mix format but we might need to convert later. 
             // Actually, simplest is to request the mix format and save it as is (often float), 
             // OR try to initialize with 16-bit PCM.
             // Let's try to constrain it to 16-bit PCM 44.1kHz Stereo for high compat.
             // However, WASAPI Shared Mode is picky. It usually only accepts the Mix Format.
             // So we will record in Mix Format and rely on that.
             // Most modern Windows defaults to Float.
        }
    }

    // 4. Initialize
    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, pwfx, NULL);
    if (FAILED(hr)) goto Exit;

    // 5. Get Capture Service
    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    if (FAILED(hr)) goto Exit;

    // 6. Start
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

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                // Write silence
                 // For now, we just skip or write 0s. 
                 // To implement properly we'd push 0s.
            } else {
                // Copy data
                std::lock_guard<std::mutex> lock(bufferMutex);
                int bytesToCopy = numFramesAvailable * pwfx->nBlockAlign;
                size_t currentSize = audioBuffer.size();
                audioBuffer.resize(currentSize + bytesToCopy);
                memcpy(audioBuffer.data() + currentSize, pData, bytesToCopy);
            }

            pCaptureClient->ReleaseBuffer(numFramesAvailable);
            hr = pCaptureClient->GetNextPacketSize(&packetLength);
        }
        
        Sleep(10); // Prevent CPU spinning
    }

    // Stop
    pAudioClient->Stop();

Exit:
    SafeRelease(&pCaptureClient);
    SafeRelease(&pAudioClient);
    SafeRelease(&pDevice);
    SafeRelease(&pEnumerator);
    CoUninitialize();
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
    
    short audioFormat = (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) ? 65534 : 1; // PCM = 1, Float/Ext = 3/65534...
    // If it's float, we should probably check. But let's assume we just dump what we got.
    // If we are dumping Float32, format is 3 usually (IEEE Float).
    if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) audioFormat = 3;
    if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
         WAVEFORMATEXTENSIBLE *pEx = (WAVEFORMATEXTENSIBLE*)pwfx;
         if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat)) audioFormat = 3;
         else audioFormat = 1;
    }
    
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
    if (!pwfx) return false;

    std::lock_guard<std::mutex> lock(bufferMutex);
    if (audioBuffer.empty()) return false;

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    // Use actual format from pwfx
    WriteWavHeader(file, (int)audioBuffer.size(), pwfx->nSamplesPerSec, pwfx->nChannels, pwfx->wBitsPerSample);
    
    file.write((char*)audioBuffer.data(), audioBuffer.size());
    file.close();
    return true;
}
