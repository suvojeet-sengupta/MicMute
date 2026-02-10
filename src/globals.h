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
extern std::string userName;
extern NOTIFYICONDATA nid;
extern HWND hMainWnd;
extern HWND hOverlayWnd;
extern HWND hMeterWnd;
extern HWND hControlPanel;

extern bool isRunOnStartup;
extern bool showOverlay;
extern bool showMeter;
extern bool showRecorder;
extern bool showNotifications;
extern bool autoRecordCalls;

// Control Panel visibility toggles
extern bool showMuteBtn;
extern bool showVoiceMeter;
extern bool showRecStatus;
extern bool showCallStats;
extern bool showManualRec;

// Control Panel size mode: 0=Compact, 1=Normal, 2=Wide
extern int panelSizeMode;

extern HFONT hFontTitle;
extern HFONT hFontStatus;
extern HFONT hFontNormal;
extern HFONT hFontSmall;
extern HFONT hFontOverlay;
extern HFONT hFontBold;

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

// Colors — Refined Navy-Dark Theme
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
extern COLORREF colorAccent;
extern COLORREF colorPanelBg;
extern COLORREF colorPanelBorder;

extern HBRUSH hBrushBg;
extern HBRUSH hBrushSidebarBg;
extern HBRUSH hBrushOverlayMuted;
extern HBRUSH hBrushOverlayLive;
extern HBRUSH hBrushMeterBg;
extern HBRUSH hBrushChroma;
extern HBRUSH hBrushSidebarHover;
extern HBRUSH hBrushSidebarSelected;
extern HBRUSH hBrushPanelBg;

// Dragging
extern bool isDragging;
extern POINT dragStart;
extern POINT clickStart;
extern bool isMeterDragging;
extern POINT meterDragStart;

// DPI Helper
float GetWindowScale(HWND hWnd);
