#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <string>
#include "resource.h"
#include "globals.h"
#include "settings.h"
#include "audio.h"
#include "tray.h"
#include "overlay.h"
#include "ui.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")

// Main Window Procedure
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

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

    // Window size and position
    int width = 450;
    int height = 420;
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
        int result = MessageBox(hMainWnd, 
            "Would you like MicMute-S to start automatically with Windows?\n\nThis is recommended for seamless microphone control.",
            "Enable Startup?", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            ManageStartup(true);
            isRunOnStartup = true;
        }
        
        result = MessageBox(hMainWnd,
            "Would you like to show a floating mute button on screen?\n\nThis small draggable button stays visible for quick mute/unmute.",
            "Enable Floating Button?", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            showOverlay = true;
        }
        
        result = MessageBox(hMainWnd,
            "Would you like to show a voice level meter?\n\nThis shows if your microphone is picking up sound.",
            "Enable Voice Meter?", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            showMeter = true;
        }
        
        SaveSettings();
    }

    // Create overlays if enabled
    if (showOverlay) CreateOverlayWindow(hInstance);
    if (showMeter) CreateMeterWindow(hInstance);

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
    CloseHandle(hMutex);
    
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hStartupCheck, hOverlayCheck, hMeterCheck;

    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
            
            CreateWindow("STATIC", "MicMute-S", WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 20, 450, 50, hWnd, NULL, hInst, NULL);
            SendMessage(GetDlgItem(hWnd, 0), WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            CreateWindow("STATIC", "Checking...", WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 75, 450, 35, hWnd, (HMENU)ID_STATUS_LABEL, hInst, NULL);

            CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ, 50, 125, 350, 2, hWnd, NULL, hInst, NULL);

            hStartupCheck = CreateWindow("BUTTON", "  Launch on Windows startup", 
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 80, 145, 300, 28, hWnd, (HMENU)ID_RUN_STARTUP, hInst, NULL);
            SendMessage(hStartupCheck, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessage(hStartupCheck, BM_SETCHECK, isRunOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);

            hOverlayCheck = CreateWindow("BUTTON", "  Show floating mute button", 
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 80, 178, 300, 28, hWnd, (HMENU)ID_SHOW_OVERLAY, hInst, NULL);
            SendMessage(hOverlayCheck, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessage(hOverlayCheck, BM_SETCHECK, showOverlay ? BST_CHECKED : BST_UNCHECKED, 0);

            hMeterCheck = CreateWindow("BUTTON", "  Show voice level meter", 
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 80, 211, 300, 28, hWnd, (HMENU)ID_SHOW_METER, hInst, NULL);
            SendMessage(hMeterCheck, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessage(hMeterCheck, BM_SETCHECK, showMeter ? BST_CHECKED : BST_UNCHECKED, 0);

            CreateWindow("STATIC", "Click tray/floating button to toggle mute", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 0, 260, 450, 18, hWnd, NULL, hInst, NULL);
            CreateWindow("STATIC", "Drag overlays to move  |  Right-click to hide", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 0, 280, 450, 18, hWnd, NULL, hInst, NULL);
            CreateWindow("STATIC", "Minimize to hide  |  Right-click tray for menu", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 0, 300, 450, 18, hWnd, NULL, hInst, NULL);
            CreateWindow("STATIC", "by Suvojeet Sengupta", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 0, 340, 450, 18, hWnd, NULL, hInst, NULL);
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, colorBg);
            if (GetDlgCtrlID((HWND)lParam) == ID_STATUS_LABEL) {
                SetTextColor(hdc, IsDefaultMicMuted() ? colorMuted : colorLive);
                SelectObject(hdc, hFontStatus);
            } else {
                SetTextColor(hdc, colorText);
            }
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
            else if (wmId == ID_TRAY_OPEN) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            }
            else if (wmId == ID_TRAY_EXIT) {
                if (hOverlayWnd) DestroyWindow(hOverlayWnd);
                if (hMeterWnd) DestroyWindow(hMeterWnd);
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

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}


