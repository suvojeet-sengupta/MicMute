#include "globals.h"

// Variables
NOTIFYICONDATA nid;
HWND hMainWnd;
HWND hOverlayWnd = NULL;
HWND hMeterWnd = NULL;

bool isRunOnStartup = false;
bool showOverlay = false;
bool showMeter = false;

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
int levelHistoryIndex = 0;

// Color Definitions
COLORREF colorBg = RGB(30, 30, 40);
COLORREF colorText = RGB(220, 220, 230);
COLORREF colorMuted = RGB(239, 68, 68);
COLORREF colorLive = RGB(34, 197, 94);
COLORREF colorOverlayBgMuted = RGB(180, 40, 40);
COLORREF colorOverlayBgLive = RGB(30, 150, 60);
COLORREF colorMeterBg = RGB(25, 25, 35);

HBRUSH hBrushBg;
HBRUSH hBrushOverlayMuted;
HBRUSH hBrushOverlayLive;
HBRUSH hBrushMeterBg;

bool isDragging = false;
POINT dragStart;
POINT clickStart;
bool isMeterDragging = false;
POINT meterDragStart;
