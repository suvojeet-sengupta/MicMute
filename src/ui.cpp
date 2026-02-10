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
        ShowTrayNotification(muted); // Trigger Toast

        // Invalidate main window to redraw status text
        InvalidateRect(hMainWnd, nullptr, TRUE);
    }
}

void ToggleMute() {
    ToggleMuteAll();
    
    // Stats
    appStats.mutesToday++;
    SaveStats(); // Periodically save? Or just rely on end session.
    // Saving every time might be too much I/O, but for now it ensures safety.

    UpdateUIState();
}
