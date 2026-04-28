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

// Helper: Delete directory recursively
static void DeleteDirectory(const std::string& path) {
    std::string searchPath = path + "\\*";
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) continue;

        std::string filePath = path + "\\" + findData.cFileName;
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            DeleteDirectory(filePath);
        } else {
            DeleteFile(filePath.c_str());
        }
    } while (FindNextFile(hFind, &findData));
    FindClose(hFind);

    RemoveDirectory(path.c_str());
}

// Helper: Cleanup old recordings
static void CleanupOldRecordings() {
    if (autoDeleteDays <= 0 || recordingFolder.empty()) return;

    time_t now = std::time(nullptr);
    struct tm tmNow;
    localtime_s(&tmNow, &now);
    
    // Approximate conversion
    // We iterate folders in recordingFolder
    std::string searchPath = recordingFolder + "\\*";
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) continue;

            // Check if folder name is YYYY-MM-DD
            int year, month, day;
            if (sscanf_s(findData.cFileName, "%d-%d-%d", &year, &month, &day) == 3) {
                struct tm tmFolder = {0};
                tmFolder.tm_year = year - 1900;
                tmFolder.tm_mon = month - 1;
                tmFolder.tm_mday = day;
                tmFolder.tm_isdst = -1;
                
                time_t folderTime = mktime(&tmFolder);
                if (folderTime != -1) {
                    double diff = difftime(now, folderTime);
                    int daysOld = (int)(diff / (60 * 60 * 24));
                    
                    if (daysOld > autoDeleteDays) {
                        std::string path = recordingFolder + "\\" + findData.cFileName;
                        DeleteDirectory(path);
                    }
                }
            }
        }
    } while (FindNextFile(hFind, &findData));
    FindClose(hFind);
}

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
    std::lock_guard<std::recursive_mutex> lk(stateMutex);
    if (enabled.load()) return;

    currentDate = GetCurrentDateString();
    CleanupOldRecordings();

    std::string folder = GetCurrentDateFolder();
    todayCallCount = CountRecordings(folder);

    enabled.store(true);
    currentState.store(State::DETECTING);
    lastVoiceTime = 0;
}

void CallAutoRecorder::Disable() {
    std::lock_guard<std::recursive_mutex> lk(stateMutex);
    if (!enabled.load()) return;

    if (currentState.load() == State::RECORDING && pRecorder && pRecorder->IsRecording()) {
        OnSilenceTimeout();
    }

    enabled.store(false);
    currentState.store(State::IDLE);
}

void CallAutoRecorder::TransitionTo(State newState) {
    currentState.store(newState);

    // Beep is now handled in http_server.cpp (independent of recording)
}

void CallAutoRecorder::Poll() {
    if (!enabled.load()) return;

    // Check if date changed (new day)
    std::string today = GetCurrentDateString();
    if (today != currentDate) {
        currentDate = today;
        std::string folder = GetCurrentDateFolder();
        todayCallCount = CountRecordings(folder);
    }

    // Safety: if the extension disconnects (tab closed) while recording, save immediately
    if (currentState.load() == State::RECORDING && !IsExtensionConnected()) {
        ForceStopRecording();
    }
}

void CallAutoRecorder::OnVoiceDetected() {
    if (!pRecorder || currentState != State::DETECTING) return;
    
    // Start recording
    if (pRecorder->Start()) {
        recordingStartTick = GetTickCount64();
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
    ULONGLONG duration = GetTickCount64() - recordingStartTick;
    if (duration >= (DWORD)minCallDurationMs) {
        SaveCurrentRecording();
    }
    
    // Clear buffer for next call
    pRecorder->Clear();
    
    // Ready for next call
    TransitionTo(State::DETECTING);
}

// Force start recording from external trigger (HTTP server).
// Critical: when a /start arrives while already recording (back-to-back calls
// where the previous /stop was missed or delayed), we MUST finalize the previous
// recording first, otherwise both calls get merged into a single file with the
// metadata of just one call.
void CallAutoRecorder::ForceStartRecording(const std::map<std::string, std::string>& metadata) {
    std::lock_guard<std::recursive_mutex> lk(stateMutex);
    if (!pRecorder) return;
    if (!enabled.load()) return;

    // If a previous recording is still in flight, finalize it first so we don't
    // bleed the next call into the same WAV file.
    if (currentState.load() == State::RECORDING) {
        TransitionTo(State::SAVING);
        pRecorder->Stop();
        ULONGLONG prevDuration = GetTickCount64() - recordingStartTick;
        if (prevDuration >= (DWORD)minCallDurationMs) {
            SaveCurrentRecording();
        } else {
            pRecorder->FinalizeStreaming("discarded.wav");
        }
        currentCallMetadata.clear();
        TransitionTo(State::DETECTING);
    }

    // Fresh metadata for the new call
    currentCallMetadata = metadata;

    // Recording folder must be selected. If called from the HTTP thread, the
    // BrowseFolder dialog opens on the main window and the HTTP thread blocks
    // until the user picks one — acceptable trade-off given Ozonetel will
    // immediately retry if we drop the call.
    if (recordingFolder.empty()) {
        if (!EnsureRecordingFolderSelected(hMainWnd)) {
            return; // User cancelled
        }
    }

    std::string dateFolder = CreateDateFolder();
    if (dateFolder.empty()) return;

    if (pRecorder->StartStreaming(dateFolder)) {
        recordingStartTick = GetTickCount64();
        recordingStartTime = std::time(nullptr);
        lastVoiceTime = recordingStartTick;
        TransitionTo(State::RECORDING);
    }
}

// Force stop recording from external trigger (HTTP server)
void CallAutoRecorder::ForceStopRecording(const std::map<std::string, std::string>& metadata) {
    std::lock_guard<std::recursive_mutex> lk(stateMutex);
    if (!pRecorder) return;

    // If no recording is in progress (already saved by a back-to-back /start, or
    // never started), just merge the metadata for diagnostics and return.
    if (currentState.load() != State::RECORDING) {
        return;
    }

    if (!metadata.empty()) {
        for (const auto& kv : metadata) {
            currentCallMetadata[kv.first] = kv.second;
        }
    }

    TransitionTo(State::SAVING);
    pRecorder->Stop();

    ULONGLONG duration = GetTickCount64() - recordingStartTick;
    if (duration >= (DWORD)minCallDurationMs) {
        SaveCurrentRecording();
    } else {
        pRecorder->FinalizeStreaming("discarded.wav");
    }

    currentCallMetadata.clear();
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
    // Ensure strict sync with setting
    if (autoRecordCalls && g_CallRecorder && !g_CallRecorder->IsEnabled()) {
        g_CallRecorder->Enable();
    }
}

void CleanupCallRecorder() {
    if (g_CallRecorder) {
        delete g_CallRecorder;
        g_CallRecorder = nullptr;
    }
}
