#include "core/globals.h"

// Version
const char* APP_VERSION = "v1.2.0";

// Variables
NOTIFYICONDATA nid;
HWND hMainWnd;
HWND hOverlayWnd = nullptr;
HWND hMeterWnd = nullptr;
HWND hControlPanel = nullptr;

bool isRunOnStartup = false;
bool showOverlay = false;
bool showMeter = false;
bool showRecorder = false;
bool showNotifications = true;
std::string recordingFolder = "";
std::string userName = "Suvojeet Sengupta";
bool autoRecordCalls = false;
bool beepOnCall = true;

#define STRINGIZE_HELPER(x) #x
#define STRINGIZE(x) STRINGIZE_HELPER(x)

#ifndef APP_PASSWORD_SECRET
#define APP_PASSWORD_SECRET admin
#endif

// We use stringization to safely handle backslashes in passwords
const char* APP_PASSWORD = STRINGIZE(APP_PASSWORD_SECRET);
bool hasAgreedToDisclaimer = false;
bool hasAgreedToManualDisclaimer = false;
int autoDeleteDays = 0; // 0 = Never delete
int scrollY = 0;

// Control Panel visibility (all visible by default)
bool showMuteBtn = true;
bool showVoiceMeter = true;
bool showRecStatus = false;
bool showCallStats = true;
bool showManualRec = false;

// Panel size mode (0=Compact, 1=Normal, 2=Wide)
int panelSizeMode = 1;

HFONT hFontTitle;
HFONT hFontStatus;
HFONT hFontNormal;
HFONT hFontSmall;
HFONT hFontOverlay;
HFONT hFontBold;

DWORD lastToggleTime = 0;
int skipTimerCycles = 0;

HICON hIconMicOn = nullptr;
HICON hIconMicOff = nullptr;

int overlayOpacity = 220;

std::array<float, LEVEL_HISTORY_SIZE> levelHistory = {};
std::array<float, LEVEL_HISTORY_SIZE> speakerLevelHistory = {};
int levelHistoryIndex = 0;

// Refined Navy-Dark Theme
COLORREF colorBg = RGB(24, 24, 32);
COLORREF colorSidebarBg = RGB(20, 20, 28);
COLORREF colorText = RGB(245, 245, 250);
COLORREF colorTextDim = RGB(150, 150, 170);
COLORREF colorMuted = RGB(255, 82, 82);
COLORREF colorLive = RGB(76, 217, 100);
COLORREF colorOverlayBgMuted = RGB(40, 12, 12);
COLORREF colorOverlayBgLive = RGB(12, 40, 18);
COLORREF colorMeterBg = RGB(16, 16, 24);
COLORREF colorChroma = RGB(255, 0, 255);

// Toggle Switch & Controls
COLORREF colorToggleBgOff = RGB(55, 55, 70);
COLORREF colorToggleBgOn = RGB(98, 130, 255);
COLORREF colorToggleCircle = RGB(255, 255, 255);

// Sidebar Interaction
COLORREF colorSidebarHover = RGB(38, 38, 50);
COLORREF colorSidebarSelected = RGB(98, 130, 255);

// New accent & panel
COLORREF colorAccent = RGB(98, 130, 255);
COLORREF colorPanelBg = RGB(22, 22, 30);
COLORREF colorPanelBorder = RGB(50, 50, 70);

HBRUSH hBrushBg;
HBRUSH hBrushSidebarBg;
HBRUSH hBrushOverlayMuted;
HBRUSH hBrushOverlayLive;
HBRUSH hBrushMeterBg;
HBRUSH hBrushChroma;
HBRUSH hBrushSidebarHover;
HBRUSH hBrushSidebarSelected;
HBRUSH hBrushPanelBg;

bool isDragging = false;
POINT dragStart;
POINT clickStart;
bool isMeterDragging = false;
POINT meterDragStart;

float GetWindowScale(HWND hWnd) {
    static auto pGetDpiForWindow = (UINT(WINAPI*)(HWND))GetProcAddress(GetModuleHandle("User32.dll"), "GetDpiForWindow");
    if (pGetDpiForWindow && hWnd) {
         UINT dpi = pGetDpiForWindow(hWnd);
         return dpi / 96.0f;
    }
    
    HDC hdc = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    return dpi / 96.0f;
}
