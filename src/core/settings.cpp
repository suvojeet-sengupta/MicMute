#include "core/settings.h"
#include "core/globals.h"
#include "ui/control_panel.h"
#include <cstdio>

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
    *w = 180; *h = 100;
    
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
        DWORD val;
        
        val = showOverlay ? 1 : 0;
        RegSetValueEx(hKey, "ShowOverlay", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = showMeter ? 1 : 0;
        RegSetValueEx(hKey, "ShowMeter", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = showRecorder ? 1 : 0;
        RegSetValueEx(hKey, "ShowRecorder", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = showNotifications ? 1 : 0;
        RegSetValueEx(hKey, "ShowNotifications", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = autoRecordCalls ? 1 : 0;
        RegSetValueEx(hKey, "AutoRecordCalls", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = beepOnCall ? 1 : 0;
        RegSetValueEx(hKey, "BeepOnCall", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        
        val = hasAgreedToDisclaimer ? 1 : 0;
        RegSetValueEx(hKey, "DisclaimerAgreed", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        
        val = hasAgreedToManualDisclaimer ? 1 : 0;
        RegSetValueEx(hKey, "ManualDisclaimerAgreed", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        
        val = (DWORD)autoDeleteDays;
        RegSetValueEx(hKey, "AutoDeleteDays", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        
        // Control panel visibility toggles
        val = showMuteBtn ? 1 : 0;
        RegSetValueEx(hKey, "ShowMuteBtn", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = showVoiceMeter ? 1 : 0;
        RegSetValueEx(hKey, "ShowVoiceMeter", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = showRecStatus ? 1 : 0;
        RegSetValueEx(hKey, "ShowRecStatus", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = showCallStats ? 1 : 0;
        RegSetValueEx(hKey, "ShowCallStats", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = showManualRec ? 1 : 0;
        RegSetValueEx(hKey, "ShowManualRec", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        
        // Panel size mode
        val = (DWORD)panelSizeMode;
        RegSetValueEx(hKey, "PanelSizeMode", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        
        // User name
        if (!userName.empty()) {
            RegSetValueEx(hKey, "UserName", 0, REG_SZ, (const BYTE*)userName.c_str(), (DWORD)(userName.length() + 1));
        }
        
        if (!recordingFolder.empty()) {
            RegSetValueEx(hKey, "RecordingFolder", 0, REG_SZ, (const BYTE*)recordingFolder.c_str(), (DWORD)(recordingFolder.length() + 1));
        }

        // Save window positions
        SaveOverlayPosition();
        SaveMeterPosition();
        SaveControlPanelPosition();
        
        RegCloseKey(hKey);
    }
}

void LoadSettings() {
    isRunOnStartup = IsStartupEnabled();
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD), val = 0;
        
        if (RegQueryValueEx(hKey, "ShowOverlay", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS) {
            showOverlay = val != 0;
            // Legacy mapping
            showMuteBtn = showOverlay;
        }
        if (RegQueryValueEx(hKey, "ShowMeter", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS) {
            showMeter = val != 0;
            // Legacy mapping
            showVoiceMeter = showMeter;
        }
        if (RegQueryValueEx(hKey, "ShowRecorder", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS) {
            showRecorder = val != 0;
            // Legacy mapping
            showRecStatus = showRecorder;
            showManualRec = showRecorder;
        }
        if (RegQueryValueEx(hKey, "ShowNotifications", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showNotifications = val != 0;
        if (RegQueryValueEx(hKey, "AutoRecordCalls", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            autoRecordCalls = val != 0;
        if (RegQueryValueEx(hKey, "BeepOnCall", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            beepOnCall = val != 0;
        if (RegQueryValueEx(hKey, "DisclaimerAgreed", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            hasAgreedToDisclaimer = val != 0;
        if (RegQueryValueEx(hKey, "ManualDisclaimerAgreed", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            hasAgreedToManualDisclaimer = val != 0;
        if (RegQueryValueEx(hKey, "AutoDeleteDays", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            autoDeleteDays = (int)val;
        
        // Control panel visibility toggles (override legacy if present)
        size = sizeof(DWORD);
        if (RegQueryValueEx(hKey, "ShowMuteBtn", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showMuteBtn = val != 0;
        if (RegQueryValueEx(hKey, "ShowVoiceMeter", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showVoiceMeter = val != 0;
        if (RegQueryValueEx(hKey, "ShowRecStatus", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showRecStatus = val != 0;
        if (RegQueryValueEx(hKey, "ShowCallStats", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showCallStats = val != 0;
        if (RegQueryValueEx(hKey, "ShowManualRec", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showManualRec = val != 0;
        
        // Panel size mode
        size = sizeof(DWORD);
        if (RegQueryValueEx(hKey, "PanelSizeMode", nullptr, nullptr, (BYTE*)&val, &size) == ERROR_SUCCESS) {
            panelSizeMode = (int)val;
            if (panelSizeMode < 0 || panelSizeMode > 2) panelSizeMode = 1;
        }
        
        // User name
        char nameBuf[256] = {0};
        DWORD nameSize = sizeof(nameBuf);
        if (RegQueryValueEx(hKey, "UserName", nullptr, nullptr, (BYTE*)nameBuf, &nameSize) == ERROR_SUCCESS) {
            userName = nameBuf;
        }
            
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
