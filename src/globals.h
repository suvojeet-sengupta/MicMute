#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <array>

// Global Variables
extern std::string recordingFolder;
extern NOTIFYICONDATA nid;
extern HWND hMainWnd;
extern HWND hOverlayWnd;
extern HWND hMeterWnd;

extern bool isRunOnStartup;
extern bool showOverlay;
extern bool showMeter;
extern bool showRecorder;
extern bool showNotifications;
extern bool autoRecordCalls;

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
constexpr size_t LEVEL_HISTORY_SIZE = 60;
extern std::array<float, LEVEL_HISTORY_SIZE> levelHistory;
extern std::array<float, LEVEL_HISTORY_SIZE> speakerLevelHistory;
extern int levelHistoryIndex;

// Colors
extern COLORREF colorBg;
extern COLORREF colorSidebarBg; 
extern COLORREF colorText;
extern COLORREF colorTextDim;
extern COLORREF colorMuted;
extern COLORREF colorLive;
extern COLORREF colorOverlayBgMuted;
extern COLORREF colorOverlayBgLive;
extern COLORREF colorMeterBg;
extern COLORREF colorChroma;
extern COLORREF colorToggleBgOff;
extern COLORREF colorToggleBgOn;
extern COLORREF colorToggleCircle;
extern COLORREF colorSidebarHover;
extern COLORREF colorSidebarSelected;

// New Customization Settings
enum class BackgroundStyle {
    Mica,
    Solid,
    Custom
};

extern std::string customTitle;
extern bool isTitleBold;
extern COLORREF customTitleColor;
extern BackgroundStyle backgroundStyle;
extern COLORREF customBackgroundColor;

// Statistics
struct AppStats {
    int callsToday;
    int callsTotal;
    int mutesToday;
    std::string lastResetDate; // YYYY-MM-DD
};
extern AppStats appStats;

extern HBRUSH hBrushBg;
extern HBRUSH hBrushSidebarBg;
extern HBRUSH hBrushOverlayMuted;
extern HBRUSH hBrushOverlayLive;
extern HBRUSH hBrushMeterBg;
extern HBRUSH hBrushChroma;
extern HBRUSH hBrushSidebarHover;
extern HBRUSH hBrushSidebarSelected;

// Dragging
extern bool isDragging;
extern POINT dragStart;
extern POINT clickStart;
extern bool isMeterDragging;
extern POINT meterDragStart;

// DPI Helper
float GetWindowScale(HWND hWnd);
