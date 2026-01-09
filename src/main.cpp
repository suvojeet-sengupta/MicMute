#include <windows.h>
#include <shellapi.h>
#include <string>
#include "resource.h"
#include "audio.h"

#pragma comment(lib, "gdi32.lib")

// Global Variables
NOTIFYICONDATA nid;
HWND hMainWnd;
bool isRunOnStartup = false;
HFONT hFontTitle;
HFONT hFontStatus;
HFONT hFontNormal;
HFONT hFontSmall;
DWORD lastToggleTime = 0;
int skipTimerCycles = 0;

// Colors
HBRUSH hBrushBg;
COLORREF colorBg = RGB(30, 30, 40);
COLORREF colorText = RGB(220, 220, 230);
COLORREF colorMuted = RGB(239, 68, 68);
COLORREF colorLive = RGB(34, 197, 94);

// Function Prototypes
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon();
void UpdateTrayIcon(bool isMuted);
void ToggleMute();
void UpdateUIState();
void ManageStartup(bool enable);
bool IsStartupEnabled();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    InitializeAudio();

    // Create dark background brush
    hBrushBg = CreateSolidBrush(colorBg);

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

    // Window size and position - BIGGER
    int width = 450;
    int height = 350;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;

    hMainWnd = CreateWindowEx(
        0,
        "MicMuteS_Class", 
        "MicMute-S",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, height, 
        NULL, NULL, hInstance, NULL
    );

    // Create Fonts - BIGGER
    hFontTitle = CreateFont(42, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontStatus = CreateFont(26, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontNormal = CreateFont(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontSmall = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    // Check if startup is enabled, if not prompt user
    isRunOnStartup = IsStartupEnabled();
    if (!isRunOnStartup) {
        int result = MessageBox(hMainWnd, 
            "Would you like MicMute-S to start automatically when Windows starts?\n\nThis is recommended for seamless microphone control.",
            "Enable Startup?",
            MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            ManageStartup(true);
            isRunOnStartup = true;
        }
    }

    AddTrayIcon(hMainWnd);
    UpdateUIState();

    // Timer for UI updates (every 2 seconds)
    SetTimer(hMainWnd, 1, 2000, NULL);

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    DeleteObject(hBrushBg);
    DeleteObject(hFontTitle);
    DeleteObject(hFontStatus);
    DeleteObject(hFontNormal);
    DeleteObject(hFontSmall);
    UninitializeAudio();
    
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hStartupCheck;

    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
            
            // App Title (centered in 450 width)
            HWND hTitle = CreateWindow("STATIC", "MicMute-S", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 30, 450, 50, hWnd, NULL, hInst, NULL);
            SendMessage(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            // Status indicator
            HWND hStatus = CreateWindow("STATIC", "Checking...", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 90, 450, 35, hWnd, (HMENU)ID_STATUS_LABEL, hInst, NULL);
            SendMessage(hStatus, WM_SETFONT, (WPARAM)hFontStatus, TRUE);

            // Divider line
            CreateWindow("STATIC", "", 
                WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ, 
                50, 145, 350, 2, hWnd, NULL, hInst, NULL);

            // Startup checkbox
            hStartupCheck = CreateWindow("BUTTON", "  Launch on Windows startup", 
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 
                90, 170, 280, 30, hWnd, (HMENU)ID_RUN_STARTUP, hInst, NULL);
            SendMessage(hStartupCheck, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessage(hStartupCheck, BM_SETCHECK, isRunOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);

            // Instructions
            HWND hInstr1 = CreateWindow("STATIC", "Click tray icon to toggle mute", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 220, 450, 20, hWnd, NULL, hInst, NULL);
            SendMessage(hInstr1, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

            HWND hInstr2 = CreateWindow("STATIC", "Right-click for menu  |  Minimize to hide", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 242, 450, 20, hWnd, NULL, hInst, NULL);
            SendMessage(hInstr2, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

            // Author
            HWND hAuthor = CreateWindow("STATIC", "by Suvojeet Sengupta", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 280, 450, 20, hWnd, NULL, hInst, NULL);
            SendMessage(hAuthor, WM_SETFONT, (WPARAM)hFontSmall, TRUE);
            
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            
            SetBkColor(hdcStatic, colorBg);
            
            // Check if this is the status label
            if (GetDlgCtrlID(hCtrl) == ID_STATUS_LABEL) {
                bool muted = IsDefaultMicMuted();
                SetTextColor(hdcStatic, muted ? colorMuted : colorLive);
            } else {
                SetTextColor(hdcStatic, colorText);
            }
            
            return (LRESULT)hBrushBg;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == ID_RUN_STARTUP) {
                isRunOnStartup = (SendMessage(hStartupCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                ManageStartup(isRunOnStartup);
            }
            else if (wmId == ID_TRAY_OPEN) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            }
            else if (wmId == ID_TRAY_EXIT) {
                DestroyWindow(hWnd);
            }
            break;
        }

        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONUP) {
                DWORD currentTime = GetTickCount();
                if (currentTime - lastToggleTime < 500) {
                    break;
                }
                lastToggleTime = currentTime;
                skipTimerCycles = 2;
                ToggleMute();
            } else if (lParam == WM_RBUTTONUP) {
                POINT curPoint;
                GetCursorPos(&curPoint);
                HMENU hMenu = CreatePopupMenu();
                AppendMenu(hMenu, MF_STRING, ID_TRAY_OPEN, "Open Settings");
                AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, "Exit");
                SetForegroundWindow(hWnd);
                TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, curPoint.x, curPoint.y, 0, hWnd, NULL);
                DestroyMenu(hMenu);
            }
            break;
        }

        case WM_TIMER: {
            if (skipTimerCycles > 0) {
                skipTimerCycles--;
                break;
            }
            UpdateUIState();
            break;
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

void ToggleMute() {
    ToggleMuteAll();
    UpdateUIState();
}

void UpdateUIState() {
    bool muted = IsDefaultMicMuted();
    UpdateTrayIcon(muted);

    HWND hStatus = GetDlgItem(hMainWnd, ID_STATUS_LABEL);
    if (hStatus) {
        SetWindowText(hStatus, muted ? "🔇  MICROPHONE MUTED" : "🎤  MICROPHONE LIVE");
        // Force redraw to update color
        InvalidateRect(hStatus, NULL, TRUE);
    }
}

void ManageStartup(bool enable) {
    HKEY hKey;
    const char* czPath = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const char* czAppName = "MicMute-S";
    
    if (RegOpenKey(HKEY_CURRENT_USER, czPath, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            char path[MAX_PATH];
            GetModuleFileName(NULL, path, MAX_PATH);
            RegSetValueEx(hKey, czAppName, 0, REG_SZ, (BYTE*)path, strlen(path) + 1);
        } else {
            RegDeleteValue(hKey, czAppName);
        }
        RegCloseKey(hKey);
    }
}

bool IsStartupEnabled() {
    HKEY hKey;
    const char* czPath = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const char* czAppName = "MicMute-S";
    bool exists = false;
    
    if (RegOpenKey(HKEY_CURRENT_USER, czPath, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, czAppName, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            exists = true;
        }
        RegCloseKey(hKey);
    }
    return exists;
}

void AddTrayIcon(HWND hWnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MIC_ON));
    strcpy_s(nid.szTip, "MicMute-S");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void UpdateTrayIcon(bool isMuted) {
    nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(isMuted ? IDI_MIC_OFF : IDI_MIC_ON));
    strcpy_s(nid.szTip, isMuted ? "MicMute-S (Muted)" : "MicMute-S (Live)");
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}
