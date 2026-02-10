#include "ui/ui_controls.h"
#include "core/globals.h"
#include "core/resource.h"

void DrawRoundedRect(HDC hdc, RECT rc, int radius, COLORREF color) {
    HBRUSH hBrush = CreateSolidBrush(color);
    HBRUSH _old = (HBRUSH)SelectObject(hdc, hBrush);
    HPEN hPen = CreatePen(PS_NULL, 0, 0);
    HPEN _oldPen = (HPEN)SelectObject(hdc, hPen);
    
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    
    SelectObject(hdc, _oldPen);
    SelectObject(hdc, _old);
    DeleteObject(hPen);
    DeleteObject(hBrush);
}

void DrawSidebar(HDC hdc, RECT clientRect, int selectedTab) {
    RECT sideRect = clientRect;
    sideRect.right = SIDEBAR_WIDTH;
    
    // Fill Sidebar Background
    FillRect(hdc, &sideRect, hBrushSidebarBg);
}

void DrawSidebarItem(HDC hdc, int index, const char* text, int selectedTab, int hoverTab) {
    RECT rcItem = {0, 100 + (index * SIDEBAR_ITEM_HEIGHT), SIDEBAR_WIDTH, 100 + ((index + 1) * SIDEBAR_ITEM_HEIGHT)};
    
    bool isSelected = (index == selectedTab);
    bool isHover = (index == hoverTab);
    
    // Windows 11 Style: Rounded Selection/Hover Indicator
    // We want a little padding so it doesn't touch edges
    RECT rcBg = rcItem;
    rcBg.left += 4;
    rcBg.right -= 4;
    rcBg.top += 2;
    rcBg.bottom -= 2;

    if (isSelected) {
        DrawRoundedRect(hdc, rcBg, 4, colorSidebarSelected);
        
        // Indicator Line is usually not present in Win11 Settings in the same way, 
        // but let's add a small vertical accent on the left *inside* the selection area or just rely on color
        // Let's rely on the background color change + accent colored text or icon. 
        // For mic mute, let's keep it simple: just the background highlight.
        // Actually, let's add a small colored pill on the left for clarity if user wants.
        // For now, clean rounded rect is best.
    } else if (isHover) {
        DrawRoundedRect(hdc, rcBg, 4, colorSidebarHover);
    }
    
    // Text
    SetBkMode(hdc, TRANSPARENT);
    // If selected, use accent color or white? Win 11 keeps it white/text color but bolder.
    // Let's stick to White for improved contrast on dark.
    SetTextColor(hdc, isSelected ? RGB(255, 255, 255) : (isHover ? RGB(240, 240, 240) : colorTextDim));
    
    // Font: Bold if selected
    SelectObject(hdc, isSelected ? hFontStatus : hFontNormal); // Re-using hFontStatus as it's SemiBold/Bold-ish
    
    // Center Text Vertically, Left Align with Padding
    RECT rcText = rcItem;
    rcText.left += 16;
    DrawText(hdc, text, -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
}

void DrawToggle(LPDRAWITEMSTRUCT lpDrawItem) {
    HDC hdc = lpDrawItem->hDC;
    RECT rc = lpDrawItem->rcItem;
    int id = lpDrawItem->CtlID;
    
    bool isChecked = false;
    if (id == ID_RUN_STARTUP) isChecked = isRunOnStartup;
    else if (id == ID_SHOW_OVERLAY) isChecked = showOverlay;
    else if (id == ID_SHOW_METER) isChecked = showMeter;
    else if (id == ID_SHOW_RECORDER) isChecked = showRecorder;
    else if (id == ID_SHOW_NOTIFICATIONS) isChecked = showNotifications;
    else if (id == ID_AUTO_RECORD_CALLS) isChecked = autoRecordCalls;
    else if (id == ID_BEEP_ON_CALL) isChecked = beepOnCall;
    else if (id == ID_HIDE_MUTE_BTN) isChecked = showMuteBtn;
    else if (id == ID_HIDE_VOICE_METER) isChecked = showVoiceMeter;
    else if (id == ID_HIDE_REC_STATUS) isChecked = showRecStatus;
    else if (id == ID_HIDE_CALL_STATS) isChecked = showCallStats;
    else if (id == ID_HIDE_MANUAL_REC) isChecked = showManualRec;

    
    // Clear Background - use parent's background for Mica effect
    // Get parent DC to maintain transparency
    HWND hParent = GetParent(lpDrawItem->hwndItem);
    if (hParent) {
        // Request parent repaint area behind us for proper layering
        POINT pt = {rc.left, rc.top};
        MapWindowPoints(lpDrawItem->hwndItem, hParent, &pt, 1);
        RECT rcParent = {pt.x, pt.y, pt.x + (rc.right - rc.left), pt.y + (rc.bottom - rc.top)};
        // Use hollow brush to show parent background
    }
    // Fill with semi-transparent background for visibility
    FillRect(hdc, &rc, hBrushBg); 
    
    // Calculate Toggle Switch Area
    // Windows 11 Toggles are wider and pill shaped
    // Width ~ 40px, Height ~ 20px
    RECT rcSwitch;
    rcSwitch.left = rc.left; // Aligned left
    rcSwitch.top = rc.top + (rc.bottom - rc.top - TOGGLE_HEIGHT) / 2;
    rcSwitch.right = rcSwitch.left + TOGGLE_WIDTH;
    rcSwitch.bottom = rcSwitch.top + TOGGLE_HEIGHT;
    
    // Draw Track (Pill)
    // On: Accent Color (Solid)
    // Off: Border with transparent/dark fill OR solid dark grey. Win11 dark mode uses solid dark grey.
    
    COLORREF trackColor = isChecked ? colorToggleBgOn : colorToggleBgOff;
    COLORREF thumbColor = isChecked ? RGB(20, 20, 20) : RGB(200, 200, 200); // Win11 Dark Mode: Thumb is dark when on (contrast) or white?
    // Correction: Win11 Dark Mode Toggle:
    // ON: Accent Color Background, White Thumb (usually) or Black Thumb. Default standard is White/Light thumb on Accent.
    // OFF: Dark Grey Background, Light Grey Thumb.
    
    thumbColor = isChecked ? RGB(255, 255, 255) : RGB(200, 200, 200); // Let's keep thumb light
    if (isChecked) trackColor = colorToggleBgOn; // Accent
    else trackColor = colorToggleBgOff; // Dark Grey
    
    // Draw Track
    DrawRoundedRect(hdc, rcSwitch, TOGGLE_HEIGHT, trackColor);
    
    // Draw Thumb (Circle)
    int circleSize = TOGGLE_HEIGHT - 6; // slightly smaller than height
    int circleX = isChecked ? (rcSwitch.right - 3 - circleSize) : (rcSwitch.left + 3);
    int circleY = rcSwitch.top + 3;
    
    RECT rcCircle = {circleX, circleY, circleX + circleSize, circleY + circleSize};
    DrawRoundedRect(hdc, rcCircle, circleSize, thumbColor);
    
    // Draw Label
    RECT rcLabel = rc;
    rcLabel.left += TOGGLE_WIDTH + 12; // Gap
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, colorText);
    SelectObject(hdc, hFontNormal);
    
    // Get Window Text
    char buf[256];
    GetWindowText(lpDrawItem->hwndItem, buf, 256);
    DrawText(hdc, buf, -1, &rcLabel, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
}
