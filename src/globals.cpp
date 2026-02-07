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
bool showNotifications = true; // Default true
std::string recordingFolder = "";


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
// Modern Dark Theme (Windows 11 Style)
COLORREF colorBg = RGB(32, 32, 32);         // Mica Fallback
COLORREF colorSidebarBg = RGB(32, 32, 32);  // Transparent/Mica
COLORREF colorText = RGB(255, 255, 255);    // Primary Text
COLORREF colorTextDim = RGB(160, 160, 160); // Secondary Text
COLORREF colorMuted = RGB(255, 69, 58);     // System Red
COLORREF colorLive = RGB(48, 209, 88);      // System Green
COLORREF colorOverlayBgMuted = RGB(50, 0, 0); // Darker Red for Overlay
COLORREF colorOverlayBgLive = RGB(0, 50, 0);  // Darker Green for Overlay
COLORREF colorMeterBg = RGB(20, 20, 20);
COLORREF colorChroma = RGB(255, 0, 255);    // Magenta

// Toggle Switch & Controls
COLORREF colorToggleBgOff = RGB(60, 60, 60);
COLORREF colorToggleBgOn = RGB(96, 205, 255); // System Accent (Blue)
COLORREF colorToggleCircle = RGB(255, 255, 255); // Thumb

// Sidebar Interaction
COLORREF colorSidebarHover = RGB(55, 55, 55);    // Subtle highlight
COLORREF colorSidebarSelected = RGB(70, 70, 70); // Stronger highlight

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
