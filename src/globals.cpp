#include "globals.h"

// Variables
NOTIFYICONDATA nid;
HWND hMainWnd;
HWND hOverlayWnd = nullptr;
HWND hMeterWnd = nullptr;

bool isRunOnStartup = false;
bool showOverlay = false;
bool showMeter = false;
bool showRecorder = false;
bool showNotifications = true; // Default true
std::string recordingFolder = "";
bool autoRecordCalls = false;


HFONT hFontTitle;
HFONT hFontStatus;
HFONT hFontNormal;
HFONT hFontSmall;
HFONT hFontOverlay;

DWORD lastToggleTime = 0;
int skipTimerCycles = 0;

HICON hIconMicOn = nullptr;
HICON hIconMicOff = nullptr;

int overlayOpacity = 220;

std::array<float, LEVEL_HISTORY_SIZE> levelHistory = {};
std::array<float, LEVEL_HISTORY_SIZE> speakerLevelHistory = {};
int levelHistoryIndex = 0;

// Color Definitions
// Color Definitions
// Modern Dark Theme
// Modern Dark Theme (Windows 11 Style)
COLORREF colorBg = RGB(32, 32, 32);         // Fallback if Mica fails
COLORREF colorSidebarBg = RGB(32, 32, 32);  // Transparent/Mica (Handled by DWM, this is fallback)
COLORREF colorText = RGB(240, 240, 240);    // Primary Text (Off-white)
COLORREF colorTextDim = RGB(160, 160, 160); // Secondary Text
COLORREF colorMuted = RGB(255, 69, 58);     // System Red (Windows 11)
COLORREF colorLive = RGB(50, 215, 75);      // System Green (Windows 11)
COLORREF colorOverlayBgMuted = RGB(30, 0, 0); // Subtle Dark Red
COLORREF colorOverlayBgLive = RGB(0, 30, 0);  // Subtle Dark Green
COLORREF colorMeterBg = RGB(20, 20, 20);    // Dark for Meter
COLORREF colorChroma = RGB(255, 0, 255);    // Magenta key

// Toggle Switch & Controls
COLORREF colorToggleBgOff = RGB(60, 60, 60);
COLORREF colorToggleBgOn = RGB(0, 120, 215);  // Windows Acccent Blue
COLORREF colorToggleCircle = RGB(255, 255, 255);

// Sidebar Interaction
COLORREF colorSidebarHover = RGB(50, 50, 50);    // Subtle highlight
COLORREF colorSidebarSelected = RGB(0, 120, 215); // Accent highlight

// New Customization Settings
std::string customTitle = "MicMute";
bool isTitleBold = true;
COLORREF customTitleColor = RGB(240, 240, 240);
BackgroundStyle backgroundStyle = BackgroundStyle::Mica;
COLORREF customBackgroundColor = RGB(32, 32, 32);

// Statistics
AppStats appStats = {0, 0, 0, ""};

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
    HDC hdc = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    return dpi / 96.0f;
}
