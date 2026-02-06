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

// General Tab Controls
HWND hStartupCheck, hOverlayCheck, hMeterCheck, hRecorderCheck;

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
    CleanupRecorder();
    CloseHandle(hMutex);
    
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
            
            // App Title in Sidebar
            // We'll draw it in WM_PAINT instead of a static control for better look
            
            // Status Label (Top Right Area)
             CreateWindow("STATIC", "Checking...", WS_VISIBLE | WS_CHILD | SS_CENTER, 
                SIDEBAR_WIDTH, 30, 850 - SIDEBAR_WIDTH, 40, hWnd, (HMENU)ID_STATUS_LABEL, hInst, NULL);

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

            HWND hNotifyCheck = CreateWindow("BUTTON", "Show system notifications", 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
                contentX, startY + gapY*4, 300, 30, hWnd, (HMENU)ID_SHOW_NOTIFICATIONS, hInst, NULL);
            SendMessage(hNotifyCheck, BM_SETCHECK, showNotifications ? BST_CHECKED : BST_UNCHECKED, 0);

            // Footer
            RECT rcClient; GetClientRect(hWnd, &rcClient); // Note: Client Rect not available in CREATE? Width is known though.
            CreateWindow("STATIC", "by Suvojeet Sengupta", 
                 WS_VISIBLE | WS_CHILD | SS_RIGHT, 850 - 160, 600 - 60, 140, 20, hWnd, NULL, hInst, NULL);

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

            // Content Headings for placeholder tabs
            if (currentTab != 0) {
                SetTextColor(hdc, colorTextDim);
                SelectObject(hdc, hFontTitle);
                RECT rcContent = {SIDEBAR_WIDTH, 150, rcClient.right, 250};
                DrawText(hdc, "Coming Soon", -1, &rcContent, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            }

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
                // Invalidate only sidebar area for performance
                RECT rcSidebar = {0, 0, SIDEBAR_WIDTH, 500};
                InvalidateRect(hWnd, &rcSidebar, FALSE);
                
                // Track mouse leave to reset hover
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                TrackMouseEvent(&tme);
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
            
            if (x < SIDEBAR_WIDTH) {
                for (int i = 0; i < TAB_COUNT; i++) {
                    int itemTop = 100 + (i * SIDEBAR_ITEM_HEIGHT);
                    int itemBottom = itemTop + SIDEBAR_ITEM_HEIGHT;
                    if (y >= itemTop && y < itemBottom) {
                        currentTab = i;
                        UpdateControlVisibility();
                        // Invalidate full window to redraw title/content
                        InvalidateRect(hWnd, NULL, TRUE); 
                        break;
                    }
                }
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, colorBg);
            int id = GetDlgCtrlID((HWND)lParam);
            if (id == ID_STATUS_LABEL) {
                SetTextColor(hdc, IsDefaultMicMuted() ? colorMuted : colorLive);
                SelectObject(hdc, hFontStatus);
            } else {
                SetTextColor(hdc, colorTextDim);
                SelectObject(hdc, hFontSmall);
            }
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

        case WM_TIMER:
            if (wParam == 1) {
                UpdateMeter();
            } else if (wParam == 2) {
                if (skipTimerCycles > 0) { skipTimerCycles--; break; }
                UpdateUIState();
            }
            break;

        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);
            if (hit == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hWnd, &pt);

                // Allow dragging from top area (e.g., top 50 pixels)
                // Also allow dragging from empty parts of sidebar?
                if (pt.y < 50) {
                    return HTCAPTION;
                }
            }
            return hit;
        }

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                ShowWindow(hWnd, SW_HIDE);
                return 0;
            }
            break;

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


