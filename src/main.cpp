#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <commctrl.h> // Common Controls (Tabs)
#include <string>
#include "resource.h"
#include "globals.h"
#include "settings.h"
#include "audio.h"
#include "tray.h"
#include "overlay.h"
#include "ui.h"
#include "recorder.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

// Main Window Procedure
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Controls
HWND hTabControl = NULL;
// General Tab
HWND hStartupCheck, hOverlayCheck, hMeterCheck, hRecorderCheck;
// Placeholders
HWND hLabelHotkeys, hLabelAudio, hLabelAppearance;

void UpdateTabVisibility(int tabIndex) {
    bool isGeneral = (tabIndex == 0);
    bool isHotkeys = (tabIndex == 1);
    bool isAudio = (tabIndex == 2);
    bool isAppearance = (tabIndex == 3);

    int showGeneral = isGeneral ? SW_SHOW : SW_HIDE;
    ShowWindow(hStartupCheck, showGeneral);
    ShowWindow(hOverlayCheck, showGeneral);
    ShowWindow(hMeterCheck, showGeneral);
    ShowWindow(hRecorderCheck, showGeneral);
    
    ShowWindow(hLabelHotkeys, isHotkeys ? SW_SHOW : SW_HIDE);
    ShowWindow(hLabelAudio, isAudio ? SW_SHOW : SW_HIDE);
    ShowWindow(hLabelAppearance, isAppearance ? SW_SHOW : SW_HIDE);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Single instance check using a named mutex
    HANDLE hMutex = CreateMutex(NULL, TRUE, "MicMuteS_SingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is already running - find and activate its window
        HWND hExisting = FindWindow("MicMuteS_Class", "MicMute-S");
        if (hExisting) {
            ShowWindow(hExisting, SW_RESTORE);
            SetForegroundWindow(hExisting);
        }
        CloseHandle(hMutex);
        return 0;
    }

    // Init Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

    InitializeAudio();

    // Create brushes
    hBrushBg = CreateSolidBrush(colorBg);
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
    wc.hbrBackground = hBrushBg;
    wc.lpszClassName = "MicMuteS_Class";
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    RegisterClassEx(&wc);

    // Register overlay window class
    WNDCLASSEX wcOverlay = {0};
    wcOverlay.cbSize = sizeof(WNDCLASSEX);
    wcOverlay.style = CS_HREDRAW | CS_VREDRAW;
    wcOverlay.lpfnWndProc = OverlayWndProc;
    wcOverlay.hInstance = hInstance;
    wcOverlay.hCursor = LoadCursor(NULL, IDC_HAND);
    wcOverlay.hbrBackground = hBrushChroma;
    wcOverlay.lpszClassName = "MicMuteS_Overlay";
    RegisterClassEx(&wcOverlay);

    // Register meter window class
    WNDCLASSEX wcMeter = {0};
    wcMeter.cbSize = sizeof(WNDCLASSEX);
    wcMeter.style = CS_HREDRAW | CS_VREDRAW;
    wcMeter.lpfnWndProc = MeterWndProc;
    wcMeter.hInstance = hInstance;
    wcMeter.hCursor = LoadCursor(NULL, IDC_SIZEALL);
    wcMeter.hbrBackground = hBrushMeterBg;
    wcMeter.lpszClassName = "MicMuteS_Meter";
    RegisterClassEx(&wcMeter);

    // Register recorder window class
    WNDCLASSEX wcRec = {0};
    wcRec.cbSize = sizeof(WNDCLASSEX);
    wcRec.style = CS_HREDRAW | CS_VREDRAW;
    wcRec.lpfnWndProc = RecorderWndProc;
    wcRec.hInstance = hInstance;
    wcRec.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcRec.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // Will be painted over
    wcRec.lpszClassName = "MicMuteS_Recorder";
    RegisterClassEx(&wcRec);

    // Window size and position
    int width = 450;
    int height = 500; 
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

    // Apply rounded corners (Windows 11+)
    DWM_WINDOW_CORNER_PREFERENCE cornerPref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hMainWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    // Pre-load icons to prevent memory leak
    hIconMicOn = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MIC_ON));
    hIconMicOff = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MIC_OFF));

    // Create Fonts
    hFontTitle = CreateFont(42, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontStatus = CreateFont(26, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontNormal = CreateFont(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontSmall = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontOverlay = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    // Load settings
    LoadSettings();
    
    // First run dialog
    if (!IsStartupEnabled()) {
        if (MessageBox(hMainWnd, "Enable Startup with Windows?", "MicMute-S", MB_YESNO | MB_ICONQUESTION) == IDYES) {
            ManageStartup(true);
            isRunOnStartup = true;
        }
        if (MessageBox(hMainWnd, "Enable Floating Mute Button?", "MicMute-S", MB_YESNO | MB_ICONQUESTION) == IDYES) showOverlay = true;
        if (MessageBox(hMainWnd, "Enable Voice Activity Meter?", "MicMute-S", MB_YESNO | MB_ICONQUESTION) == IDYES) showMeter = true;
        SaveSettings();
    }

    // Create overlays if enabled
    if (showOverlay) CreateOverlayWindow(hInstance);
    if (showMeter) CreateMeterWindow(hInstance);
    if (showRecorder) CreateRecorderWindow(hInstance);

    AddTrayIcon(hMainWnd);
    UpdateUIState();

    // Fast timer for meter updates (50ms)
    SetTimer(hMainWnd, 1, 50, NULL);
    // Slow timer for UI updates (2 seconds)
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
            
            // Header
            CreateWindow("STATIC", "MicMute-S", WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 10, 450, 50, hWnd, NULL, hInst, NULL);
            SendMessage(GetDlgItem(hWnd, 0), WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            CreateWindow("STATIC", "Checking...", WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 60, 450, 35, hWnd, (HMENU)ID_STATUS_LABEL, hInst, NULL);
            
            // Tab Control
            hTabControl = CreateWindow(WC_TABCONTROL, "", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 
                10, 110, 415, 300, hWnd, (HMENU)ID_TAB_CONTROL, hInst, NULL);
            SendMessage(hTabControl, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

            TCITEM tie;
            tie.mask = TCIF_TEXT;
            
            tie.pszText = "General";
            TabCtrl_InsertItem(hTabControl, 0, &tie);
            
            tie.pszText = "Hotkeys";
            TabCtrl_InsertItem(hTabControl, 1, &tie);
            
            tie.pszText = "Audio";
            TabCtrl_InsertItem(hTabControl, 2, &tie);
            
            tie.pszText = "Appearance";
            TabCtrl_InsertItem(hTabControl, 3, &tie);

            // -- GENERAL TAB CONTROLS --
            // Position relative to Tab Client Area (approx inside 10,110 + padding)
            // Tab client area usually starts ~30px down.
            // Let's use absolute positioning over the tab background for simplicity in this GDI app
            int tabY_offset = 150; 
            int chkX = 30;

            hStartupCheck = CreateWindow("BUTTON", "  Launch on Windows startup", 
                WS_CHILD | BS_AUTOCHECKBOX, chkX, tabY_offset, 300, 28, hWnd, (HMENU)ID_RUN_STARTUP, hInst, NULL);
            SendMessage(hStartupCheck, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessage(hStartupCheck, BM_SETCHECK, isRunOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);

            hOverlayCheck = CreateWindow("BUTTON", "  Show floating mute button", 
                WS_CHILD | BS_AUTOCHECKBOX, chkX, tabY_offset + 35, 300, 28, hWnd, (HMENU)ID_SHOW_OVERLAY, hInst, NULL);
            SendMessage(hOverlayCheck, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessage(hOverlayCheck, BM_SETCHECK, showOverlay ? BST_CHECKED : BST_UNCHECKED, 0);

            hMeterCheck = CreateWindow("BUTTON", "  Show voice level meter", 
                WS_CHILD | BS_AUTOCHECKBOX, chkX, tabY_offset + 70, 300, 28, hWnd, (HMENU)ID_SHOW_METER, hInst, NULL);
            SendMessage(hMeterCheck, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessage(hMeterCheck, BM_SETCHECK, showMeter ? BST_CHECKED : BST_UNCHECKED, 0);

            hRecorderCheck = CreateWindow("BUTTON", "  Show call recorder", 
                WS_CHILD | BS_AUTOCHECKBOX, chkX, tabY_offset + 105, 300, 28, hWnd, (HMENU)ID_SHOW_RECORDER, hInst, NULL);
            SendMessage(hRecorderCheck, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessage(hRecorderCheck, BM_SETCHECK, showRecorder ? BST_CHECKED : BST_UNCHECKED, 0);

            // -- PLACEHOLDERS --
            hLabelHotkeys = CreateWindow("STATIC", "Global Hotkeys Coming Soon...", WS_CHILD | SS_CENTER, 
                20, 200, 390, 50, hWnd, NULL, hInst, NULL);
            SendMessage(hLabelHotkeys, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            
            hLabelAudio = CreateWindow("STATIC", "Audio Device Selector Coming Soon...", WS_CHILD | SS_CENTER, 
                20, 200, 390, 50, hWnd, NULL, hInst, NULL);
            SendMessage(hLabelAudio, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

            hLabelAppearance = CreateWindow("STATIC", "Theme Settings Coming Soon...", WS_CHILD | SS_CENTER, 
                20, 200, 390, 50, hWnd, NULL, hInst, NULL);
            SendMessage(hLabelAppearance, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

            // Footer
            CreateWindow("STATIC", "by Suvojeet Sengupta", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 0, 430, 450, 18, hWnd, NULL, hInst, NULL);
                
            // Initial Show
            UpdateTabVisibility(0);
            break;
        }

        case WM_NOTIFY: {
            NMHDR* pnm = (NMHDR*)lParam;
            if (pnm->idFrom == ID_TAB_CONTROL && pnm->code == TCN_SELCHANGE) {
                int index = TabCtrl_GetCurSel(hTabControl);
                UpdateTabVisibility(index);
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
                SetTextColor(hdc, colorText);
            }
            return (LRESULT)hBrushBg;
        }
        
        // Make sure Checkboxes have transparency on the tab bg (which is colorBg)
        case WM_CTLCOLORBTN: {
             // For buttons, if we want them to blend with dark theme:
             // Standard buttons don't support custom colors easily without owner draw.
             // But existing code didn't handle it, so they likely look 'classic' or 'themed'
             // If we really want them dark, we need OwnerDraw or Manifest + dark theme hook.
             // For now, let's keep it simple as before.
             return (LRESULT)hBrushBg; 
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == ID_RUN_STARTUP) {
                isRunOnStartup = (SendMessage(hStartupCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                ManageStartup(isRunOnStartup);
            }
            else if (wmId == ID_SHOW_OVERLAY) {
                showOverlay = (SendMessage(hOverlayCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SaveSettings();
                if (showOverlay) {
                    if (!hOverlayWnd) CreateOverlayWindow(GetModuleHandle(NULL));
                    else ShowWindow(hOverlayWnd, SW_SHOW);
                    UpdateOverlay();
                } else if (hOverlayWnd) ShowWindow(hOverlayWnd, SW_HIDE);
            }
            else if (wmId == ID_SHOW_METER) {
                showMeter = (SendMessage(hMeterCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SaveSettings();
                if (showMeter) {
                    if (!hMeterWnd) CreateMeterWindow(GetModuleHandle(NULL));
                    else ShowWindow(hMeterWnd, SW_SHOW);
                } else if (hMeterWnd) ShowWindow(hMeterWnd, SW_HIDE);
            }
            else if (wmId == ID_SHOW_RECORDER) {
                showRecorder = (SendMessage(hRecorderCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SaveSettings();
                if (showRecorder) {
                    if (!hRecorderWnd) CreateRecorderWindow(GetModuleHandle(NULL));
                    else ShowWindow(hRecorderWnd, SW_SHOW);
                } else if (hRecorderWnd) ShowWindow(hRecorderWnd, SW_HIDE);
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
                // Fast timer - update meter
                UpdateMeter();
            } else if (wParam == 2) {
                // Slow timer - update UI
                if (skipTimerCycles > 0) { skipTimerCycles--; break; }
                UpdateUIState();
            }
            break;

        case WM_NCHITTEST: {
            // Make the window draggable from anywhere
            LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);
            if (hit == HTCLIENT) {
                return HTCAPTION;
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


