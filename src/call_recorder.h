#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <atomic>
#include <ctime>

// Forward declaration
class WasapiRecorder;

// Auto Call Recorder with Voice Activity Detection
class CallAutoRecorder {
public:
    // Recording States
    enum class State {
        IDLE,       // Not detecting, waiting for enable
        DETECTING,  // Monitoring audio levels for voice
        RECORDING,  // Currently recording a call
        SAVING      // Saving the recording, brief transition state
    };

    CallAutoRecorder();
    ~CallAutoRecorder();

    // Enable/Disable auto recording
    void Enable();
    void Disable();
    bool IsEnabled() const { return enabled; }

    // Called periodically from UI timer (~100ms - now only for UI updates, no VAD)
    void Poll();

    // Get current state
    State GetState() const { return currentState; }
    
    // Force start/stop from external source (HTTP server)
    void ForceStartRecording();
    void ForceStopRecording();
    
    // Statistics
    int GetTodayCallCount() const { return todayCallCount; }
    std::string GetCurrentDateFolder() const;

    // Configuration
    void SetVoiceThreshold(float threshold) { voiceThreshold = threshold; }
    float GetVoiceThreshold() const { return voiceThreshold; }
    
    void SetSilenceTimeoutMs(int ms) { silenceTimeoutMs = ms; }
    int GetSilenceTimeoutMs() const { return silenceTimeoutMs; }
    
    void SetMinCallDurationMs(int ms) { minCallDurationMs = ms; }
    int GetMinCallDurationMs() const { return minCallDurationMs; }
    
    // Grace period: no silence detection during initial call period
    void SetGracePeriodMs(int ms) { gracePeriodMs = ms; }
    int GetGracePeriodMs() const { return gracePeriodMs; }

private:
    void TransitionTo(State newState);
    void OnVoiceDetected();
    void OnSilenceTimeout();
    
    std::string CreateDateFolder();
    std::string GetNextFileName(int count);
    void SaveCurrentRecording();
    void CreateMetadataFile(const std::string& audioPath, time_t startTime, time_t endTime);

    // State
    std::atomic<bool> enabled;
    State currentState;
    
    // Timing
    DWORD lastVoiceTime;      // Last time voice was detected
    DWORD recordingStartTick; // When current recording started
    time_t recordingStartTime;// CalendarCall start time
    
    // Configuration
    float voiceThreshold;     // Audio level threshold (0.0-1.0)
    int silenceTimeoutMs;     // Silence duration to end call
    int minCallDurationMs;    // Minimum call duration to save
    int gracePeriodMs;        // No silence check during this initial period
    
    // Statistics
    int todayCallCount;
    std::string currentDate;  // YYYY-MM-DD format
    
    // Recorder instance (uses existing WasapiRecorder)
    WasapiRecorder* pRecorder;
};

// Global instance
extern CallAutoRecorder* g_CallRecorder;

// Helper functions
void InitCallRecorder();
void CleanupCallRecorder();
