#include "ui_controls.h"
#include "globals.h"
#include "resource.h"

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
    
    // Background
    if (isSelected) {
        FillRect(hdc, &rcItem, hBrushSidebarSelected);
        // Draw Accent Line on Left
        RECT rcAccent = rcItem;
        rcAccent.right = 4;
        HBRUSH hAccent = CreateSolidBrush(colorLive); // Use green accent
        FillRect(hdc, &rcAccent, hAccent);
        DeleteObject(hAccent);
    } else if (isHover) {
        FillRect(hdc, &rcItem, hBrushSidebarHover);
    }
    
    // Text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, isSelected ? colorText : (isHover ? colorText : colorTextDim));
    SelectObject(hdc, hFontNormal);
    
    // Center Text Vertically, Left Align with Padding
    RECT rcText = rcItem;
    rcText.left += 15;
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
    
    // Clear Background
    FillRect(hdc, &rc, hBrushBg);
    
    // Calculate Toggle Switch Area
    RECT rcSwitch;
    rcSwitch.left = rc.left;
    rcSwitch.top = rc.top + (rc.bottom - rc.top - TOGGLE_HEIGHT) / 2;
    rcSwitch.right = rcSwitch.left + TOGGLE_WIDTH;
    rcSwitch.bottom = rcSwitch.top + TOGGLE_HEIGHT;
    
    // Draw Track
    DrawRoundedRect(hdc, rcSwitch, TOGGLE_HEIGHT, isChecked ? colorToggleBgOn : colorToggleBgOff);
    
    // Draw Thumb (Circle)
    int circleSize = TOGGLE_HEIGHT - 4;
    int circleX = isChecked ? (rcSwitch.right - 2 - circleSize) : (rcSwitch.left + 2);
    int circleY = rcSwitch.top + 2;
    RECT rcCircle = {circleX, circleY, circleX + circleSize, circleY + circleSize};
    DrawRoundedRect(hdc, rcCircle, circleSize, colorToggleCircle);
    
    // Draw Label
    RECT rcLabel = rc;
    rcLabel.left += TOGGLE_WIDTH + 10;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, colorText);
    SelectObject(hdc, hFontNormal);
    
    // Get Window Text
    char buf[256];
    GetWindowText(lpDrawItem->hwndItem, buf, 256);
    DrawText(hdc, buf, -1, &rcLabel, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
}
