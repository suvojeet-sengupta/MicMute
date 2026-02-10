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
#include <condition_variable>

// Forward declaration
class StreamingWavWriter;

class WasapiRecorder {
public:
    WasapiRecorder();
    ~WasapiRecorder();

    // Legacy mode: buffer in RAM (for short recordings only)
    bool Start();
    
    // Streaming mode: write directly to disk (recommended for long recordings)
    bool StartStreaming(const std::string& outputFolder);
    
    void Stop();
    void Pause();
    void Resume();
    
    // Saves the currently recorded buffer to a WAV file (legacy mode only)
    bool SaveToFile(const std::string& filename);
    
    // Finalize streaming recording and return final filename
    std::string FinalizeStreaming(const std::string& filename);
    
    // Clears the current buffer
    void Clear();

    bool IsRecording() const { return isRecording; }
    bool IsPaused() const { return isPaused; }
    bool IsStreaming() const { return m_streamingMode; }
    
    // Get current recording duration in seconds
    double GetDurationSeconds() const;

private:
    void MicrophoneLoop();    // Captures microphone audio
    void LoopbackLoop();      // Captures system audio (loopback)
    void MixerLoop();         // Mixes and writes to disk (streaming mode)
    void WriteWavHeader(std::ofstream& file, int totalDataLen, int sampleRate, int channels, int bitsPerSample);
    
    // Mix two buffers into one (stereo: mic=left, loopback=right)
    std::vector<BYTE> MixBuffers();
    
    // Mix a chunk of data for streaming mode
    void MixAndWriteChunk();

    std::atomic<bool> isRecording;
    std::atomic<bool> isPaused;
    std::atomic<bool> m_streamingMode;
    
    // Capture threads
    std::thread micThread;
    std::thread loopbackThread;
    std::thread mixerThread;  // For streaming mode

    // Separate buffers for each source (used as circular buffers in streaming mode)
    std::mutex micBufferMutex;
    std::vector<BYTE> micBuffer;
    
    std::mutex loopbackBufferMutex;
    std::vector<BYTE> loopbackBuffer;
    
    // Streaming writer
    StreamingWavWriter* m_pWriter;
    std::string m_outputFolder;
    
    // Mixer synchronization
    std::mutex m_mixerMutex;
    std::condition_variable m_mixerCV;
    DWORD m_lastFlushTime;
    static const DWORD FLUSH_INTERVAL_MS = 2000; // Flush every 2 seconds
    static const size_t MAX_BUFFER_SIZE = 5 * 1024 * 1024; // 5MB max buffer (~25 seconds)
    
    // Audio Formats (may differ between devices)
    WAVEFORMATEX* pwfxMic;
    WAVEFORMATEX* pwfxLoopback;
    
    // Common output format for mixing
    static const int OUTPUT_SAMPLE_RATE = 48000;
    static const int OUTPUT_CHANNELS = 1;    // Mono
    static const int OUTPUT_BITS = 16;
};

