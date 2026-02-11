#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "network/http_server.h"
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <string>
#include <array>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "core/resource.h"

// Temporary ID manually added if not in resource.h yet
#ifndef ID_CHECK_UPDATE
#define ID_CHECK_UPDATE 4001
#endif

#include "core/globals.h"
#include "core/settings.h"
#include "audio/audio.h"
#include "ui/tray.h"
#include "ui/overlay.h"
#include "ui/ui.h"
#include "audio/recorder.h"
#include "audio/call_recorder.h"
#include "ui/ui_controls.h"
#include "ui/control_panel.h"
#include "network/updater.h"
#include "ui/password_dialog.h"

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

// Current tab: 0=General, 1=Hide/Unhide, 2=Shape & Size, 3=Appearance
int currentTab = 0;
int hoverTab = -1;

// Custom Caption Buttons
RECT rcClose = {0};
RECT rcMin = {0};
bool isHoverClose = false;
bool isHoverMin = false;
bool isPressedClose = false;
bool isPressedMin = false;

// General Tab Controls
HWND hStartupCheck, hOverlayCheck, hMeterCheck, hRecorderCheck, hNotifyCheck, hAutoRecordCheck;
HWND hBeepCheck;

// Hide/Unhide Tab Controls
HWND hHideMuteBtn, hHideVoiceMeter, hHideRecStatus, hHideCallStats, hHideManualRec;

// Shape & Size Tab Controls
HWND hSizeCompact, hSizeNormal, hSizeWide;

// Tab Labels
constexpr std::array<const char*, 4> tabNames = { "General", "Hide/Unhide", "Shape & Size", "Appearance" };
constexpr int TAB_COUNT = 4;

void UpdateControlVisibility() {
    bool isGeneral = (currentTab == 0);
    bool isHide = (currentTab == 1);
    bool isSize = (currentTab == 2);
    
    int showGeneral = isGeneral ? SW_SHOW : SW_HIDE;
    ShowWindow(hStartupCheck, showGeneral);
    ShowWindow(hOverlayCheck, showGeneral);
    ShowWindow(hMeterCheck, showGeneral);
    ShowWindow(hRecorderCheck, showGeneral);
    ShowWindow(hNotifyCheck, showGeneral);
    ShowWindow(hAutoRecordCheck, showGeneral);
    if (hBeepCheck) ShowWindow(hBeepCheck, showGeneral);
    
    // Hide/Unhide tab
    int showHide = isHide ? SW_SHOW : SW_HIDE;
    if (hHideMuteBtn) ShowWindow(hHideMuteBtn, showHide);
    if (hHideVoiceMeter) ShowWindow(hHideVoiceMeter, showHide);
    if (hHideRecStatus) ShowWindow(hHideRecStatus, showHide);
    if (hHideCallStats) ShowWindow(hHideCallStats, showHide);
    if (hHideManualRec) ShowWindow(hHideManualRec, showHide);
    
    // Shape & Size tab
    int showSize = isSize ? SW_SHOW : SW_HIDE;
    if (hSizeCompact) ShowWindow(hSizeCompact, showSize);
    if (hSizeNormal) ShowWindow(hSizeNormal, showSize);
    if (hSizeWide) ShowWindow(hSizeWide, showSize);
    
    InvalidateRect(hMainWnd, nullptr, TRUE);
}

// Helper: Resize control panel based on current mode
void ResizeControlPanel() {
    if (!hControlPanel) return;
    float scale = GetWindowScale(hControlPanel);
    int w, h;
    GetPanelDimensions(panelSizeMode, scale, &w, &h);
    SetWindowPos(hControlPanel, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) return 0;

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

    InitCommonControls();

    InitializeAudio();
    InitCallRecorder();
    InitHttpServer();
    
    // Auto-Updater Init
    AutoUpdater::Init(hMainWnd);
    // Silent startup check
    if (isRunOnStartup) {
        AutoUpdater::CheckForUpdateAsync(true);
    } else {
        // Also check silently on normal launch
        AutoUpdater::CheckForUpdateAsync(true);
    }

    // Create brushes
    hBrushBg = CreateSolidBrush(colorBg);
    hBrushSidebarBg = CreateSolidBrush(colorSidebarBg);
    hBrushSidebarHover = CreateSolidBrush(colorSidebarHover);
    hBrushSidebarSelected = CreateSolidBrush(colorSidebarSelected);
    hBrushOverlayMuted = CreateSolidBrush(colorOverlayBgMuted);
    hBrushOverlayLive = CreateSolidBrush(colorOverlayBgLive);
    hBrushMeterBg = CreateSolidBrush(colorMeterBg);
    hBrushChroma = CreateSolidBrush(colorChroma);
    hBrushPanelBg = CreateSolidBrush(colorPanelBg);

    // Register main window class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "MicMuteS_Class";
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    RegisterClassEx(&wc);

    // Register overlay class
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

    // Window size
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

    // Mica / DWM setup
    MARGINS margins = {-1};
    DwmExtendFrameIntoClientArea(hMainWnd, &margins);

    BOOL value = TRUE;
    DwmSetWindowAttribute(hMainWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
    
    int micaValue = DWMSBT_MAINWINDOW;
    DwmSetWindowAttribute(hMainWnd, DWMWA_SYSTEMBACKDROP_TYPE, &micaValue, sizeof(micaValue));
    
    DWM_WINDOW_CORNER_PREFERENCE cornerPref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hMainWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    hIconMicOn = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MIC_ON));
    hIconMicOff = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MIC_OFF));

    // Create Fonts
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
    hFontBold = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    LoadSettings();

    if (isRunOnStartup) {
        ManageStartup(true);
    } else {
        if (MessageBox(hMainWnd, "Enable Startup with Windows?", "MicMute-S", MB_YESNO | MB_ICONQUESTION) == IDYES) {
             ManageStartup(true);
             isRunOnStartup = true;
             SaveSettings();
        }
    }

    // Create the unified control panel (replaces separate overlay/meter/recorder)
    CreateControlPanel(hInstance);
    
    // Legacy: still support individual windows if user prefers
    // if (showOverlay) CreateOverlayWindow(hInstance); 
    // if (showMeter) CreateMeterWindow(hInstance);
    // if (showRecorder) CreateRecorderWindow(hInstance);
    
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
    DeleteObject(hBrushPanelBg);
    
    DeleteObject(hFontTitle);
    DeleteObject(hFontStatus);
    DeleteObject(hFontNormal);
    DeleteObject(hFontSmall);
    DeleteObject(hFontOverlay);
    DeleteObject(hFontBold);
    
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
            if (wParam) return 0;
            return DefWindowProc(hWnd, msg, wParam, lParam);

        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
            
            int contentX = SIDEBAR_WIDTH + 40;
            int startY = 100;
            int gapY = 50;

            // === General Tab Toggles ===
            hStartupCheck = CreateWindow("BUTTON", "Launch on Windows startup", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY, 300, 30, hWnd, (HMENU)ID_RUN_STARTUP, hInst, nullptr);
            SendMessage(hStartupCheck, BM_SETCHECK, isRunOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);

            hOverlayCheck = CreateWindow("BUTTON", "Show floating mute button", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY, 300, 30, hWnd, (HMENU)ID_SHOW_OVERLAY, hInst, nullptr);
            SendMessage(hOverlayCheck, BM_SETCHECK, showOverlay ? BST_CHECKED : BST_UNCHECKED, 0);

            hMeterCheck = CreateWindow("BUTTON", "Show voice level meter", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY*2, 300, 30, hWnd, (HMENU)ID_SHOW_METER, hInst, nullptr);
            SendMessage(hMeterCheck, BM_SETCHECK, showMeter ? BST_CHECKED : BST_UNCHECKED, 0);

            hRecorderCheck = CreateWindow("BUTTON", "Show call recorder", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY*3, 300, 30, hWnd, (HMENU)ID_SHOW_RECORDER, hInst, nullptr);
            SendMessage(hRecorderCheck, BM_SETCHECK, showRecorder ? BST_CHECKED : BST_UNCHECKED, 0);

            hNotifyCheck = CreateWindow("BUTTON", "Show system notifications", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY*4, 300, 30, hWnd, (HMENU)ID_SHOW_NOTIFICATIONS, hInst, nullptr);
            SendMessage(hNotifyCheck, BM_SETCHECK, showNotifications ? BST_CHECKED : BST_UNCHECKED, 0);

            hAutoRecordCheck = CreateWindow("BUTTON", "Auto record calls (Ozonetel)", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY*5, 350, 30, hWnd, (HMENU)ID_AUTO_RECORD_CALLS, hInst, nullptr);
            SendMessage(hAutoRecordCheck, BM_SETCHECK, autoRecordCalls ? BST_CHECKED : BST_UNCHECKED, 0);

            hBeepCheck = CreateWindow("BUTTON", "Beep on call detected", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY*6, 350, 30, hWnd, (HMENU)ID_BEEP_ON_CALL, hInst, nullptr);
            SendMessage(hBeepCheck, BM_SETCHECK, beepOnCall ? BST_CHECKED : BST_UNCHECKED, 0);
            
            // Extension status
            CreateWindow("STATIC", "", 
                WS_CHILD | SS_LEFT, 
                contentX, startY + gapY*7, 350, 20, hWnd, (HMENU)9998, hInst, nullptr);

            // Check for Updates Button
            CreateWindow("BUTTON", "Check for Updates", 
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 
                contentX, startY + gapY*8, 160, 30, hWnd, (HMENU)ID_CHECK_UPDATE, hInst, nullptr);

            // === Hide/Unhide Tab Toggles ===
            hHideMuteBtn = CreateWindow("BUTTON", "Mute/Unmute Button",
                WS_CHILD | BS_OWNERDRAW,
                contentX, startY, 300, 30, hWnd, (HMENU)ID_HIDE_MUTE_BTN, hInst, nullptr);
            SendMessage(hHideMuteBtn, BM_SETCHECK, showMuteBtn ? BST_CHECKED : BST_UNCHECKED, 0);

            hHideVoiceMeter = CreateWindow("BUTTON", "Voice Level Meter",
                WS_CHILD | BS_OWNERDRAW,
                contentX, startY + gapY, 300, 30, hWnd, (HMENU)ID_HIDE_VOICE_METER, hInst, nullptr);
            SendMessage(hHideVoiceMeter, BM_SETCHECK, showVoiceMeter ? BST_CHECKED : BST_UNCHECKED, 0);

            hHideRecStatus = CreateWindow("BUTTON", "Recording Status",
                WS_CHILD | BS_OWNERDRAW,
                contentX, startY + gapY*2, 300, 30, hWnd, (HMENU)ID_HIDE_REC_STATUS, hInst, nullptr);
            SendMessage(hHideRecStatus, BM_SETCHECK, showRecStatus ? BST_CHECKED : BST_UNCHECKED, 0);

            hHideCallStats = CreateWindow("BUTTON", "Call Statistics",
                WS_CHILD | BS_OWNERDRAW,
                contentX, startY + gapY*3, 300, 30, hWnd, (HMENU)ID_HIDE_CALL_STATS, hInst, nullptr);
            SendMessage(hHideCallStats, BM_SETCHECK, showCallStats ? BST_CHECKED : BST_UNCHECKED, 0);

            hHideManualRec = CreateWindow("BUTTON", "Manual Record Buttons",
                WS_CHILD | BS_OWNERDRAW,
                contentX, startY + gapY*4, 300, 30, hWnd, (HMENU)ID_HIDE_MANUAL_REC, hInst, nullptr);
            SendMessage(hHideManualRec, BM_SETCHECK, showManualRec ? BST_CHECKED : BST_UNCHECKED, 0);

            // === Shape & Size Tab Buttons ===
            hSizeCompact = CreateWindow("BUTTON", "Compact",
                WS_CHILD | BS_PUSHBUTTON,
                contentX, startY, 120, 36, hWnd, (HMENU)ID_SIZE_COMPACT, hInst, nullptr);
            hSizeNormal = CreateWindow("BUTTON", "Normal",
                WS_CHILD | BS_PUSHBUTTON,
                contentX + 140, startY, 120, 36, hWnd, (HMENU)ID_SIZE_NORMAL, hInst, nullptr);
            hSizeWide = CreateWindow("BUTTON", "Wide",
                WS_CHILD | BS_PUSHBUTTON,
                contentX + 280, startY, 120, 36, hWnd, (HMENU)ID_SIZE_WIDE, hInst, nullptr);

            // Footer
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

            // Draw Sidebar
            RECT rcSidebar = {0, 0, SIDEBAR_WIDTH, rcClient.bottom};
            FillRect(hdc, &rcSidebar, hBrushSidebarBg);

            SetBkMode(hdc, TRANSPARENT);

            // Draw User Name at sidebar top (bold, accent colored)
            SetTextColor(hdc, colorAccent);
            SelectObject(hdc, hFontBold);
            RECT rcName = {0, 12, SIDEBAR_WIDTH, 44};
            DrawText(hdc, userName.c_str(), -1, &rcName, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            // App title below name
            SetTextColor(hdc, colorText);
            SelectObject(hdc, hFontTitle);
            RECT rcTitle = {0, 44, SIDEBAR_WIDTH, 85};
            DrawText(hdc, "MicMute", -1, &rcTitle, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            // Draw Sidebar Items
            for (int i = 0; i < TAB_COUNT; i++) {
                bool isSelected = (i == currentTab);
                bool isHovered = (i == hoverTab);
                
                RECT rcItem = {0, 100 + (i * SIDEBAR_ITEM_HEIGHT), SIDEBAR_WIDTH, 100 + ((i + 1) * SIDEBAR_ITEM_HEIGHT)};
                
                if (isSelected) {
                    RECT rcIndicator = {2, rcItem.top + 10, 6, rcItem.bottom - 10};
                    HBRUSH hAccent = CreateSolidBrush(colorAccent);
                    FillRect(hdc, &rcIndicator, hAccent);
                    DeleteObject(hAccent);
                    
                    HBRUSH hSelectedBg = CreateSolidBrush(RGB(35, 35, 50));
                    FillRect(hdc, &rcItem, hSelectedBg);
                    DeleteObject(hSelectedBg);
                } else if (isHovered) {
                    FillRect(hdc, &rcItem, hBrushSidebarHover);
                }

                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, isSelected ? colorText : (isHovered ? colorText : colorTextDim));
                SelectObject(hdc, isSelected ? hFontNormal : hFontSmall);
                
                RECT rcText = rcItem;
                rcText.left += 50;
                DrawText(hdc, tabNames[i], -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            }

            // Draw Status Text
            {
                bool isMutedStatus = IsDefaultMicMuted();
                SetTextColor(hdc, isMutedStatus ? colorMuted : colorLive);
                SelectObject(hdc, hFontStatus);
                RECT rcStatus = {SIDEBAR_WIDTH, 30, rcClient.right - 100, 70}; 
                const char* statusText = isMutedStatus ? "MICROPHONE MUTED" : "MICROPHONE LIVE";
                DrawText(hdc, statusText, -1, &rcStatus, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            }

            // Content for placeholder tabs (Appearance)
            if (currentTab == 3) {
                SetTextColor(hdc, colorTextDim);
                SelectObject(hdc, hFontTitle);
                RECT rcContent = {SIDEBAR_WIDTH, 150, rcClient.right, 250};
                DrawText(hdc, "Coming Soon", -1, &rcContent, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            }

            // Content heading for Hide/Unhide tab
            if (currentTab == 1) {
                SetTextColor(hdc, colorTextDim);
                SelectObject(hdc, hFontSmall);
                int contentX = SIDEBAR_WIDTH + 40;
                RECT rcHint = {contentX, 75, rcClient.right - 40, 95};
                DrawText(hdc, "Toggle which sections appear on the control panel:", -1, &rcHint, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            }

            // Content heading for Shape & Size tab
            if (currentTab == 2) {
                SetTextColor(hdc, colorTextDim);
                SelectObject(hdc, hFontSmall);
                int contentX = SIDEBAR_WIDTH + 40;
                RECT rcHint = {contentX, 75, rcClient.right - 40, 95};
                DrawText(hdc, "Choose control panel size:", -1, &rcHint, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

                // Show current selection indicator
                SetTextColor(hdc, colorAccent);
                SelectObject(hdc, hFontSmall);
                const char* curSize = panelSizeMode == 0 ? "Current: Compact" : (panelSizeMode == 2 ? "Current: Wide" : "Current: Normal");
                RECT rcCur = {contentX, 145, rcClient.right - 40, 165};
                DrawText(hdc, curSize, -1, &rcCur, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            }

            // Call recording stats in General tab
            if (currentTab == 0 && g_CallRecorder) {
                SetTextColor(hdc, colorAccent);
                SelectObject(hdc, hFontNormal);
                int contentX = SIDEBAR_WIDTH + 40;
                int statsY = 100 + 50 * 7; // Below all toggles

                auto t = std::time(nullptr);
                struct tm tm;
                localtime_s(&tm, &t);
                char dateBuf[32];
                strftime(dateBuf, sizeof(dateBuf), "%d %b %Y", &tm);

                int count = g_CallRecorder->GetTodayCallCount();
                char statsBuf[128];
                sprintf_s(statsBuf, "Today (%s): %d call%s recorded", dateBuf, count, count == 1 ? "" : "s");
                
                RECT rcStats = {contentX, statsY, rcClient.right - 40, statsY + 30};
                DrawText(hdc, statsBuf, -1, &rcStats, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            }

            // Custom Caption Buttons
            int btnW = 45;
            int btnH = 30;
            rcClose = {rcClient.right - btnW, 0, rcClient.right, btnH};
            rcMin = {rcClose.left - btnW, 0, rcClose.left, btnH};

            if (isPressedClose) {
                HBRUSH hPressed = CreateSolidBrush(RGB(196, 43, 28));
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

            if (isPressedMin) {
                HBRUSH hPressed = CreateSolidBrush(RGB(55, 55, 70));
                FillRect(hdc, &rcMin, hPressed);
                DeleteObject(hPressed);
            } else if (isHoverMin) {
                HBRUSH hHover = CreateSolidBrush(RGB(45, 45, 60));
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

            // Caption button hover
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

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            
            // General tab toggles
            if (wmId == ID_RUN_STARTUP) {
                isRunOnStartup = !isRunOnStartup;
                ManageStartup(isRunOnStartup);
                InvalidateRect(hStartupCheck, nullptr, FALSE);
            }
            else if (wmId == ID_SHOW_OVERLAY) {
                // Remapped to: Show Mute Button in Panel
                showMuteBtn = !showMuteBtn;
                SaveSettings();
                UpdateControlPanel();
                InvalidateRect(hOverlayCheck, nullptr, FALSE);
            }
            else if (wmId == ID_SHOW_METER) {
                // Remapped to: Show Voice Meter in Panel
                showVoiceMeter = !showVoiceMeter;
                SaveSettings();
                UpdateControlPanel();
                InvalidateRect(hMeterCheck, nullptr, FALSE);
            }
            else if (wmId == ID_SHOW_RECORDER) {
                // Remapped to: Show Recorder Status + Manual Rec in Panel
                bool newState = !showRecStatus; 
                showRecStatus = newState;
                showManualRec = newState;
                SaveSettings();
                UpdateControlPanel();
                InvalidateRect(hRecorderCheck, nullptr, FALSE);
            }
            else if (wmId == ID_SHOW_NOTIFICATIONS) {
                showNotifications = !showNotifications;
                SaveSettings();
                InvalidateRect((HWND)lParam, nullptr, FALSE);
            }
            else if (wmId == ID_AUTO_RECORD_CALLS) {
                // Security Check
                if (!autoRecordCalls) { // If turning ON
                    if (!hasAgreedToDisclaimer) {
                        int ret = MessageBox(hWnd, 
                            "LEGAL DISCLAIMER:\n\n"
                            "Call recording laws vary by jurisdiction. In many areas, you must notify all parties "
                            "that the call is being recorded. By enabling this feature, you acknowledge that you "
                            "understand and will comply with all applicable local, state, and federal laws regarding "
                            "call recording.\n\n"
                            "Do you agree to these terms?", 
                            "Legal Warning", MB_YESNO | MB_ICONWARNING);
                        
                        if (ret != IDYES) return 0;
                        
                        hasAgreedToDisclaimer = true;
                        SaveSettings();
                    }
                }

                // Password Protection for ANY toggle (On or Off)
                if (!PromptForPassword(hWnd)) {
                    return 0; // Password incorrect or cancelled
                }

                autoRecordCalls = !autoRecordCalls;
                SaveSettings();
                if (g_CallRecorder) {
                    if (autoRecordCalls) g_CallRecorder->Enable();
                    else g_CallRecorder->Disable();
                }
                InvalidateRect(hAutoRecordCheck, nullptr, FALSE);
            }
            else if (wmId == ID_BEEP_ON_CALL) {
                beepOnCall = !beepOnCall;
                SaveSettings();
                InvalidateRect(hBeepCheck, nullptr, FALSE);
            }
            // Hide/Unhide tab toggles
            else if (wmId == ID_HIDE_MUTE_BTN) {
                showMuteBtn = !showMuteBtn;
                SaveSettings();
                UpdateControlPanel();
                InvalidateRect(hHideMuteBtn, nullptr, FALSE);
            }
            else if (wmId == ID_HIDE_VOICE_METER) {
                showVoiceMeter = !showVoiceMeter;
                SaveSettings();
                UpdateControlPanel();
                InvalidateRect(hHideVoiceMeter, nullptr, FALSE);
            }
            else if (wmId == ID_HIDE_REC_STATUS) {
                showRecStatus = !showRecStatus;
                SaveSettings();
                UpdateControlPanel();
                InvalidateRect(hHideRecStatus, nullptr, FALSE);
            }
            else if (wmId == ID_HIDE_CALL_STATS) {
                showCallStats = !showCallStats;
                SaveSettings();
                UpdateControlPanel();
                InvalidateRect(hHideCallStats, nullptr, FALSE);
            }
            else if (wmId == ID_HIDE_MANUAL_REC) {
                showManualRec = !showManualRec;
                SaveSettings();
                UpdateControlPanel();
                InvalidateRect(hHideManualRec, nullptr, FALSE);
            }
            // Shape & Size tab
            else if (wmId == ID_SIZE_COMPACT) {
                panelSizeMode = 0;
                SaveSettings();
                ResizeControlPanel();
                InvalidateRect(hWnd, nullptr, TRUE);
            }
            else if (wmId == ID_SIZE_NORMAL) {
                panelSizeMode = 1;
                SaveSettings();
                ResizeControlPanel();
                InvalidateRect(hWnd, nullptr, TRUE);
            }
            else if (wmId == ID_SIZE_WIDE) {
                panelSizeMode = 2;
                SaveSettings();
                ResizeControlPanel();
                InvalidateRect(hWnd, nullptr, TRUE);
            }
            // Tray
            else if (wmId == ID_TRAY_OPEN) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            }
            else if (wmId == ID_CHECK_UPDATE) {
                AutoUpdater::CheckForUpdateAsync(false); // Not silent = show dialogs
            }

            else if (wmId == ID_TRAY_EXIT) {
                if (hOverlayWnd) DestroyWindow(hOverlayWnd);
                if (hMeterWnd) DestroyWindow(hMeterWnd);
                if (hRecorderWnd) CleanupRecorder(), DestroyWindow(hRecorderWnd);
                if (hControlPanel) DestroyWindow(hControlPanel);
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
                // Update control panel meter too
                UpdateControlPanel();
                
                if (g_CallRecorder && g_CallRecorder->IsEnabled()) {
                    g_CallRecorder->Poll();
                    if (hRecorderWnd && showRecorder) {
                        InvalidateRect(hRecorderWnd, nullptr, FALSE);
                    }
                }
                
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
                    if (hStatus) ShowWindow(hStatus, SW_HIDE);
                }
            } else if (wParam == 2) {
                if (skipTimerCycles > 0) { skipTimerCycles--; break; }
                UpdateUIState();
            }
            break;

        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);

            POINT pt; GetCursorPos(&pt);
            RECT rc; GetWindowRect(hWnd, &rc);
            
            int border = 6;
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

            if (PtInRect(&rcClose, clientPt) || PtInRect(&rcMin, clientPt)) {
                return HTCLIENT;
            }

            if (clientPt.y < 60) {
                return HTCAPTION;
            }

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
            return TRUE;

        case WM_ENDSESSION:
            if (wParam == TRUE) {
                SaveSettings();
                if (hRecorderWnd) SaveRecorderPosition();
                SaveControlPanelPosition();
            }
            return 0;

        case WM_DESTROY:
            SaveSettings();
            if (hRecorderWnd) SaveRecorderPosition();
            SaveControlPanelPosition();
            
            RemoveTrayIcon();
            PostQuitMessage(0);
            break;

        case WM_APP_MUTE_CHANGED:
            UpdateUIState();
            UpdateControlPanel();
            return 0;

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}
