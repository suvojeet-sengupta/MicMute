#pragma once
#include <windows.h>
#include <shellapi.h>

// Global Variables
extern NOTIFYICONDATA nid;
extern HWND hMainWnd;
extern HWND hOverlayWnd;
extern HWND hMeterWnd;

extern bool isRunOnStartup;
extern bool showOverlay;
extern bool showMeter;

extern HFONT hFontTitle;
extern HFONT hFontStatus;
extern HFONT hFontNormal;
extern HFONT hFontSmall;
extern HFONT hFontOverlay;

extern DWORD lastToggleTime;
extern int skipTimerCycles;

// Cached icons
extern HICON hIconMicOn;
extern HICON hIconMicOff;

// Overlay animation
extern int overlayOpacity;

// Level history for waveform
#define LEVEL_HISTORY_SIZE 30
extern float levelHistory[LEVEL_HISTORY_SIZE];
extern int levelHistoryIndex;

// Colors
extern COLORREF colorBg;
extern COLORREF colorText;
extern COLORREF colorMuted;
extern COLORREF colorLive;
extern COLORREF colorOverlayBgMuted;
extern COLORREF colorOverlayBgLive;
extern COLORREF colorMeterBg;
extern COLORREF colorChroma;

extern HBRUSH hBrushBg;
extern HBRUSH hBrushOverlayMuted;
extern HBRUSH hBrushOverlayLive;
extern HBRUSH hBrushMeterBg;
extern HBRUSH hBrushChroma;

// Dragging
extern bool isDragging;
extern POINT dragStart;
extern POINT clickStart;
extern bool isMeterDragging;
extern POINT meterDragStart;
