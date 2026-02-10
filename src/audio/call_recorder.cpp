#include "audio/call_recorder.h"
#include "audio/WasapiRecorder.h"
#include "audio/recorder.h"
#include "network/http_server.h"
#include "core/globals.h"
#include "audio/audio.h"
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <sys/stat.h>

#pragma comment(lib, "shell32.lib")

// Global instance
CallAutoRecorder* g_CallRecorder = nullptr;

// Helper: Get current date as YYYY-MM-DD
static std::string GetCurrentDateString() {
    time_t now = std::time(nullptr);
    struct tm tm;
    localtime_s(&tm, &now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

// Helper: Get timestamp for file naming
static std::string GetTimestampString() {
    time_t now = std::time(nullptr);
    struct tm tm;
    localtime_s(&tm, &now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H-%M-%S");
    return oss.str();
}

// Helper: Check if directory exists
static bool DirectoryExists(const std::string& path) {
    struct _stat info;
    if (_stat(path.c_str(), &info) != 0) return false;
    return (info.st_mode & _S_IFDIR) != 0;
}

// Helper: Create directory
static bool CreateDirectoryIfNeeded(const std::string& path) {
    if (DirectoryExists(path)) return true;
    return CreateDirectoryA(path.c_str(), nullptr) != 0;
}

// Helper: Count .wav files in a directory
static int CountRecordings(const std::string& folderPath) {
    if (folderPath.empty()) return 0;
    
    std::string searchPath = folderPath + "\\*.wav";
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) return 0;
    
    int count = 0;
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            count++;
        }
    } while (FindNextFile(hFind, &findData));
    
    FindClose(hFind);
    return count;
}

CallAutoRecorder::CallAutoRecorder()
    : enabled(false)
    , currentState(State::IDLE)
    , lastVoiceTime(0)
    , recordingStartTick(0)
    , recordingStartTime(0)
    , voiceThreshold(0.03f)      // 3% audio level (more sensitive)
    , silenceTimeoutMs(30000)    // 30 seconds silence (robust for pauses)
    , minCallDurationMs(10000)   // 10 seconds minimum to be a valid call
    , gracePeriodMs(15000)       // 15 seconds grace - no silence check initially
    , todayCallCount(0)
    , pRecorder(nullptr)
{
    pRecorder = new WasapiRecorder();
    currentDate = GetCurrentDateString();
}

CallAutoRecorder::~CallAutoRecorder() {
    Disable();
    if (pRecorder) {
        delete pRecorder;
        pRecorder = nullptr;
    }
}

void CallAutoRecorder::Enable() {
    if (enabled) return;
    
    // Reset date tracking
    currentDate = GetCurrentDateString();
    
    // Scan existing recordings for today
    std::string folder = GetCurrentDateFolder();
    todayCallCount = CountRecordings(folder);
    
    enabled = true;
    currentState = State::DETECTING;
    lastVoiceTime = 0;
}

void CallAutoRecorder::Disable() {
    if (!enabled) return;
    
    // If currently recording, stop and save
    if (currentState == State::RECORDING && pRecorder && pRecorder->IsRecording()) {
        OnSilenceTimeout();
    }
    
    enabled = false;
    currentState = State::IDLE;
}

void CallAutoRecorder::TransitionTo(State newState) {
    currentState = newState;
    
    // Play beep if starting recording and enabled
    if (newState == State::RECORDING && beepOnCall) {
        // High pitch beep (750Hz) for 300ms 
        Beep(750, 300);
    }
}

void CallAutoRecorder::Poll() {
    if (!enabled) return;
    
    // Check if date changed (new day)
    std::string today = GetCurrentDateString();
    if (today != currentDate) {
        currentDate = today;
        // Date changed, reset count by scanning new folder (likely 0, but good to be safe)
        std::string folder = GetCurrentDateFolder();
        todayCallCount = CountRecordings(folder);
    }

    // Safety: If extension disconnects (tab closed) while recording, save immediately
    if (currentState == State::RECORDING && !IsExtensionConnected()) {
        ForceStopRecording();
    }
    
    // No VAD - recording is controlled by HTTP server (extension signals)
    // Poll() is now only for date tracking and UI updates
}

void CallAutoRecorder::OnVoiceDetected() {
    if (!pRecorder || currentState != State::DETECTING) return;
    
    // Start recording
    if (pRecorder->Start()) {
        recordingStartTick = GetTickCount();
        recordingStartTime = std::time(nullptr);
        lastVoiceTime = recordingStartTick;
        TransitionTo(State::RECORDING);
    }
}

void CallAutoRecorder::OnSilenceTimeout() {
    if (!pRecorder || currentState != State::RECORDING) return;
    
    TransitionTo(State::SAVING);
    
    // Stop recording
    pRecorder->Stop();
    
    // Check minimum duration
    DWORD duration = GetTickCount() - recordingStartTick;
    if (duration >= (DWORD)minCallDurationMs) {
        SaveCurrentRecording();
    }
    
    // Clear buffer for next call
    pRecorder->Clear();
    
    // Ready for next call
    TransitionTo(State::DETECTING);
}

// Force start recording from external trigger (HTTP server)
void CallAutoRecorder::ForceStartRecording(const std::map<std::string, std::string>& metadata) {
    if (!pRecorder) return;
    
    // If already recording, do nothing
    if (currentState == State::RECORDING) return;

    // Store initial metadata
    currentCallMetadata = metadata;

    // Enforce Folder Selection (Must be on UI thread really, but let's try direct call or use main window)
    // Since this might be called from HTTP thread, we need to be careful. 
    // However, MessageBox is generally thread-safe if parent is NULL or valid window.
    // Ideally we should PostMessage to main thread, but for now strict enforcement:
    if (recordingFolder.empty()) {
        // We need updates to run on UI thread to be safe with COM (BrowseFolder)
        // But for satisfied requirement "select hone ke bd hi on hoga", we can block here or fail.
        // Prompting from a hidden background thread is bad UX (might hide behind windows).
        // Better to fail internally if not set, BUT user asked for PROMPT.
        
        // Let's use the main window handle if available
        if (!EnsureRecordingFolderSelected(hMainWnd)) {
            return; // Cancelled or failed
        }
    }
    
    // Create date folder for streaming
    std::string dateFolder = CreateDateFolder();
    if (dateFolder.empty()) return;
    
    // Start streaming mode for memory safety
    if (pRecorder->StartStreaming(dateFolder)) {
        recordingStartTick = GetTickCount();
        recordingStartTime = std::time(nullptr);
        lastVoiceTime = recordingStartTick;
        TransitionTo(State::RECORDING);
    }
}

// Force stop recording from external trigger (HTTP server)
void CallAutoRecorder::ForceStopRecording(const std::map<std::string, std::string>& metadata) {
    if (!pRecorder) return;
    
    // If not recording, do nothing
    if (currentState != State::RECORDING) return;
    
    // Update/Merge metadata (extension sends fresh full data on stop)
    if (!metadata.empty()) {
        for (const auto& kv : metadata) {
            currentCallMetadata[kv.first] = kv.second;
        }
    }
    
    TransitionTo(State::SAVING);
    
    // Stop recording
    pRecorder->Stop();
    
    // Save if long enough
    DWORD duration = GetTickCount() - recordingStartTick;
    if (duration >= (DWORD)minCallDurationMs) {
        SaveCurrentRecording();
    } else {
        // Too short - just cleanup (FinalizeStreaming will still save temp file)
        pRecorder->FinalizeStreaming("discarded.wav");
    }
    
    // Clear metadata
    currentCallMetadata.clear();
    
    // Ready for next call (waiting for extension signal)
    TransitionTo(State::DETECTING);
}

std::string CallAutoRecorder::CreateDateFolder() {
    if (recordingFolder.empty()) return "";
    
    std::string dateFolder = recordingFolder + "\\" + currentDate;
    CreateDirectoryIfNeeded(dateFolder);
    return dateFolder;
}

std::string CallAutoRecorder::GetNextFileName(int count) {
    std::ostringstream oss;
    oss << "call_" << std::setfill('0') << std::setw(3) << count 
        << "_" << GetTimestampString() << ".wav";
    return oss.str();
}

std::string CallAutoRecorder::GetCurrentDateFolder() const {
    if (recordingFolder.empty()) return "";
    return recordingFolder + "\\" + currentDate;
}

void CallAutoRecorder::SaveCurrentRecording() {
    std::string folder = CreateDateFolder();
    if (folder.empty()) return;
    
    // Use the next number for the filename
    std::string filename = GetNextFileName(todayCallCount + 1);
    
    time_t endTime = std::time(nullptr);
    
    // Finalize streaming file (updates header and renames)
    std::string savedPath = pRecorder->FinalizeStreaming(filename);
    
    if (!savedPath.empty()) {
        // Only now that the file exists on disk, we sync the count
        todayCallCount = CountRecordings(folder);
        
        CreateMetadataFile(savedPath, recordingStartTime, endTime);
        // Notify recorder window about saved file
        NotifyAutoRecordSaved(filename);
    }
}

void CallAutoRecorder::CreateMetadataFile(const std::string& audioPath, time_t startTime, time_t endTime) {
    // Replace .wav with .txt
    std::string txtPath = audioPath;
    size_t pos = txtPath.rfind(".wav");
    if (pos != std::string::npos) {
        txtPath.replace(pos, 4, ".txt");
    } else {
        txtPath += ".txt";
    }
    
    std::ofstream txtFile(txtPath);
    if (txtFile.is_open()) {
        struct tm tmStart, tmEnd;
        localtime_s(&tmStart, &startTime);
        localtime_s(&tmEnd, &endTime);
        
        double duration = difftime(endTime, startTime);
        
        txtFile << "Call Recording Metadata\n";
        txtFile << "=======================\n";
        txtFile << "File: " << audioPath.substr(audioPath.find_last_of("\\/") + 1) << "\n";
        txtFile << "Start Time: " << std::put_time(&tmStart, "%Y-%m-%d %H:%M:%S") << "\n";
        txtFile << "End Time: " << std::put_time(&tmEnd, "%Y-%m-%d %H:%M:%S") << "\n";
        txtFile << "Duration: " << duration << " seconds\n";
        txtFile << "Call Number: " << todayCallCount << "\n";
        
        // Write dynamic metadata from Ozonetel
        if (!currentCallMetadata.empty()) {
            txtFile << "\n[Ozonetel Details]\n";
            for (const auto& kv : currentCallMetadata) {
                txtFile << kv.first << ": " << kv.second << "\n";
            }
        }
        
        txtFile.close();
    }
}

// Global helper functions
void InitCallRecorder() {
    if (!g_CallRecorder) {
        g_CallRecorder = new CallAutoRecorder();
    }
}

void CleanupCallRecorder() {
    if (g_CallRecorder) {
        delete g_CallRecorder;
        g_CallRecorder = nullptr;
    }
}
