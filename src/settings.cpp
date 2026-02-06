#include "settings.h"
#include "globals.h"
#include <cstdio> // For NULL

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
        RegQueryValueEx(hKey, "OverlayX", NULL, NULL, (BYTE*)x, &size);
        RegQueryValueEx(hKey, "OverlayY", NULL, NULL, (BYTE*)y, &size);
        RegCloseKey(hKey);
    }
}

void SaveMeterPosition() {
    if (!hMeterWnd) return;
    RECT rect; GetWindowRect(hMeterWnd, &rect);
    HKEY hKey;
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, "MeterX", 0, REG_DWORD, (BYTE*)&rect.left, sizeof(DWORD));
        RegSetValueEx(hKey, "MeterY", 0, REG_DWORD, (BYTE*)&rect.top, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

void LoadMeterPosition(int* x, int* y) {
    *x = 50; *y = 100;
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        RegQueryValueEx(hKey, "MeterX", NULL, NULL, (BYTE*)x, &size);
        RegQueryValueEx(hKey, "MeterY", NULL, NULL, (BYTE*)y, &size);
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
        RegCloseKey(hKey);
    }
}

void LoadSettings() {
    isRunOnStartup = IsStartupEnabled();
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD), val = 0;
        if (RegQueryValueEx(hKey, "ShowOverlay", NULL, NULL, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showOverlay = val != 0;
        if (RegQueryValueEx(hKey, "ShowMeter", NULL, NULL, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showMeter = val != 0;
        if (RegQueryValueEx(hKey, "ShowRecorder", NULL, NULL, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showRecorder = val != 0;
        if (RegQueryValueEx(hKey, "ShowNotifications", NULL, NULL, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showNotifications = val != 0;
        RegCloseKey(hKey);
    }
}

void ManageStartup(bool enable) {
    HKEY hKey;
    const char* path = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKey(HKEY_CURRENT_USER, path, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            char exe[MAX_PATH];
            GetModuleFileName(NULL, exe, MAX_PATH);
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
        if (RegQueryValueEx(hKey, "MicMute-S", NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
            exists = true;
        RegCloseKey(hKey);
    }
    return exists;
}
