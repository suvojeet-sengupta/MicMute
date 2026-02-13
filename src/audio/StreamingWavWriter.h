#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>
#include <fstream>
#include <mutex>
#include <atomic>

// Streaming WAV file writer - writes audio data directly to disk
// without accumulating in RAM. Handles crash recovery via temp files.
class StreamingWavWriter {
public:
    StreamingWavWriter();
    ~StreamingWavWriter();

    // Initialize and open temp file for writing
    // outputFolder: Directory where recordings are saved
    // Returns true on success
    bool Start(const std::string& outputFolder, int sampleRate, int channels, int bitsPerSample);

    // Write a chunk of PCM audio data to disk
    // This appends to the file immediately - no RAM accumulation
    void WriteChunk(const void* data, size_t bytes);

    // Finalize the recording: update WAV header with correct size
    // and rename temp file to final filename
    // Returns the final filename on success, empty string on failure
    std::string Finalize(const std::string& finalFilename);

    // Abort recording without saving (delete temp file)
    void Abort();

    // Check if writer is active
    bool IsActive() const { return m_isActive; }

    // Get current temp file path
    std::string GetTempFilePath() const { return m_tempFilePath; }

    // Get total bytes written so far
    size_t GetBytesWritten() const { return m_totalBytesWritten; }

    // Check if writer has failed (e.g. disk full, disconnected)
    bool HasFailed() const { return m_failed; }

    // Get current recording duration in seconds
    double GetDurationSeconds() const;

private:
    void WriteWavHeader();
    void UpdateWavHeader();

    std::ofstream m_file;
    std::string m_tempFilePath;
    std::string m_outputFolder;
    std::mutex m_writeMutex;
    std::atomic<bool> m_isActive;
    std::atomic<bool> m_failed;
    std::atomic<size_t> m_totalBytesWritten;

    std::atomic<ULONGLONG> m_lastFlushTime;
    void PeriodicFlush();

    // Audio format
    int m_sampleRate;
    int m_channels;
    int m_bitsPerSample;
    int m_byteRate;
    int m_blockAlign;
};
