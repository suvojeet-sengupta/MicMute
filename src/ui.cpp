#include "ui.h"
#include "globals.h"
#include "settings.h"
#include "audio.h"
#include "tray.h"
#include "overlay.h"
#include "resource.h"
#include <dwmapi.h>

void UpdateUIState() {
    static int lastMuted = -1; // -1 for uninitialized
    bool muted = IsDefaultMicMuted();
    
    if ((int)muted != lastMuted) {
        lastMuted = (int)muted;
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
    ToggleMuteAll();
    UpdateUIState();
}
