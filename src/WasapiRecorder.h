#pragma once

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
    void RecordingLoop();
    void WriteWavHeader(std::ofstream& file, int totalDataLen, int sampleRate, int channels, int bitsPerSample);

    std::atomic<bool> isRecording;
    std::atomic<bool> isPaused;
    std::thread recordingThread;

    std::mutex bufferMutex;
    std::vector<BYTE> audioBuffer;
    
    // Audio Format
    WAVEFORMATEX* pwfx;
};
