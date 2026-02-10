#include "ui/tray.h"
#include "core/globals.h"
#include "core/resource.h"
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

void ShowTrayNotification(bool isMuted) {
    if (!showNotifications) return;

    nid.uFlags |= NIF_INFO;
    strcpy_s(nid.szInfoTitle, isMuted ? "Microphone Muted" : "Microphone Live");
    strcpy_s(nid.szInfo, isMuted ? "Your microphone has been muted." : "Your microphone is now live.");
    nid.dwInfoFlags = isMuted ? NIIF_WARNING : NIIF_INFO; // Warning icon for mute (red-ish usually), Info for live
    // NIIF_WARNING is technically a yellow yield sign, NIIF_ERROR is red X.
    // Let's use NIIF_INFO for both or NIIF_NONE to avoid confusing icons. 
    // Or NIIF_USER if we had a custom icon. Win 10/11 toasts show the app icon anyway.
    nid.dwInfoFlags = NIIF_NOSOUND | (isMuted ? NIIF_WARNING : NIIF_INFO);

    Shell_NotifyIcon(NIM_MODIFY, &nid);
    
    // Reset flags
    nid.uFlags &= ~NIF_INFO;
    nid.szInfo[0] = '\0';
    nid.szInfoTitle[0] = '\0';
}
