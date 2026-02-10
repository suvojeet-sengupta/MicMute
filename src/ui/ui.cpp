#include "ui/ui.h"
#include "core/globals.h"
#include "core/settings.h"
#include "audio/audio.h"
#include "ui/tray.h"
#include "ui/overlay.h"
#include "core/resource.h"
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
    UpdateUIState();
}
