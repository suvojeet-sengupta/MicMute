#include "ui.h"
#include "globals.h"
#include "settings.h"
#include "audio.h"
#include "tray.h"
#include "overlay.h"
#include "resource.h"
#include <dwmapi.h>

void UpdateUIState() {
    bool muted = IsDefaultMicMuted();
    UpdateTrayIcon(muted);
    UpdateOverlay();

    HWND hStatus = GetDlgItem(hMainWnd, ID_STATUS_LABEL);
    if (hStatus) {
        SetWindowText(hStatus, muted ? "MICROPHONE MUTED" : "MICROPHONE LIVE");
        InvalidateRect(hStatus, NULL, TRUE);
    }
}

void ToggleMute() {
    // Fade out animation
    if (hOverlayWnd && IsWindowVisible(hOverlayWnd)) {
        for (int i = overlayOpacity; i >= 100; i -= 30) {
            SetLayeredWindowAttributes(hOverlayWnd, 0, (BYTE)i, LWA_ALPHA);
            Sleep(15);
        }
    }
    
    ToggleMuteAll();
    UpdateUIState();
    
    // Fade in animation
    if (hOverlayWnd && IsWindowVisible(hOverlayWnd)) {
        for (int i = 100; i <= overlayOpacity; i += 30) {
            SetLayeredWindowAttributes(hOverlayWnd, 0, (BYTE)i, LWA_ALPHA);
            Sleep(15);
        }
        SetLayeredWindowAttributes(hOverlayWnd, 0, (BYTE)overlayOpacity, LWA_ALPHA);
    }
}
