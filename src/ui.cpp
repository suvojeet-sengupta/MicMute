#include "ui.h"
#include "globals.h"
#include "settings.h"
#include "audio.h"
#include "tray.h"
#include "overlay.h"
#include "resource.h"
#include <dwmapi.h>

void UpdateUIState() {
    static bool lastMuted = -1; // -1 for uninitialized
    bool muted = IsDefaultMicMuted();
    
    if (muted != lastMuted) {
        lastMuted = muted;
        UpdateTrayIcon(muted);
        UpdateOverlay();

        HWND hStatus = GetDlgItem(hMainWnd, ID_STATUS_LABEL);
        if (hStatus) {
            SetWindowText(hStatus, muted ? "MICROPHONE MUTED" : "MICROPHONE LIVE");
            InvalidateRect(hStatus, NULL, TRUE);
        }
    }
}

void ToggleMute() {
    // Fade out animation
    if (hOverlayWnd && IsWindowVisible(hOverlayWnd)) {
        for (int i = overlayOpacity; i >= 100; i -= 30) {
            SetLayeredWindowAttributes(hOverlayWnd, colorChroma, (BYTE)i, LWA_ALPHA | LWA_COLORKEY);
            Sleep(15);
        }
    }
    
    ToggleMuteAll();
    UpdateUIState();
    
    // Fade in animation
    if (hOverlayWnd && IsWindowVisible(hOverlayWnd)) {
        for (int i = 100; i <= overlayOpacity; i += 30) {
            SetLayeredWindowAttributes(hOverlayWnd, colorChroma, (BYTE)i, LWA_ALPHA | LWA_COLORKEY);
            Sleep(15);
        }
        SetLayeredWindowAttributes(hOverlayWnd, colorChroma, (BYTE)overlayOpacity, LWA_ALPHA | LWA_COLORKEY);
    }
}
