#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <string>
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

// General Tab Controls
HWND hStartupCheck, hOverlayCheck, hMeterCheck, hRecorderCheck, hNotifyCheck, hAutoRecordCheck;

// Tab Labels
const char* tabNames[] = { "General", "Hotkeys", "Audio", "Appearance" };
#define TAB_COUNT 4

void UpdateControlVisibility() {
    bool isGeneral = (currentTab == 0);
    // Other tabs are placeholders for now
    
    int showGeneral = isGeneral ? SW_SHOW : SW_HIDE;
    ShowWindow(hStartupCheck, showGeneral);
    ShowWindow(hOverlayCheck, showGeneral);
    ShowWindow(hMeterCheck, showGeneral);
    ShowWindow(hRecorderCheck, showGeneral);
    ShowWindow(hNotifyCheck, showGeneral);
    ShowWindow(hAutoRecordCheck, showGeneral);
    
    // Repaint to clear/draw proper background
    InvalidateRect(hMainWnd, NULL, TRUE);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize COM
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) return 0;

    // Single Instance
    HANDLE hMutex = CreateMutex(NULL, TRUE, "MicMuteS_SingleInstanceMutex");
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
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
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
    wcOverlay.hCursor = LoadCursor(NULL, IDC_HAND);
    wcOverlay.hbrBackground = hBrushChroma;
    wcOverlay.lpszClassName = "MicMuteS_Overlay";
    RegisterClassEx(&wcOverlay);

    WNDCLASSEX wcMeter = {0};
    wcMeter.cbSize = sizeof(WNDCLASSEX);
    wcMeter.style = CS_HREDRAW | CS_VREDRAW;
    wcMeter.lpfnWndProc = MeterWndProc;
    wcMeter.hInstance = hInstance;
    wcMeter.hCursor = LoadCursor(NULL, IDC_SIZEALL);
    wcMeter.hbrBackground = hBrushMeterBg;
    wcMeter.lpszClassName = "MicMuteS_Meter";
    RegisterClassEx(&wcMeter);

    WNDCLASSEX wcRec = {0};
    wcRec.cbSize = sizeof(WNDCLASSEX);
    wcRec.style = CS_HREDRAW | CS_VREDRAW;
    wcRec.lpfnWndProc = RecorderWndProc;
    wcRec.hInstance = hInstance;
    wcRec.hCursor = LoadCursor(NULL, IDC_ARROW);
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
        NULL, NULL, hInstance, NULL
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

    if (!IsStartupEnabled()) {
         // Simple first run logic could go here, omitting for brevity in this refactor
         if (MessageBox(hMainWnd, "Enable Startup with Windows?", "MicMute-S", MB_YESNO | MB_ICONQUESTION) == IDYES) {
            ManageStartup(true);
            isRunOnStartup = true;
         }
         SaveSettings();
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

    SetTimer(hMainWnd, 1, 50, NULL);
    SetTimer(hMainWnd, 2, 2000, NULL);

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
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
            
            // Status Label (Top Right Area)
            // REMOVED: Static control blocks dragging. We will draw this manually in WM_PAINT.
            // CreateWindow("STATIC", "Checking...", WS_VISIBLE | WS_CHILD | SS_CENTER, 
            //    SIDEBAR_WIDTH, 30, 850 - SIDEBAR_WIDTH, 40, hWnd, (HMENU)ID_STATUS_LABEL, hInst, NULL);

            // -- GENERAL TAB TOGGLES --
            // Positioned in the content area (Right of Sidebar)
            int contentX = SIDEBAR_WIDTH + 40;
            int startY = 100;
            int gapY = 50;

            hStartupCheck = CreateWindow("BUTTON", "Launch on Windows startup", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY, 300, 30, hWnd, (HMENU)ID_RUN_STARTUP, hInst, NULL);
            SendMessage(hStartupCheck, BM_SETCHECK, isRunOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);

            hOverlayCheck = CreateWindow("BUTTON", "Show floating mute button", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY, 300, 30, hWnd, (HMENU)ID_SHOW_OVERLAY, hInst, NULL);
            SendMessage(hOverlayCheck, BM_SETCHECK, showOverlay ? BST_CHECKED : BST_UNCHECKED, 0);

            hMeterCheck = CreateWindow("BUTTON", "Show voice level meter", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY*2, 300, 30, hWnd, (HMENU)ID_SHOW_METER, hInst, NULL);
            SendMessage(hMeterCheck, BM_SETCHECK, showMeter ? BST_CHECKED : BST_UNCHECKED, 0);

            hRecorderCheck = CreateWindow("BUTTON", "Show call recorder", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY*3, 300, 30, hWnd, (HMENU)ID_SHOW_RECORDER, hInst, NULL);
            SendMessage(hRecorderCheck, BM_SETCHECK, showRecorder ? BST_CHECKED : BST_UNCHECKED, 0);

            hNotifyCheck = CreateWindow("BUTTON", "Show system notifications", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY*4, 300, 30, hWnd, (HMENU)ID_SHOW_NOTIFICATIONS, hInst, NULL);
            SendMessage(hNotifyCheck, BM_SETCHECK, showNotifications ? BST_CHECKED : BST_UNCHECKED, 0);

            hAutoRecordCheck = CreateWindow("BUTTON", "Auto record calls (voice detection)", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY*5, 350, 30, hWnd, (HMENU)ID_AUTO_RECORD_CALLS, hInst, NULL);
            SendMessage(hAutoRecordCheck, BM_SETCHECK, autoRecordCalls ? BST_CHECKED : BST_UNCHECKED, 0);

            // Footer - Use dynamic positioning
            CreateWindow("STATIC", "by Suvojeet Sengupta", 
                 WS_VISIBLE | WS_CHILD | SS_RIGHT, 0, 0, 0, 0, hWnd, (HMENU)9999, hInst, NULL);

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

            // Draw Sidebar Items
            for (int i = 0; i < TAB_COUNT; i++) {
                DrawSidebarItem(hdc, i, tabNames[i], currentTab, hoverTab);
            }

            // Draw Status Text (Directly on Main Window)
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
                        InvalidateRect(hWnd, NULL, TRUE); 
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

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            
            // Handle Toggles
            // Note: For OwnerDraw buttons, we still get BN_CLICKED
            if (wmId == ID_RUN_STARTUP) {
                isRunOnStartup = !isRunOnStartup;
                ManageStartup(isRunOnStartup);
                InvalidateRect(hStartupCheck, NULL, FALSE);
            }
            else if (wmId == ID_SHOW_OVERLAY) {
                showOverlay = !showOverlay;
                SaveSettings();
                if (showOverlay) {
                    if (!hOverlayWnd) CreateOverlayWindow(GetModuleHandle(NULL));
                    else ShowWindow(hOverlayWnd, SW_SHOW);
                    UpdateOverlay();
                } else if (hOverlayWnd) ShowWindow(hOverlayWnd, SW_HIDE);
                InvalidateRect(hOverlayCheck, NULL, FALSE);
            }
            else if (wmId == ID_SHOW_METER) {
                showMeter = !showMeter;
                SaveSettings();
                if (showMeter) {
                    if (!hMeterWnd) CreateMeterWindow(GetModuleHandle(NULL));
                    else ShowWindow(hMeterWnd, SW_SHOW);
                } else if (hMeterWnd) ShowWindow(hMeterWnd, SW_HIDE);
                InvalidateRect(hMeterCheck, NULL, FALSE);
            }
            else if (wmId == ID_SHOW_RECORDER) {
                showRecorder = !showRecorder;
                SaveSettings();
                if (showRecorder) {
                    if (!hRecorderWnd) CreateRecorderWindow(GetModuleHandle(NULL));
                    else ShowWindow(hRecorderWnd, SW_SHOW);
                } else if (hRecorderWnd) ShowWindow(hRecorderWnd, SW_HIDE);
                InvalidateRect(hRecorderCheck, NULL, FALSE);
            }
            else if (wmId == ID_SHOW_NOTIFICATIONS) {
                showNotifications = !showNotifications;
                SaveSettings();
                InvalidateRect((HWND)lParam, NULL, FALSE);
            }
            else if (wmId == ID_AUTO_RECORD_CALLS) {
                autoRecordCalls = !autoRecordCalls;
                SaveSettings();
                if (g_CallRecorder) {
                    if (autoRecordCalls) {
                        g_CallRecorder->Enable();
                    } else {
                        g_CallRecorder->Disable();
                    }
                }
                InvalidateRect(hAutoRecordCheck, NULL, FALSE);
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
                AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, "Exit");
                SetForegroundWindow(hWnd);
                TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
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
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }

        case WM_TIMER:
            if (wParam == 1) {
                UpdateMeter();
                // Poll call auto-recorder for voice detection
                if (g_CallRecorder && g_CallRecorder->IsEnabled()) {
                    g_CallRecorder->Poll();
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

        case WM_DESTROY:
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


