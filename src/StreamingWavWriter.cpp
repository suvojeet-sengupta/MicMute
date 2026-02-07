#include "StreamingWavWriter.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cstdio>

StreamingWavWriter::StreamingWavWriter()
    : m_isActive(false)
    , m_totalBytesWritten(0)
    , m_sampleRate(48000)
    , m_channels(2)
    , m_bitsPerSample(16)
    , m_byteRate(0)
    , m_blockAlign(0)
{
}

StreamingWavWriter::~StreamingWavWriter() {
    Abort();
}

bool StreamingWavWriter::Start(const std::string& outputFolder, int sampleRate, int channels, int bitsPerSample) {
    std::lock_guard<std::mutex> lock(m_writeMutex);

    if (m_isActive) {
        return false; // Already recording
    }

    m_outputFolder = outputFolder;
    m_sampleRate = sampleRate;
    m_channels = channels;
    m_bitsPerSample = bitsPerSample;
    m_blockAlign = channels * bitsPerSample / 8;
    m_byteRate = sampleRate * m_blockAlign;
    m_totalBytesWritten = 0;

    // Generate temp filename with timestamp
    auto t = std::time(nullptr);
    struct tm tm;
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << outputFolder << "\\~recording_" 
        << std::put_time(&tm, "%Y%m%d_%H%M%S") 
        << ".wav.tmp";
    m_tempFilePath = oss.str();

    // Open file for binary writing
    m_file.open(m_tempFilePath, std::ios::binary | std::ios::trunc);
    if (!m_file.is_open()) {
        OutputDebugStringA("[StreamingWavWriter] Failed to open temp file\n");
        return false;
    }

    // Write initial WAV header with placeholder size
    WriteWavHeader();

    m_isActive = true;

    char debug[256];
    snprintf(debug, sizeof(debug), "[StreamingWavWriter] Started: %s\n", m_tempFilePath.c_str());
    OutputDebugStringA(debug);

    return true;
}

void StreamingWavWriter::WriteWavHeader() {
    // Write WAV header with placeholder sizes (will be updated on Finalize)
    // RIFF chunk
    m_file.write("RIFF", 4);
    int chunkSize = 0; // Placeholder - will be updated
    m_file.write(reinterpret_cast<char*>(&chunkSize), 4);
    m_file.write("WAVE", 4);

    // fmt subchunk
    m_file.write("fmt ", 4);
    int subChunk1Size = 16; // PCM
    m_file.write(reinterpret_cast<char*>(&subChunk1Size), 4);

    short audioFormat = 1; // PCM
    m_file.write(reinterpret_cast<char*>(&audioFormat), 2);

    short numChannels = static_cast<short>(m_channels);
    m_file.write(reinterpret_cast<char*>(&numChannels), 2);

    m_file.write(reinterpret_cast<char*>(&m_sampleRate), 4);
    m_file.write(reinterpret_cast<char*>(&m_byteRate), 4);

    short blockAlign = static_cast<short>(m_blockAlign);
    m_file.write(reinterpret_cast<char*>(&blockAlign), 2);

    short bitsPerSample = static_cast<short>(m_bitsPerSample);
    m_file.write(reinterpret_cast<char*>(&bitsPerSample), 2);

    // data subchunk
    m_file.write("data", 4);
    int dataSize = 0; // Placeholder - will be updated
    m_file.write(reinterpret_cast<char*>(&dataSize), 4);

    m_file.flush();
}

void StreamingWavWriter::WriteChunk(const void* data, size_t bytes) {
    if (!m_isActive || bytes == 0) return;

    std::lock_guard<std::mutex> lock(m_writeMutex);

    if (!m_file.is_open()) return;

    m_file.write(static_cast<const char*>(data), bytes);
    m_totalBytesWritten += bytes;

    // Flush periodically to ensure data is on disk
    // (helps with crash recovery)
    m_file.flush();
}

void StreamingWavWriter::UpdateWavHeader() {
    if (!m_file.is_open()) return;

    size_t dataSize = m_totalBytesWritten;
    int chunkSize = static_cast<int>(36 + dataSize);
    int dataSizeInt = static_cast<int>(dataSize);

    // Seek to RIFF chunk size (offset 4)
    m_file.seekp(4, std::ios::beg);
    m_file.write(reinterpret_cast<char*>(&chunkSize), 4);

    // Seek to data chunk size (offset 40)
    m_file.seekp(40, std::ios::beg);
    m_file.write(reinterpret_cast<char*>(&dataSizeInt), 4);

    m_file.flush();
}

std::string StreamingWavWriter::Finalize(const std::string& finalFilename) {
    std::lock_guard<std::mutex> lock(m_writeMutex);

    if (!m_isActive) {
        return "";
    }

    // Update WAV header with actual sizes
    UpdateWavHeader();

    // Close the file
    m_file.close();

    m_isActive = false;

    // Build final path
    std::string finalPath = m_outputFolder + "\\" + finalFilename;

    // Rename temp file to final file
    // First, delete existing file if present
    DeleteFileA(finalPath.c_str());

    if (MoveFileA(m_tempFilePath.c_str(), finalPath.c_str())) {
        char debug[512];
        snprintf(debug, sizeof(debug), "[StreamingWavWriter] Finalized: %s (%.2f MB)\n", 
                 finalPath.c_str(), m_totalBytesWritten / (1024.0 * 1024.0));
        OutputDebugStringA(debug);
        return finalPath;
    } else {
        DWORD err = GetLastError();
        char debug[256];
        snprintf(debug, sizeof(debug), "[StreamingWavWriter] Failed to rename file, error: %lu\n", err);
        OutputDebugStringA(debug);
        // Return temp path as fallback - file is still valid
        return m_tempFilePath;
    }
}

void StreamingWavWriter::Abort() {
    std::lock_guard<std::mutex> lock(m_writeMutex);

    if (m_file.is_open()) {
        m_file.close();
    }

    if (m_isActive && !m_tempFilePath.empty()) {
        // Delete temp file
        DeleteFileA(m_tempFilePath.c_str());
        OutputDebugStringA("[StreamingWavWriter] Aborted and deleted temp file\n");
    }

    m_isActive = false;
    m_totalBytesWritten = 0;
    m_tempFilePath.clear();
}

double StreamingWavWriter::GetDurationSeconds() const {
    if (m_byteRate == 0) return 0.0;
    return static_cast<double>(m_totalBytesWritten) / m_byteRate;
}
