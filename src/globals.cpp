#include "globals.h"

// Variables
NOTIFYICONDATA nid;
HWND hMainWnd;
HWND hOverlayWnd = NULL;
HWND hMeterWnd = NULL;

bool isRunOnStartup = false;
bool showOverlay = false;
bool showMeter = false;
bool showRecorder = false;

HFONT hFontTitle;
HFONT hFontStatus;
HFONT hFontNormal;
HFONT hFontSmall;
HFONT hFontOverlay;

DWORD lastToggleTime = 0;
int skipTimerCycles = 0;

HICON hIconMicOn = NULL;
HICON hIconMicOff = NULL;

int overlayOpacity = 220;

float levelHistory[LEVEL_HISTORY_SIZE] = {0};
float speakerLevelHistory[LEVEL_HISTORY_SIZE] = {0};
int levelHistoryIndex = 0;

// Color Definitions
// Color Definitions
// Modern Dark Theme
COLORREF colorBg = RGB(32, 33, 36);        // Main content area (lighter dark)
COLORREF colorSidebarBg = RGB(25, 26, 29); // Sidebar (darker)
COLORREF colorText = RGB(232, 234, 237);   // High contrast text
COLORREF colorTextDim = RGB(154, 160, 166);// Dimmed text
COLORREF colorMuted = RGB(244, 67, 54);    // Material Red 500
COLORREF colorLive = RGB(76, 175, 80);     // Material Green 500
COLORREF colorOverlayBgMuted = RGB(180, 40, 40);
COLORREF colorOverlayBgLive = RGB(30, 150, 60);
COLORREF colorMeterBg = RGB(20, 20, 20);
COLORREF colorChroma = RGB(255, 0, 255); // Magenta

// Toggle Switch Colors
COLORREF colorToggleBgOff = RGB(80, 80, 80);
COLORREF colorToggleBgOn = RGB(76, 175, 80);
COLORREF colorToggleCircle = RGB(255, 255, 255);

// Sidebar Interaction
COLORREF colorSidebarHover = RGB(45, 46, 50);
COLORREF colorSidebarSelected = RGB(55, 56, 60);

HBRUSH hBrushBg;
HBRUSH hBrushSidebarBg;
HBRUSH hBrushOverlayMuted;
HBRUSH hBrushOverlayLive;
HBRUSH hBrushMeterBg;
HBRUSH hBrushChroma;
HBRUSH hBrushSidebarHover;
HBRUSH hBrushSidebarSelected;

bool isDragging = false;
POINT dragStart;
POINT clickStart;
bool isMeterDragging = false;
POINT meterDragStart;

float GetWindowScale(HWND hWnd) {
    // Try GetDpiForWindow (Win 10 1607+)
    static auto pGetDpiForWindow = (UINT(WINAPI*)(HWND))GetProcAddress(GetModuleHandle("User32.dll"), "GetDpiForWindow");
    if (pGetDpiForWindow && hWnd) {
         UINT dpi = pGetDpiForWindow(hWnd);
         return dpi / 96.0f;
    }
    
    // Fallback: System DPI
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);
    return dpi / 96.0f;
}
