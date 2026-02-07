#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

class WasapiRecorder {
public:
    WasapiRecorder();
    ~WasapiRecorder();

    bool Start();
    void Stop();
    void Pause();
    void Resume();
    
    // Saves the currently recorded buffer to a WAV file
    bool SaveToFile(const std::string& filename);
    
    // Clears the current buffer
    void Clear();

    bool IsRecording() const { return isRecording; }
    bool IsPaused() const { return isPaused; }

private:
    void MicrophoneLoop();    // Captures microphone audio
    void LoopbackLoop();      // Captures system audio (loopback)
    void WriteWavHeader(std::ofstream& file, int totalDataLen, int sampleRate, int channels, int bitsPerSample);
    
    // Mix two buffers into one (stereo: mic=left, loopback=right)
    std::vector<BYTE> MixBuffers();

    std::atomic<bool> isRecording;
    std::atomic<bool> isPaused;
    
    // Dual capture threads
    std::thread micThread;
    std::thread loopbackThread;

    // Separate buffers for each source
    std::mutex micBufferMutex;
    std::vector<BYTE> micBuffer;
    
    std::mutex loopbackBufferMutex;
    std::vector<BYTE> loopbackBuffer;
    
    // Audio Formats (may differ between devices)
    WAVEFORMATEX* pwfxMic;
    WAVEFORMATEX* pwfxLoopback;
    
    // Common output format for mixing
    static const int OUTPUT_SAMPLE_RATE = 48000;
    static const int OUTPUT_CHANNELS = 2;    // Stereo
    static const int OUTPUT_BITS = 16;
};
