#include "tray.h"
#include "globals.h"
#include "resource.h"
#include <shellapi.h>
#include <cstdio> // strcpy_s

void AddTrayIcon(HWND hWnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hIconMicOn;  // Use cached icon
    strcpy_s(nid.szTip, "MicMute-S");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void UpdateTrayIcon(bool isMuted) {
    nid.hIcon = isMuted ? hIconMicOff : hIconMicOn;  // Use cached icons
    strcpy_s(nid.szTip, isMuted ? "MicMute-S (Muted)" : "MicMute-S (Live)");
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}
