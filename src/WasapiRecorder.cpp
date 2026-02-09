#include "WasapiRecorder.h"
#include "StreamingWavWriter.h"
#include "recorder.h" // For hRecorderWnd and WM_APP_RECORDING_ERROR
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
        *ppT = nullptr;
    }
}

WasapiRecorder::WasapiRecorder() 
    : isRecording(false)
    , isPaused(false)
    , m_streamingMode(false)
    , pwfxMic(nullptr)
    , pwfxLoopback(nullptr)
    , m_pWriter(nullptr)
    , m_lastFlushTime(0)
{
}

WasapiRecorder::~WasapiRecorder() {
    Stop();
    if (pwfxMic) CoTaskMemFree(pwfxMic);
    if (pwfxLoopback) CoTaskMemFree(pwfxLoopback);
    if (m_pWriter) {
        // Ensure writer is finalized or aborted if not already
        if (m_pWriter->IsActive()) {
             m_pWriter->Abort(); // Determine if we should finalize instead?
             // If we rely on Stop() being called before destruct, this is a fallback.
             // Best to abort to avoid partial files masquerading as good ones?
             // Or maybe we want to save what we have?
             // Safe bet: Stop() should be called explicitly.
             // If we are here, something went wrong, let's Abort to cleantemp.
             // but if it was valid data... 
             // Let's rely on Stop() being called.
             // But let's make sure we delete it.
        }
        delete m_pWriter;
        m_pWriter = nullptr;
    }
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
    m_streamingMode = false;
    
    // Start both capture threads
    micThread = std::thread(&WasapiRecorder::MicrophoneLoop, this);
    loopbackThread = std::thread(&WasapiRecorder::LoopbackLoop, this);
    
    return true;
}

bool WasapiRecorder::StartStreaming(const std::string& outputFolder) {
    if (isRecording) {
        if (isPaused) {
            Resume();
            return true;
        }
        return false; // Already running
    }

    // Clear previous buffers
    Clear();
    
    // Initialize streaming writer
    if (!m_pWriter) {
        m_pWriter = new StreamingWavWriter();
    }
    
    m_outputFolder = outputFolder;
    
    // Start the writer with output format
    if (!m_pWriter->Start(outputFolder, OUTPUT_SAMPLE_RATE, OUTPUT_CHANNELS, OUTPUT_BITS)) {
        OutputDebugStringA("[WasapiRecorder] Failed to start streaming writer\n");
        return false;
    }

    isRecording = true;
    isPaused = false;
    m_streamingMode = true;
    m_lastFlushTime = GetTickCount();
    
    // Start capture threads
    micThread = std::thread(&WasapiRecorder::MicrophoneLoop, this);
    loopbackThread = std::thread(&WasapiRecorder::LoopbackLoop, this);
    
    // Start mixer thread (periodically mixes and writes to disk)
    mixerThread = std::thread(&WasapiRecorder::MixerLoop, this);
    
    OutputDebugStringA("[WasapiRecorder] Started in STREAMING mode\n");
    return true;
}

void WasapiRecorder::Stop() {
    isRecording = false; // Signals threads to stop
    isPaused = false;
    
    // Wake up mixer thread if waiting
    m_mixerCV.notify_all();
    
    if (micThread.joinable()) {
        try { micThread.join(); } catch(...) {}
    }
    if (loopbackThread.joinable()) {
        try { loopbackThread.join(); } catch(...) {}
    }
    if (mixerThread.joinable()) {
         try { mixerThread.join(); } catch(...) {}
    }
    
    m_streamingMode = false;
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
    IMMDeviceEnumerator *pEnumerator = nullptr;
    IMMDevice *pDevice = nullptr;
    IAudioClient *pAudioClient = nullptr;
    IAudioCaptureClient *pCaptureClient = nullptr;

    CoInitialize(nullptr);

    UINT32 packetLength = 0;
    UINT32 numFramesAvailable = 0;
    BYTE *pData = nullptr;
    DWORD flags = 0;

    // Get Default Capture Device (Microphone)
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, 
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) goto Exit;

    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pDevice);
    if (FAILED(hr)) goto Exit;

    // Log Device Name
    {
        IPropertyStore *pProps = nullptr;
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
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
    if (FAILED(hr)) goto Exit;

    // Get Mix Format
    if (pwfxMic) CoTaskMemFree(pwfxMic);
    hr = pAudioClient->GetMixFormat(&pwfxMic);
    if (FAILED(hr)) goto Exit;

    // Initialize
    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, pwfxMic, nullptr);
    if (FAILED(hr)) goto Exit;

    // Get Capture Service
    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    if (FAILED(hr)) goto Exit;

    // Start
    hr = pAudioClient->Start();
    if (FAILED(hr)) goto Exit;

    while (isRecording) {
        if (isPaused) {
            Sleep(10);
            continue;
        }

        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        while (packetLength != 0) {
            hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
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
    if (FAILED(hr) && isRecording) {
        if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_RESOURCES_INVALIDATED) {
             OutputDebugStringA("[WasapiRecorder] Mic Disconnected/Invalidated!\n");
        } else {
             char errBuf[64];
             snprintf(errBuf, sizeof(errBuf), "[WasapiRecorder] Mic Error: 0x%08X\n", hr);
             OutputDebugStringA(errBuf);
        }
        
        if (hRecorderWnd) {
            PostMessage(hRecorderWnd, WM_APP_RECORDING_ERROR, 0, 0);
        }
    }
    SafeRelease(&pCaptureClient);
    SafeRelease(&pAudioClient);
    SafeRelease(&pDevice);
    SafeRelease(&pEnumerator);
    CoUninitialize();
}

// Loopback capture loop (system audio - CX voice)
void WasapiRecorder::LoopbackLoop() {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = nullptr;
    IMMDevice *pDevice = nullptr;
    IAudioClient *pAudioClient = nullptr;
    IAudioCaptureClient *pCaptureClient = nullptr;

    CoInitialize(nullptr);

    UINT32 packetLength = 0;
    UINT32 numFramesAvailable = 0;
    BYTE *pData = nullptr;
    DWORD flags = 0;

    // Get Default RENDER Device (Speaker/Output) for loopback
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, 
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) goto Exit;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    if (FAILED(hr)) goto Exit;

    // Log Device Name
    {
        IPropertyStore *pProps = nullptr;
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
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
    if (FAILED(hr)) goto Exit;

    // Get Mix Format
    if (pwfxLoopback) CoTaskMemFree(pwfxLoopback);
    hr = pAudioClient->GetMixFormat(&pwfxLoopback);
    if (FAILED(hr)) goto Exit;

    // Initialize with LOOPBACK flag - this captures what's being played
    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0, pwfxLoopback, nullptr);
    if (FAILED(hr)) goto Exit;

    // Get Capture Service
    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    if (FAILED(hr)) goto Exit;

    // Start
    hr = pAudioClient->Start();
    if (FAILED(hr)) goto Exit;

    while (isRecording) {
        if (isPaused) {
            Sleep(10);
            continue;
        }

        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        while (packetLength != 0) {
            hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
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
    if (FAILED(hr) && isRecording) {
         if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_RESOURCES_INVALIDATED) {
             OutputDebugStringA("[WasapiRecorder] Loopback Disconnected/Invalidated!\n");
        } else {
             char errBuf[64];
             snprintf(errBuf, sizeof(errBuf), "[WasapiRecorder] Loopback Error: 0x%08X\n", hr);
             OutputDebugStringA(errBuf);
        }

        if (hRecorderWnd) {
            PostMessage(hRecorderWnd, WM_APP_RECORDING_ERROR, 0, 0);
        }
    }
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

// Helper: Get sample from buffer at frame index (handles float/PCM, mono-mixes channels)
static float GetSampleFromBuffer(const std::vector<BYTE>& buffer, size_t frameIndex, 
                                  WAVEFORMATEX* pwfx, bool isFloat) {
    if (frameIndex >= buffer.size() / pwfx->nBlockAlign) return 0.0f;
    
    float sample = 0.0f;
    size_t offset = frameIndex * pwfx->nBlockAlign;
    
    if (isFloat) {
        float* ptr = (float*)(buffer.data() + offset);
        for (int ch = 0; ch < pwfx->nChannels; ch++) {
            sample += ptr[ch];
        }
        sample /= pwfx->nChannels;
    } else {
        short* ptr = (short*)(buffer.data() + offset);
        for (int ch = 0; ch < pwfx->nChannels; ch++) {
            sample += ptr[ch] / 32768.0f;
        }
        sample /= pwfx->nChannels;
    }
    return sample;
}

// Linear interpolation for resampling
static float InterpolateSample(const std::vector<BYTE>& buffer, double position,
                                WAVEFORMATEX* pwfx, bool isFloat) {
    size_t totalFrames = buffer.size() / pwfx->nBlockAlign;
    if (totalFrames == 0) return 0.0f;
    
    size_t idx0 = (size_t)position;
    size_t idx1 = idx0 + 1;
    double frac = position - idx0;
    
    if (idx0 >= totalFrames) return 0.0f;
    if (idx1 >= totalFrames) idx1 = idx0;
    
    float s0 = GetSampleFromBuffer(buffer, idx0, pwfx, isFloat);
    float s1 = GetSampleFromBuffer(buffer, idx1, pwfx, isFloat);
    
    return s0 + (float)(frac * (s1 - s0));
}

// Mix both buffers with proper sample rate handling
std::vector<BYTE> WasapiRecorder::MixBuffers() {
    std::vector<BYTE> output;
    
    // Lock both buffers
    std::lock_guard<std::mutex> lockMic(micBufferMutex);
    std::lock_guard<std::mutex> lockLoop(loopbackBufferMutex);
    
    if (!pwfxMic || !pwfxLoopback) return output;
    
    // Detect format types
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
    
    // Get sample rates
    int micSampleRate = pwfxMic->nSamplesPerSec;
    int loopSampleRate = pwfxLoopback->nSamplesPerSec;
    
    // Use loopback sample rate as output (it's usually system default)
    int outputSampleRate = loopSampleRate;
    
    // Calculate frame counts
    size_t micFrames = micBuffer.size() / pwfxMic->nBlockAlign;
    size_t loopFrames = loopbackBuffer.size() / pwfxLoopback->nBlockAlign;
    
    // Calculate duration in seconds for each buffer
    double micDuration = (micFrames > 0) ? (double)micFrames / micSampleRate : 0.0;
    double loopDuration = (loopFrames > 0) ? (double)loopFrames / loopSampleRate : 0.0;
    double maxDuration = (micDuration > loopDuration) ? micDuration : loopDuration;
    
    // Output frames based on output sample rate
    size_t outputFrames = (size_t)(maxDuration * outputSampleRate);
    if (outputFrames == 0) return output;
    
    // Log sample rates for debugging
    char debugBuf[256];
    snprintf(debugBuf, sizeof(debugBuf), 
        "[WasapiRecorder] Mixing: Mic=%dHz (%zu frames), Loop=%dHz (%zu frames), Output=%dHz (%zu frames)\n",
        micSampleRate, micFrames, loopSampleRate, loopFrames, outputSampleRate, outputFrames);
    OutputDebugStringA(debugBuf);
    
    // Output: 16-bit stereo
    output.resize(outputFrames * 4); // 2 channels * 2 bytes per sample
    short* outPtr = (short*)output.data();
    
    for (size_t i = 0; i < outputFrames; i++) {
        // Calculate position in time (0.0 to maxDuration)
        double timePos = (double)i / outputSampleRate;
        
        // Get mic sample (resample to output rate)
        float micSample = 0.0f;
        if (micFrames > 0 && timePos <= micDuration) {
            double micPos = timePos * micSampleRate;
            micSample = InterpolateSample(micBuffer, micPos, pwfxMic, micIsFloat);
        }
        
        // Get loopback sample (already at output rate, no resampling needed)
        float loopSample = 0.0f;
        if (loopFrames > 0 && timePos <= loopDuration) {
            double loopPos = timePos * loopSampleRate;
            loopSample = InterpolateSample(loopbackBuffer, loopPos, pwfxLoopback, loopIsFloat);
        }
        
        // Stereo output: Mic (Advisor) = Left, Loopback (CX) = Right
        outPtr[i * 2] = FloatToShort(micSample);      // Left = Advisor
        outPtr[i * 2 + 1] = FloatToShort(loopSample); // Right = CX
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

// Mixer thread: periodically mixes captured audio and writes to disk
void WasapiRecorder::MixerLoop() {
    OutputDebugStringA("[WasapiRecorder] Mixer thread started\n");
    
    while (isRecording) {
        // Wait for flush interval or stop signal
        {
            std::unique_lock<std::mutex> lock(m_mixerMutex);
            m_mixerCV.wait_for(lock, std::chrono::milliseconds(FLUSH_INTERVAL_MS));
        }
        
        if (!isRecording) break;
        if (isPaused) continue;
        
        // Mix and write buffered audio to disk
        MixAndWriteChunk();
    }
    
    // Final flush of remaining audio
    MixAndWriteChunk();
    
    OutputDebugStringA("[WasapiRecorder] Mixer thread stopped\n");
}

// Mix currently buffered audio and write to disk
void WasapiRecorder::MixAndWriteChunk() {
    if (!m_pWriter || !m_pWriter->IsActive()) return;
    
    // Get copies of current buffers and clear them
    std::vector<BYTE> micCopy, loopCopy;
    
    {
        std::lock_guard<std::mutex> lock(micBufferMutex);
        micCopy = std::move(micBuffer);
        micBuffer.clear();
    }
    {
        std::lock_guard<std::mutex> lock(loopbackBufferMutex);
        loopCopy = std::move(loopbackBuffer);
        loopbackBuffer.clear();
    }
    
    if (micCopy.empty() && loopCopy.empty()) return;
    
    // Need format info for mixing
    if (!pwfxMic || !pwfxLoopback) return;
    
    // Detect format types
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
    
    // Get sample rates
    int micSampleRate = pwfxMic->nSamplesPerSec;
    int loopSampleRate = pwfxLoopback->nSamplesPerSec;
    int outputSampleRate = OUTPUT_SAMPLE_RATE; // Use fixed output rate to match writer
    
    // Calculate frame counts
    size_t micFrames = micCopy.size() / pwfxMic->nBlockAlign;
    size_t loopFrames = loopCopy.size() / pwfxLoopback->nBlockAlign;
    
    // Calculate durations
    double micDuration = (micFrames > 0) ? (double)micFrames / micSampleRate : 0.0;
    double loopDuration = (loopFrames > 0) ? (double)loopFrames / loopSampleRate : 0.0;
    double maxDuration = (micDuration > loopDuration) ? micDuration : loopDuration;
    
    // Output frames
    size_t outputFrames = (size_t)(maxDuration * outputSampleRate);
    if (outputFrames == 0) return;
    
    // Create output buffer (16-bit stereo)
    std::vector<BYTE> output(outputFrames * 4);
    short* outPtr = (short*)output.data();
    
    // Helper lambda for getting samples (simplified interpolation)
    auto getSample = [](const std::vector<BYTE>& buf, size_t frame, WAVEFORMATEX* pwfx, bool isFloat) -> float {
        if (buf.empty() || !pwfx || frame >= buf.size() / pwfx->nBlockAlign) return 0.0f;
        size_t offset = frame * pwfx->nBlockAlign;
        float sample = 0.0f;
        if (isFloat) {
            float* ptr = (float*)(buf.data() + offset);
            for (int ch = 0; ch < pwfx->nChannels; ch++) sample += ptr[ch];
            sample /= pwfx->nChannels;
        } else {
            short* ptr = (short*)(buf.data() + offset);
            for (int ch = 0; ch < pwfx->nChannels; ch++) sample += ptr[ch] / 32768.0f;
            sample /= pwfx->nChannels;
        }
        return sample;
    };
    
    for (size_t i = 0; i < outputFrames; i++) {
        double timePos = (double)i / outputSampleRate;
        
        // Get mic sample
        float micSample = 0.0f;
        if (micFrames > 0 && timePos <= micDuration) {
            size_t micFrame = (size_t)(timePos * micSampleRate);
            micSample = getSample(micCopy, micFrame, pwfxMic, micIsFloat);
        }
        
        // Get loopback sample
        float loopSample = 0.0f;
        if (loopFrames > 0 && timePos <= loopDuration) {
            size_t loopFrame = (size_t)(timePos * loopSampleRate);
            loopSample = getSample(loopCopy, loopFrame, pwfxLoopback, loopIsFloat);
        }
        
        // Stereo output: Mic (Advisor) = Left, Loopback (CX) = Right
        auto toShort = [](float s) -> short {
            s = s * 32767.0f;
            if (s > 32767.0f) s = 32767.0f;
            if (s < -32768.0f) s = -32768.0f;
            return (short)s;
        };
        outPtr[i * 2] = toShort(micSample);      // Left = Advisor
        outPtr[i * 2 + 1] = toShort(loopSample); // Right = CX
    }
    
    // Write to disk
    m_pWriter->WriteChunk(output.data(), output.size());
    
    // Check for failure (e.g. folder deleted)
    if (m_pWriter->HasFailed()) {
        OutputDebugStringA("[WasapiRecorder] Writer failed! Stopping recording...\n");
        // Post message to UI thread to stop recording safely
        if (hRecorderWnd) {
            PostMessage(hRecorderWnd, WM_APP_RECORDING_ERROR, 0, 0);
        }
        // Also signal stop internally to break loops?
        // No, let UI allow Stop() to handle it properly.
        // But we should stop writing immediately.
        return;
    }

    char debug[128];
    snprintf(debug, sizeof(debug), "[WasapiRecorder] Wrote %.2f sec chunk to disk\n", maxDuration);
    OutputDebugStringA(debug);
}

std::string WasapiRecorder::FinalizeStreaming(const std::string& filename) {
    if (!m_pWriter) return "";
    
    // Finalize the streaming writer
    std::string result = m_pWriter->Finalize(filename);
    
    // Delete writer for next recording
    delete m_pWriter;
    m_pWriter = nullptr;
    
    return result;
}

double WasapiRecorder::GetDurationSeconds() const {
    if (m_streamingMode && m_pWriter) {
        return m_pWriter->GetDurationSeconds();
    }
    
    // Legacy mode: estimate from buffer sizes
    if (pwfxLoopback && pwfxLoopback->nAvgBytesPerSec > 0) {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(loopbackBufferMutex));
        return (double)loopbackBuffer.size() / pwfxLoopback->nAvgBytesPerSec;
    }
    return 0.0;
}
