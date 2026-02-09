#include "settings.h"
#include "globals.h"
#include <cstdio> // For nullptr

void SaveOverlayPosition() {
    if (!hOverlayWnd) return;
    RECT rect; GetWindowRect(hOverlayWnd, &rect);
    HKEY hKey;
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, "OverlayX", 0, REG_DWORD, (BYTE*)&rect.left, sizeof(DWORD));
        RegSetValueEx(hKey, "OverlayY", 0, REG_DWORD, (BYTE*)&rect.top, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

void LoadOverlayPosition(int* x, int* y) {
    *x = 50; *y = 50;
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        RegQueryValueEx(hKey, "OverlayX", nullptr, nullptr, (BYTE*)x, &size);
        RegQueryValueEx(hKey, "OverlayY", nullptr, nullptr, (BYTE*)y, &size);
        RegCloseKey(hKey);
    }
}

void SaveMeterPosition() {
    if (!hMeterWnd) return;
    RECT rect; GetWindowRect(hMeterWnd, &rect);
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    
    HKEY hKey;
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, "MeterX", 0, REG_DWORD, (BYTE*)&rect.left, sizeof(DWORD));
        RegSetValueEx(hKey, "MeterY", 0, REG_DWORD, (BYTE*)&rect.top, sizeof(DWORD));
        RegSetValueEx(hKey, "MeterW", 0, REG_DWORD, (BYTE*)&w, sizeof(DWORD));
        RegSetValueEx(hKey, "MeterH", 0, REG_DWORD, (BYTE*)&h, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

void LoadMeterPosition(int* x, int* y, int* w, int* h) {
    *x = 50; *y = 100;
    *w = 180; *h = 100; // Defaults
    
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        RegQueryValueEx(hKey, "MeterX", nullptr, nullptr, (BYTE*)x, &size);
        RegQueryValueEx(hKey, "MeterY", nullptr, nullptr, (BYTE*)y, &size);
        RegQueryValueEx(hKey, "MeterW", nullptr, nullptr, (BYTE*)w, &size);
        RegQueryValueEx(hKey, "MeterH", nullptr, nullptr, (BYTE*)h, &size);
        RegCloseKey(hKey);
    }
}

void SaveSettings() {
    HKEY hKey;
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD val = showOverlay ? 1 : 0;
        RegSetValueEx(hKey, "ShowOverlay", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = showMeter ? 1 : 0;
        RegSetValueEx(hKey, "ShowMeter", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = showRecorder ? 1 : 0;
        RegSetValueEx(hKey, "ShowRecorder", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = showNotifications ? 1 : 0;
        RegSetValueEx(hKey, "ShowNotifications", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = autoRecordCalls ? 1 : 0;
        RegSetValueEx(hKey, "AutoRecordCalls", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        
        if (!recordingFolder.empty()) {
            RegSetValueEx(hKey, "RecordingFolder", 0, REG_SZ, (const BYTE*)recordingFolder.c_str(), (DWORD)(recordingFolder.length() + 1));
        }

        // Save Window Positions on global save
        SaveOverlayPosition();
        SaveMeterPosition();
        // Recorder position is handled in recorder.cpp, but we should ensure it's called
        // We need a forward declaration or include, but for now relying on individual window saves is okay IF they are called.
        // Better: Call them here if windows are valid, OR rely on WM_DESTROY/WM_ENDSESSION in main.cpp to call individual save functions.
        
        RegCloseKey(hKey);
    }
}

void LoadSettings() {
    isRunOnStartup = IsStartupEnabled();
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD), val = 0;
        if (RegQueryValueEx(hKey, "ShowOverlay", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showOverlay = val != 0;
        if (RegQueryValueEx(hKey, "ShowMeter", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showMeter = val != 0;
        if (RegQueryValueEx(hKey, "ShowRecorder", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showRecorder = val != 0;
        if (RegQueryValueEx(hKey, "ShowNotifications", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showNotifications = val != 0;
        if (RegQueryValueEx(hKey, "AutoRecordCalls", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            autoRecordCalls = val != 0;
            
        char buffer[MAX_PATH] = {0};
        DWORD bufSize = sizeof(buffer);
        if (RegQueryValueEx(hKey, "RecordingFolder", nullptr, nullptr, (BYTE*)buffer, &bufSize) == ERROR_SUCCESS) {
            recordingFolder = buffer;
        }

        RegCloseKey(hKey);
    }
}

void ManageStartup(bool enable) {
    HKEY hKey;
    const char* path = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    long result = RegOpenKeyEx(HKEY_CURRENT_USER, path, 0, KEY_SET_VALUE, &hKey);
    
    if (result == ERROR_SUCCESS) {
        if (enable) {
            char exe[MAX_PATH];
            GetModuleFileName(nullptr, exe, MAX_PATH);
            // Robustness: Always update the path in case EXE moved
            RegSetValueEx(hKey, "MicMute-S", 0, REG_SZ, (BYTE*)exe, (DWORD)(strlen(exe) + 1));
        } else {
            RegDeleteValue(hKey, "MicMute-S");
        }
        RegCloseKey(hKey);
    }
}

bool IsStartupEnabled() {
    HKEY hKey;
    bool exists = false;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, "MicMute-S", nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
            exists = true;
        RegCloseKey(hKey);
    }
    return exists;
}
