#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "http_server.h"
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <string>
#include <array>
#include "resource.h"
#include "globals.h"
#include "settings.h"
#include "audio.h"
#include "tray.h"
#include "overlay.h"
#include "ui.h"
#include "recorder.h"
#include "call_recorder.h"
#include "ui_controls.h" // New custom controls

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWA_MICA_EFFECT
#define DWMWA_MICA_EFFECT 1029
#endif

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

// Main Window Procedure
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Controls
int currentTab = 0; // 0=General, 1=Hotkeys, 2=Audio, 3=Appearance
int hoverTab = -1;

// Custom Caption Buttons
RECT rcClose = {0};
RECT rcMin = {0};
bool isHoverClose = false;
bool isHoverMin = false;
bool isPressedClose = false;
bool isPressedMin = false;

// Tab Labels
constexpr std::array<const char*, 4> tabNames = { "Dashboard", "Layout", "Appearance", "Settings" };
constexpr int TAB_COUNT = 4;

// Controls - Dashboard
// (Drawn manually in WM_PAINT)

// Controls - Layout
HWND hHideOverlayCheck, hHideMeterCheck, hHideRecorderCheck;
HWND hWidthSlider, hHeightSlider;

// Controls - Appearance
HWND hNameEdit, hBoldCheck, hColorRed, hColorGreen, hColorBlue, hColorWhite, hColorMagenta;
HWND hBgMica, hBgSolid, hBgBlack;

// Controls - Settings (General)
HWND hStartupCheck, hNotifyCheck, hAutoRecordCheck;

void UpdateControlVisibility() {
    int cmdShow = SW_HIDE;
    
    // settings (Tab 3)
    if (currentTab == 3) cmdShow = SW_SHOW;
    else cmdShow = SW_HIDE;

    ShowWindow(hStartupCheck, cmdShow);
    ShowWindow(hNotifyCheck, cmdShow);
    ShowWindow(hAutoRecordCheck, cmdShow);
    ShowWindow(GetDlgItem(hMainWnd, 9998), cmdShow); // Extension status

    // Layout (Tab 1)
    if (currentTab == 1) cmdShow = SW_SHOW;
    else cmdShow = SW_HIDE;
    
    ShowWindow(hHideOverlayCheck, cmdShow);
    ShowWindow(hHideMeterCheck, cmdShow);
    ShowWindow(hHideRecorderCheck, cmdShow);
    ShowWindow(hWidthSlider, cmdShow);
    ShowWindow(hHeightSlider, cmdShow);

    // Appearance (Tab 2)
    if (currentTab == 2) cmdShow = SW_SHOW;
    else cmdShow = SW_HIDE;
    
    ShowWindow(hNameEdit, cmdShow);
    ShowWindow(hBoldCheck, cmdShow);
    ShowWindow(hColorRed, cmdShow);
    ShowWindow(hColorGreen, cmdShow);
    ShowWindow(hColorBlue, cmdShow);
    ShowWindow(hColorWhite, cmdShow);
    ShowWindow(hColorMagenta, cmdShow);
    ShowWindow(hBgMica, cmdShow);
    ShowWindow(hBgSolid, cmdShow);
    ShowWindow(hBgBlack, cmdShow);

    // Repaint to clear/draw proper background
    InvalidateRect(hMainWnd, nullptr, TRUE);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize COM
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) return 0;

    // Single Instance
    HANDLE hMutex = CreateMutex(nullptr, TRUE, "MicMuteS_SingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindow("MicMuteS_Class", "MicMute-S");
        if (hExisting) {
            ShowWindow(hExisting, SW_RESTORE);
            SetForegroundWindow(hExisting);
        }
        CloseHandle(hMutex);
        CoUninitialize();
        return 0;
    }

    InitCommonControls(); // Basic init

    InitializeAudio();
    InitCallRecorder();
    InitHttpServer(); // For Ozonetel Chrome Extension integration

    // Create brushes
    hBrushBg = CreateSolidBrush(colorBg);
    hBrushSidebarBg = CreateSolidBrush(colorSidebarBg);
    hBrushSidebarHover = CreateSolidBrush(colorSidebarHover);
    hBrushSidebarSelected = CreateSolidBrush(colorSidebarSelected);
    hBrushOverlayMuted = CreateSolidBrush(colorOverlayBgMuted);
    hBrushOverlayLive = CreateSolidBrush(colorOverlayBgLive);
    hBrushMeterBg = CreateSolidBrush(colorMeterBg);
    hBrushChroma = CreateSolidBrush(colorChroma);

    // Register main window class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // Transparency for Mica
    wc.lpszClassName = "MicMuteS_Class";
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    RegisterClassEx(&wc);

    // Register other classes
    WNDCLASSEX wcOverlay = {0};
    wcOverlay.cbSize = sizeof(WNDCLASSEX);
    wcOverlay.style = CS_HREDRAW | CS_VREDRAW;
    wcOverlay.lpfnWndProc = OverlayWndProc;
    wcOverlay.hInstance = hInstance;
    wcOverlay.hCursor = LoadCursor(nullptr, IDC_HAND);
    wcOverlay.hbrBackground = hBrushChroma;
    wcOverlay.lpszClassName = "MicMuteS_Overlay";
    RegisterClassEx(&wcOverlay);

    WNDCLASSEX wcMeter = {0};
    wcMeter.cbSize = sizeof(WNDCLASSEX);
    wcMeter.style = CS_HREDRAW | CS_VREDRAW;
    wcMeter.lpfnWndProc = MeterWndProc;
    wcMeter.hInstance = hInstance;
    wcMeter.hCursor = LoadCursor(nullptr, IDC_SIZEALL);
    wcMeter.hbrBackground = hBrushMeterBg;
    wcMeter.lpszClassName = "MicMuteS_Meter";
    RegisterClassEx(&wcMeter);

    WNDCLASSEX wcRec = {0};
    wcRec.cbSize = sizeof(WNDCLASSEX);
    wcRec.style = CS_HREDRAW | CS_VREDRAW;
    wcRec.lpfnWndProc = RecorderWndProc;
    wcRec.hInstance = hInstance;
    wcRec.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcRec.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcRec.lpszClassName = "MicMuteS_Recorder";
    RegisterClassEx(&wcRec);

    // Window size (Wider for sidebar)
    int width = 850; 
    int height = 600; 
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;

    hMainWnd = CreateWindowEx(
        0, "MicMuteS_Class", "MicMute-S",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, height, 
        nullptr, nullptr, hInstance, nullptr
    );
    
    if (!hMainWnd) return 0;

    // Extend Frame into Client Area for Mica
    MARGINS margins = {-1};
    DwmExtendFrameIntoClientArea(hMainWnd, &margins);

    // Dark Mode Titlebar (Windows 11)
    BOOL value = TRUE;
    DwmSetWindowAttribute(hMainWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
    
    // Mica Effect
    int micaValue = DWMSBT_MAINWINDOW;
    DwmSetWindowAttribute(hMainWnd, DWMWA_SYSTEMBACKDROP_TYPE, &micaValue, sizeof(micaValue));
    
    DWM_WINDOW_CORNER_PREFERENCE cornerPref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hMainWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    hIconMicOn = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MIC_ON));
    hIconMicOff = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MIC_OFF));

    // Create Fonts (Modern)
    hFontTitle = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontStatus = CreateFont(24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontNormal = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontSmall = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontOverlay = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    LoadSettings();
    LoadStats(); // Load Statistics

    if (isRunOnStartup) {
        // Robustness: Re-apply startup registry key on every launch to fix broken paths/missing keys
        ManageStartup(true);
    } else {
        // First run logic
        if (MessageBox(hMainWnd, "Enable Startup with Windows?", "MicMute-S", MB_YESNO | MB_ICONQUESTION) == IDYES) {
             ManageStartup(true);
             isRunOnStartup = true;
             SaveSettings();
        }
    }

    if (showOverlay) CreateOverlayWindow(hInstance);
    if (showMeter) CreateMeterWindow(hInstance);
    if (showRecorder) CreateRecorderWindow(hInstance);
    
    // Enable auto call recording if setting is on
    if (autoRecordCalls && g_CallRecorder) {
        g_CallRecorder->Enable();
    }

    AddTrayIcon(hMainWnd);
    UpdateUIState();

    SetTimer(hMainWnd, 1, 50, nullptr);
    SetTimer(hMainWnd, 2, 2000, nullptr);

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    DeleteObject(hBrushBg);
    DeleteObject(hBrushSidebarBg);
    DeleteObject(hBrushSidebarHover);
    DeleteObject(hBrushSidebarSelected);
    DeleteObject(hBrushOverlayMuted);
    DeleteObject(hBrushOverlayLive);
    DeleteObject(hBrushMeterBg);
    DeleteObject(hBrushChroma);
    
    DeleteObject(hFontTitle);
    DeleteObject(hFontStatus);
    DeleteObject(hFontNormal);
    DeleteObject(hFontSmall);
    DeleteObject(hFontOverlay);
    
    UninitializeAudio();
    CleanupHttpServer();
    CleanupCallRecorder();
    CleanupRecorder();
    CloseHandle(hMutex);
    
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wParam) return 0; // Remove standard frame for seamless client area
            return DefWindowProc(hWnd, msg, wParam, lParam);

        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
            
            // App Title in Sidebar
            // We'll draw it in WM_PAINT instead of a static control for better look
            
            int contentX = SIDEBAR_WIDTH + 40;
            int startY = 100;
            int gapY = 40;
            
            // -- TAB 3: SETTINGS (General) --
            hStartupCheck = CreateWindow("BUTTON", "Launch on Windows startup", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY, 300, 30, hWnd, (HMENU)ID_RUN_STARTUP, hInst, nullptr);
            SendMessage(hStartupCheck, BM_SETCHECK, isRunOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);

            hNotifyCheck = CreateWindow("BUTTON", "Show system notifications", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY, 300, 30, hWnd, (HMENU)ID_SHOW_NOTIFICATIONS, hInst, nullptr);
            SendMessage(hNotifyCheck, BM_SETCHECK, showNotifications ? BST_CHECKED : BST_UNCHECKED, 0);

            hAutoRecordCheck = CreateWindow("BUTTON", "Auto record calls (Ozonetel)", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY*2, 350, 30, hWnd, (HMENU)ID_AUTO_RECORD_CALLS, hInst, nullptr);
            SendMessage(hAutoRecordCheck, BM_SETCHECK, autoRecordCalls ? BST_CHECKED : BST_UNCHECKED, 0);
            
            // Extension connection status (shown when Auto Record is enabled)
            CreateWindow("STATIC", "", 
                WS_CHILD | SS_LEFT, 
                contentX, startY + gapY*3, 350, 20, hWnd, (HMENU)9998, hInst, nullptr);

            // -- TAB 1: LAYOUT --
            int layoutY = 100;
            hHideOverlayCheck = CreateWindow("BUTTON", "Show Floating Mute Button", 
                WS_CHILD | BS_AUTOCHECKBOX, 
                contentX, layoutY, 300, 30, hWnd, (HMENU)ID_SHOW_OVERLAY, hInst, nullptr);
            SendMessage(hHideOverlayCheck, BM_SETCHECK, showOverlay ? BST_CHECKED : BST_UNCHECKED, 0);

            hHideMeterCheck = CreateWindow("BUTTON", "Show Voice Level Meter", 
                WS_CHILD | BS_AUTOCHECKBOX, 
                contentX, layoutY + gapY, 300, 30, hWnd, (HMENU)ID_SHOW_METER, hInst, nullptr);
            SendMessage(hHideMeterCheck, BM_SETCHECK, showMeter ? BST_CHECKED : BST_UNCHECKED, 0);

            hHideRecorderCheck = CreateWindow("BUTTON", "Show Call Recorder", 
                WS_CHILD | BS_AUTOCHECKBOX, 
                contentX, layoutY + gapY*2, 300, 30, hWnd, (HMENU)ID_SHOW_RECORDER, hInst, nullptr);
            SendMessage(hHideRecorderCheck, BM_SETCHECK, showRecorder ? BST_CHECKED : BST_UNCHECKED, 0);

            // Sliders for Window Size
            hWidthSlider = CreateWindow(TRACKBAR_CLASS, "Width", 
                WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 
                contentX, 300, 400, 30, hWnd, (HMENU)1001, hInst, nullptr);
            SendMessage(hWidthSlider, TBM_SETRANGE, TRUE, MAKELONG(600, 1200));
            SendMessage(hWidthSlider, TBM_SETPOS, TRUE, 850); // Current default

            hHeightSlider = CreateWindow(TRACKBAR_CLASS, "Height", 
                WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 
                contentX, 340, 400, 30, hWnd, (HMENU)1002, hInst, nullptr);
            SendMessage(hHeightSlider, TBM_SETRANGE, TRUE, MAKELONG(400, 800));
            SendMessage(hHeightSlider, TBM_SETPOS, TRUE, 600); // Current default

            // -- TAB 2: APPEARANCE --
            hNameEdit = CreateWindow("EDIT", customTitle.c_str(), 
                WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 
                contentX, 130, 300, 30, hWnd, (HMENU)2001, hInst, nullptr);

            hBoldCheck = CreateWindow("BUTTON", "Bold Title", 
                WS_CHILD | BS_AUTOCHECKBOX, 
                contentX + 320, 130, 100, 30, hWnd, (HMENU)2002, hInst, nullptr);
            SendMessage(hBoldCheck, BM_SETCHECK, isTitleBold ? BST_CHECKED : BST_UNCHECKED, 0);

            // Color Buttons
            int colY = 210;
            hColorRed = CreateWindow("BUTTON", "Red", WS_CHILD | BS_PUSHBUTTON, contentX, colY, 60, 30, hWnd, (HMENU)2010, hInst, nullptr);
            hColorGreen = CreateWindow("BUTTON", "Green", WS_CHILD | BS_PUSHBUTTON, contentX + 70, colY, 60, 30, hWnd, (HMENU)2011, hInst, nullptr);
            hColorBlue = CreateWindow("BUTTON", "Blue", WS_CHILD | BS_PUSHBUTTON, contentX + 140, colY, 60, 30, hWnd, (HMENU)2012, hInst, nullptr);
            hColorWhite = CreateWindow("BUTTON", "White", WS_CHILD | BS_PUSHBUTTON, contentX + 210, colY, 60, 30, hWnd, (HMENU)2013, hInst, nullptr);
            hColorMagenta = CreateWindow("BUTTON", "Pink", WS_CHILD | BS_PUSHBUTTON, contentX + 280, colY, 60, 30, hWnd, (HMENU)2014, hInst, nullptr);

            // Background Style
            int bgY = 290;
            hBgMica = CreateWindow("BUTTON", "Mica Effect (Default)", WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP, contentX, bgY, 200, 30, hWnd, (HMENU)2020, hInst, nullptr);
            hBgSolid = CreateWindow("BUTTON", "Solid Black", WS_CHILD | BS_AUTORADIOBUTTON, contentX + 200, bgY, 200, 30, hWnd, (HMENU)2021, hInst, nullptr);
            
            if (backgroundStyle == BackgroundStyle::Mica) SendMessage(hBgMica, BM_SETCHECK, BST_CHECKED, 0);
            else SendMessage(hBgSolid, BM_SETCHECK, BST_CHECKED, 0);

            // Footer - Use dynamic positioning
            CreateWindow("STATIC", "by Suvojeet Sengupta", 
                 WS_VISIBLE | WS_CHILD | SS_RIGHT, 0, 0, 0, 0, hWnd, (HMENU)9999, hInst, nullptr);

            UpdateControlVisibility();
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);

            // Don't fill background to let Mica show through
            // Draw Sidebar Background (Make it semi-transparent or just draw directly)
            // If sidebar needs to be distinct, we can draw it with alpha or solid color.
            // For now, let's keep the design but relying on Mica for the main background.
            
            // Draw Sidebar (we might want this slightly opaque or just let Mica handle it)
            // DrawSidebar(hdc, rcClient, currentTab); 
            // ^ Replacing generic DrawSidebar with direct drawing to control transparency if needed
            
            RECT rcSidebar = {0, 0, SIDEBAR_WIDTH, rcClient.bottom};
            FillRect(hdc, &rcSidebar, hBrushSidebarBg); // Sidebar stays solid/dark for contrast

            // Draw Sidebar Title
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, colorText);
            SelectObject(hdc, hFontTitle);
            RECT rcTitle = {0, 30, SIDEBAR_WIDTH, 80};
            DrawText(hdc, "MicMute", -1, &rcTitle, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            // Draw Dashboard Content
            if (currentTab == 0) {
                 // 1. Custom Title
                 HFONT hCustomFont = CreateFont(48, 0, 0, 0, isTitleBold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
                 
                 SelectObject(hdc, hCustomFont);
                 SetTextColor(hdc, customTitleColor);
                 RECT rcCustomTitle = {SIDEBAR_WIDTH, 60, rcClient.right, 140};
                 DrawText(hdc, customTitle.c_str(), -1, &rcCustomTitle, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                 DeleteObject(hCustomFont);

                 // 2. Mute Status (Big)
                 bool isMuted = IsDefaultMicMuted();
                 RECT rcStatusBig = {SIDEBAR_WIDTH, 160, rcClient.right, 240};
                 SetTextColor(hdc, isMuted ? colorMuted : colorLive);
                 SelectObject(hdc, hFontTitle); // Re-use title font
                 DrawText(hdc, isMuted ? "MICROPHONE MUTED" : "MICROPHONE LIVE", -1, &rcStatusBig, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

                 // 3. Statistics Box
                 RECT rcStats = {SIDEBAR_WIDTH + 50, 280, rcClient.right - 50, 450};
                 HBRUSH hStatBg = CreateSolidBrush(RGB(40, 40, 40));
                 FillRect(hdc, &rcStats, hStatBg);
                 DeleteObject(hStatBg);
                 
                 SetTextColor(hdc, colorText);
                 SelectObject(hdc, hFontNormal);
                 
                 char buf[256];
                 RECT rcLine1 = rcStats; rcLine1.top += 20; rcLine1.left += 20;
                 sprintf_s(buf, "Calls Recorded Today: %d", appStats.callsToday);
                 DrawText(hdc, buf, -1, &rcLine1, DT_SINGLELINE | DT_LEFT | DT_TOP);
                 
                 RECT rcLine2 = rcLine1; rcLine2.top += 30;
                 sprintf_s(buf, "Total Calls Recorded: %d", appStats.callsTotal);
                 DrawText(hdc, buf, -1, &rcLine2, DT_SINGLELINE | DT_LEFT | DT_TOP);

                 RECT rcLine3 = rcLine2; rcLine3.top += 30;
                 sprintf_s(buf, "Mutes Toggled Today: %d", appStats.mutesToday);
                 DrawText(hdc, buf, -1, &rcLine3, DT_SINGLELINE | DT_LEFT | DT_TOP);
                 
                 // Show previous call stats? 
                 // Previous day stats not stored yet, but we have "Total".
            } 
            else if (currentTab == 1) {
                // Layout Labels
                SetTextColor(hdc, colorText);
                SelectObject(hdc, hFontTitle);
                RECT rcHeader = {SIDEBAR_WIDTH + 40, 40, rcClient.right, 80};
                DrawText(hdc, "Layout & Visibility", -1, &rcHeader, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

                SelectObject(hdc, hFontNormal);
                RECT rcSizeLabel = {SIDEBAR_WIDTH + 40, 260, rcClient.right, 290};
                DrawText(hdc, "Window Size:", -1, &rcSizeLabel, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            }
            else if (currentTab == 2) {
                // Appearance Labels
                SetTextColor(hdc, colorText);
                SelectObject(hdc, hFontTitle);
                RECT rcHeader = {SIDEBAR_WIDTH + 40, 40, rcClient.right, 80};
                DrawText(hdc, "Appearance", -1, &rcHeader, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
                
                SelectObject(hdc, hFontNormal);
                RECT rcNameLabel = {SIDEBAR_WIDTH + 40, 100, rcClient.right, 130};
                DrawText(hdc, "Custom Name:", -1, &rcNameLabel, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
                
                RECT rcColorLabel = {SIDEBAR_WIDTH + 40, 180, rcClient.right, 210};
                DrawText(hdc, "Text Color:", -1, &rcColorLabel, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

                RECT rcBgLabel = {SIDEBAR_WIDTH + 40, 260, rcClient.right, 290};
                DrawText(hdc, "Background Style:", -1, &rcBgLabel, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            }
            else if (currentTab == 3) {
                 SetTextColor(hdc, colorText);
                 SelectObject(hdc, hFontTitle);
                 RECT rcHeader = {SIDEBAR_WIDTH + 40, 40, rcClient.right, 80};
                 DrawText(hdc, "General Settings", -1, &rcHeader, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            }
            for (int i = 0; i < TAB_COUNT; i++) {
                // Determine State
                bool isSelected = (i == currentTab);
                bool isHovered = (i == hoverTab);
                
                RECT rcItem = {0, 100 + (i * SIDEBAR_ITEM_HEIGHT), SIDEBAR_WIDTH, 100 + ((i + 1) * SIDEBAR_ITEM_HEIGHT)};
                
                // Selection / Hover Background
                if (isSelected) {
                    // Modern left pill indicator
                    RECT rcIndicator = {2, rcItem.top + 10, 6, rcItem.bottom - 10};
                    HBRUSH hAccent = CreateSolidBrush(colorSidebarSelected);
                    FillRect(hdc, &rcIndicator, hAccent);
                    DeleteObject(hAccent);
                    
                    // Subtle background for selected item
                    HBRUSH hSelectedBg = CreateSolidBrush(RGB(45, 45, 45));
                    FillRect(hdc, &rcItem, hSelectedBg);
                    DeleteObject(hSelectedBg);
                } else if (isHovered) {
                    FillRect(hdc, &rcItem, hBrushSidebarHover);
                }
                
                // Icon Placement (Placeholder for actual icons)
                // DrawIconEx(hdc, 20, rcItem.top + 12, ...);

                // Text
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, isSelected ? colorText : (isHovered ? colorText : colorTextDim));
                SelectObject(hdc, isSelected ? hFontNormal : hFontSmall);
                
                RECT rcText = rcItem;
                rcText.left += 50; // Gap for icon
                DrawText(hdc, tabNames[i], -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            }

            // Draw Status Text (Directly on Main Window)
            /* REMOVED: Status is now part of Dashboard
            {
                bool isMutedStatus = IsDefaultMicMuted();
                // Set color based on status
                SetTextColor(hdc, isMutedStatus ? colorMuted : colorLive);
                SelectObject(hdc, hFontStatus);
                
                // Draw in the top-right area
                // Avoid caption buttons (right side ~100px)
                RECT rcStatus = {SIDEBAR_WIDTH, 30, rcClient.right - 100, 70}; 
                const char* statusText = isMutedStatus ? "MICROPHONE MUTED" : "MICROPHONE LIVE";
                DrawText(hdc, statusText, -1, &rcStatus, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            }
            */

            // Content Headings for placeholder tabs
            if (currentTab != 0) {
                SetTextColor(hdc, colorTextDim);
                SelectObject(hdc, hFontTitle);
                RECT rcContent = {SIDEBAR_WIDTH, 150, rcClient.right, 250};
                DrawText(hdc, "Coming Soon", -1, &rcContent, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            }

            // Draw Custom Caption Buttons
            int btnW = 45;
            int btnH = 30;
            rcClose = {rcClient.right - btnW, 0, rcClient.right, btnH};
            rcMin = {rcClose.left - btnW, 0, rcClose.left, btnH};

            // Close Button - proper state handling
            if (isPressedClose) {
                HBRUSH hPressed = CreateSolidBrush(RGB(196, 43, 28)); // Darker red on press
                FillRect(hdc, &rcClose, hPressed);
                DeleteObject(hPressed);
                SetTextColor(hdc, RGB(255, 255, 255));
            } else if (isHoverClose) {
                HBRUSH hRed = CreateSolidBrush(RGB(232, 17, 35));
                FillRect(hdc, &rcClose, hRed);
                DeleteObject(hRed);
                SetTextColor(hdc, RGB(255, 255, 255));
            } else {
                SetTextColor(hdc, colorText);
            }
            SelectObject(hdc, hFontNormal);
            DrawText(hdc, "X", 1, &rcClose, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);

            // Minimize Button - proper state handling
            if (isPressedMin) {
                HBRUSH hPressed = CreateSolidBrush(RGB(70, 70, 70)); // Darker on press
                FillRect(hdc, &rcMin, hPressed);
                DeleteObject(hPressed);
            } else if (isHoverMin) {
                HBRUSH hHover = CreateSolidBrush(RGB(50, 50, 50));
                FillRect(hdc, &rcMin, hHover);
                DeleteObject(hHover);
            }
            SetTextColor(hdc, colorText);
            DrawText(hdc, "_", 1, &rcMin, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlType == ODT_BUTTON) {
                DrawToggle(lpDrawItem);
                return TRUE;
            }
            break;
        }

        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            int newHover = -1;
            
            if (x < SIDEBAR_WIDTH) {
                // Check which item
                for (int i = 0; i < TAB_COUNT; i++) {
                    int itemTop = 100 + (i * SIDEBAR_ITEM_HEIGHT);
                    int itemBottom = itemTop + SIDEBAR_ITEM_HEIGHT;
                    if (y >= itemTop && y < itemBottom) {
                        newHover = i;
                        break;
                    }
                }
            }

            if (newHover != hoverTab) {
                hoverTab = newHover;
                RECT rcSidebar = {0, 0, SIDEBAR_WIDTH, 500};
                InvalidateRect(hWnd, &rcSidebar, FALSE);
                
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                TrackMouseEvent(&tme);
            }

            // Caption Button Hover
            bool paint = false;
            POINT pt = {x, y};
            if (PtInRect(&rcClose, pt)) {
                if (!isHoverClose) { isHoverClose = true; paint = true; }
            } else {
                if (isHoverClose) { isHoverClose = false; paint = true; }
            }

            if (PtInRect(&rcMin, pt)) {
                if (!isHoverMin) { isHoverMin = true; paint = true; }
            } else {
                if (isHoverMin) { isHoverMin = false; paint = true; }
            }
            
            if (paint) {
                RECT rcBtns = {rcMin.left, 0, rcClose.right, rcClose.bottom};
                InvalidateRect(hWnd, &rcBtns, FALSE);
            }

            break;
        }

        case WM_MOUSELEAVE: {
            hoverTab = -1;
            RECT rcSidebar = {0, 0, SIDEBAR_WIDTH, 500};
            InvalidateRect(hWnd, &rcSidebar, FALSE);
            break;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            POINT pt = {x, y};

            // Caption Buttons - track pressed state
            if (PtInRect(&rcClose, pt)) {
                isPressedClose = true;
                SetCapture(hWnd);
                InvalidateRect(hWnd, &rcClose, FALSE);
                return 0;
            }
            if (PtInRect(&rcMin, pt)) {
                isPressedMin = true;
                SetCapture(hWnd);
                InvalidateRect(hWnd, &rcMin, FALSE);
                return 0;
            }
            
            if (x < SIDEBAR_WIDTH) {
                for (int i = 0; i < TAB_COUNT; i++) {
                    int itemTop = 100 + (i * SIDEBAR_ITEM_HEIGHT);
                    int itemBottom = itemTop + SIDEBAR_ITEM_HEIGHT;
                    if (y >= itemTop && y < itemBottom) {
                        currentTab = i;
                        UpdateControlVisibility();
                        InvalidateRect(hWnd, nullptr, TRUE); 
                        break;
                    }
                }
            }
            break;
        }

        case WM_LBUTTONUP: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            POINT pt = {x, y};
            
            ReleaseCapture();
            
            // Handle caption button clicks on release
            if (isPressedClose && PtInRect(&rcClose, pt)) {
                isPressedClose = false;
                SendMessage(hWnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (isPressedMin && PtInRect(&rcMin, pt)) {
                isPressedMin = false;
                ShowWindow(hWnd, SW_MINIMIZE);
                return 0;
            }
            
            isPressedClose = false;
            isPressedMin = false;
            RECT rcBtns = {rcMin.left, 0, rcClose.right, rcClose.bottom};
            InvalidateRect(hWnd, &rcBtns, FALSE);
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, colorBg);
            SetTextColor(hdc, colorTextDim);
            SelectObject(hdc, hFontSmall);
            return (LRESULT)hBrushBg;
        }

        case WM_HSCROLL: {
            HWND hCtl = (HWND)lParam;
            if (hCtl == hWidthSlider || hCtl == hHeightSlider) {
                int w = (int)SendMessage(hWidthSlider, TBM_GETPOS, 0, 0);
                int h = (int)SendMessage(hHeightSlider, TBM_GETPOS, 0, 0);
                
                SetWindowPos(hWnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            
            // Handle Toggles
            // Note: For OwnerDraw buttons, we still get BN_CLICKED
            if (wmId == ID_RUN_STARTUP) {
                isRunOnStartup = !isRunOnStartup;
                ManageStartup(isRunOnStartup);
                SendMessage(hStartupCheck, BM_SETCHECK, isRunOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);
            }
            else if (wmId == ID_SHOW_OVERLAY) {
                showOverlay = !showOverlay;
                SaveSettings();
                if (showOverlay) {
                    if (!hOverlayWnd) CreateOverlayWindow(GetModuleHandle(nullptr));
                    else ShowWindow(hOverlayWnd, SW_SHOW);
                    UpdateOverlay();
                } else if (hOverlayWnd) ShowWindow(hOverlayWnd, SW_HIDE);
                SendMessage(hHideOverlayCheck, BM_SETCHECK, showOverlay ? BST_CHECKED : BST_UNCHECKED, 0);
            }
            else if (wmId == ID_SHOW_METER) {
                showMeter = !showMeter;
                SaveSettings();
                if (showMeter) {
                    if (!hMeterWnd) CreateMeterWindow(GetModuleHandle(nullptr));
                    else ShowWindow(hMeterWnd, SW_SHOW);
                } else if (hMeterWnd) ShowWindow(hMeterWnd, SW_HIDE);
                SendMessage(hHideMeterCheck, BM_SETCHECK, showMeter ? BST_CHECKED : BST_UNCHECKED, 0);
            }
            else if (wmId == ID_SHOW_RECORDER) {
                showRecorder = !showRecorder;
                SaveSettings();
                if (showRecorder) {
                    if (!hRecorderWnd) CreateRecorderWindow(GetModuleHandle(nullptr));
                    else ShowWindow(hRecorderWnd, SW_SHOW);
                } else if (hRecorderWnd) ShowWindow(hRecorderWnd, SW_HIDE);
                SendMessage(hHideRecorderCheck, BM_SETCHECK, showRecorder ? BST_CHECKED : BST_UNCHECKED, 0);
            }
            else if (wmId == ID_SHOW_NOTIFICATIONS) {
                showNotifications = !showNotifications;
                SaveSettings();
                SendMessage((HWND)lParam, BM_SETCHECK, showNotifications ? BST_CHECKED : BST_UNCHECKED, 0);
            }
            else if (wmId == ID_AUTO_RECORD_CALLS) {
                autoRecordCalls = !autoRecordCalls;
                SaveSettings();
                if (g_CallRecorder) {
                     if (autoRecordCalls) g_CallRecorder->Enable();
                     else g_CallRecorder->Disable();
                }
                SendMessage(hAutoRecordCheck, BM_SETCHECK, autoRecordCalls ? BST_CHECKED : BST_UNCHECKED, 0);
            }
            
            // Appearance Handlers
            else if (wmId == 2001 && HIWORD(wParam) == EN_CHANGE) {
                 char buf[256];
                 GetWindowText(hNameEdit, buf, 256);
                 customTitle = buf;
                 InvalidateRect(hMainWnd, nullptr, TRUE);
                 SaveStats(); // Save on change? Or wait for exit? Wait for exit/save interval to avoid lag.
            }
            else if (wmId == 2002) {
                 isTitleBold = !isTitleBold;
                 SendMessage(hBoldCheck, BM_SETCHECK, isTitleBold ? BST_CHECKED : BST_UNCHECKED, 0);
                 InvalidateRect(hMainWnd, nullptr, TRUE);
            }
            else if (wmId >= 2010 && wmId <= 2014) {
                 if (wmId == 2010) customTitleColor = RGB(255, 69, 58); // Red
                 else if (wmId == 2011) customTitleColor = RGB(50, 215, 75); // Green
                 else if (wmId == 2012) customTitleColor = RGB(0, 120, 215); // Blue
                 else if (wmId == 2013) customTitleColor = RGB(240, 240, 240); // White
                 else if (wmId == 2014) customTitleColor = RGB(255, 0, 255); // Pink
                 
                 InvalidateRect(hMainWnd, nullptr, TRUE);
            }
            else if (wmId == 2020) {
                 backgroundStyle = BackgroundStyle::Mica;
                 // Set Mica
                 BOOL UseImmersiveDarkMode = TRUE;
                 DwmSetWindowAttribute(hMainWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &UseImmersiveDarkMode, sizeof(BOOL));
                 int micaValue = DWMSBT_MAINWINDOW;
                 DwmSetWindowAttribute(hMainWnd, DWMWA_SYSTEMBACKDROP_TYPE, &micaValue, sizeof(micaValue));
                 // Set brush to black (transparent for Mica)
                 SetClassLongPtr(hMainWnd, GCLP_HBRBACKGROUND, (LONG_PTR)GetStockObject(BLACK_BRUSH));
                 InvalidateRect(hMainWnd, nullptr, TRUE);
            }
            else if (wmId == 2021) {
                 backgroundStyle = BackgroundStyle::Solid;
                 // Disable Mica (Set to None)
                 int micaValue = DWMSBT_NONE;
                 DwmSetWindowAttribute(hMainWnd, DWMWA_SYSTEMBACKDROP_TYPE, &micaValue, sizeof(micaValue));
                 // Set solid background brush
                 SetClassLongPtr(hMainWnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrushBg);
                 InvalidateRect(hMainWnd, nullptr, TRUE);
            }

            else if (wmId == ID_TRAY_OPEN) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            }
            else if (wmId == ID_TRAY_EXIT) {
                if (hOverlayWnd) DestroyWindow(hOverlayWnd);
                if (hMeterWnd) DestroyWindow(hMeterWnd);
                if (hRecorderWnd) CleanupRecorder(), DestroyWindow(hRecorderWnd);
                DestroyWindow(hWnd);
            }
            break;
        }

        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONUP) {
                DWORD t = GetTickCount();
                if (t - lastToggleTime < 500) break;
                lastToggleTime = t;
                skipTimerCycles = 2;
                ToggleMute();
            } else if (lParam == WM_RBUTTONUP) {
                POINT pt; GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                AppendMenu(hMenu, MF_STRING, ID_TRAY_OPEN, "Open Settings");
                AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, "Exit");
                SetForegroundWindow(hWnd);
                TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, nullptr);
                DestroyMenu(hMenu);
            }
            break;
        }

        case WM_SIZE: {
            // Reposition footer dynamically
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            HWND hFooter = GetDlgItem(hWnd, 9999);
            if (hFooter) {
                MoveWindow(hFooter, rcClient.right - 160, rcClient.bottom - 40, 140, 20, TRUE);
            }
            InvalidateRect(hWnd, nullptr, TRUE);
            break;
        }

        case WM_TIMER:
            if (wParam == 1) {
                UpdateMeter();
                // Poll call auto-recorder (date tracking only, no VAD)
                if (g_CallRecorder && g_CallRecorder->IsEnabled()) {
                    g_CallRecorder->Poll();
                    // Trigger repaint for animation when auto-recording is active
                    if (hRecorderWnd && showRecorder) {
                        InvalidateRect(hRecorderWnd, nullptr, FALSE);
                    }
                }
                
                // Update extension connection status display
                if (autoRecordCalls) {
                    HWND hStatus = GetDlgItem(hMainWnd, 9998);
                    if (hStatus) {
                        ShowWindow(hStatus, SW_SHOW);
                        if (IsExtensionConnected()) {
                            SetWindowText(hStatus, "✓ Extension Connected");
                        } else {
                            SetWindowText(hStatus, "⏳ Waiting for Extension...");
                        }
                    }
                } else {
                    HWND hStatus = GetDlgItem(hMainWnd, 9998);
                    if (hStatus) {
                        ShowWindow(hStatus, SW_HIDE);
                    }
                }
            } else if (wParam == 2) {
                if (skipTimerCycles > 0) { skipTimerCycles--; break; }
                UpdateUIState();
            }
            break;

        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);
            // Handle resizing edges manually if NCCALCSIZE removed them?
            // Actually DefWindowProc still handles resizing if we respect borders... 
            // BUT with NCCALCSIZE=0, the client area covers borders.
            // We need to implement manual border hit testing for resizing.
            
            POINT pt; GetCursorPos(&pt);
            RECT rc; GetWindowRect(hWnd, &rc);
            
            int border = 6; // Resize border thickness
            if (pt.x < rc.left + border && pt.y < rc.top + border) return HTTOPLEFT;
            if (pt.x > rc.right - border && pt.y < rc.top + border) return HTTOPRIGHT;
            if (pt.x < rc.left + border && pt.y > rc.bottom - border) return HTBOTTOMLEFT;
            if (pt.x > rc.right - border && pt.y > rc.bottom - border) return HTBOTTOMRIGHT;
            if (pt.x < rc.left + border) return HTLEFT;
            if (pt.x > rc.right - border) return HTRIGHT;
            if (pt.y < rc.top + border) return HTTOP;
            if (pt.y > rc.bottom - border) return HTBOTTOM;
            
            POINT clientPt = pt;
            ScreenToClient(hWnd, &clientPt);

            // Caption Buttons (Pass through to Client for Hover/Click)
            if (PtInRect(&rcClose, clientPt) || PtInRect(&rcMin, clientPt)) {
                return HTCLIENT;
            }

            // Draggable Header Area (Top 60px)
            if (clientPt.y < 60) {
                return HTCAPTION;
            }

            // Draggable Sidebar (Empty Areas)
            // Items are from 100 to 100 + (4*50) = 300
            if (clientPt.x < SIDEBAR_WIDTH) {
                int itemStart = 100;
                int itemEnd = 100 + (TAB_COUNT * SIDEBAR_ITEM_HEIGHT);
                if (clientPt.y < itemStart || clientPt.y > itemEnd) {
                    return HTCAPTION;
                }
            }
            
            return HTCLIENT;
        }

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                ShowWindow(hWnd, SW_HIDE);
                return 0;
            }
            return DefWindowProc(hWnd, msg, wParam, lParam);

        case WM_QUERYENDSESSION:
            return TRUE; // We agree to shut down

        case WM_ENDSESSION:
            if (wParam == TRUE) {
                // Windows is shutting down, save everything
                SaveSettings();
                SaveStats(); // Save stats
                // Individual window positions should be saved too
                if (hRecorderWnd) SaveRecorderPosition(); // Add to recorder.h/cpp if needed
                // Helper to save all - implemented in settings.cpp now calls SaveOverlay/Meter
                // But we need to make sure recorder pos is saved.
            }
            return 0;

        case WM_DESTROY:
            // Save state before exit
            SaveSettings();
            if (hRecorderWnd) SaveRecorderPosition();
            
            RemoveTrayIcon();
            PostQuitMessage(0);
            break;

        case WM_APP_MUTE_CHANGED:
            UpdateUIState();
            return 0;

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}


