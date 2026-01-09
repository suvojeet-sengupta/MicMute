#include <windows.h>
#include <shellapi.h>
#include <string>
#include "resource.h"
#include "audio.h"

#pragma comment(lib, "gdi32.lib")

// Global Variables
NOTIFYICONDATA nid;
HWND hMainWnd;
HWND hOverlayWnd = NULL;
bool isRunOnStartup = false;
bool showOverlay = false;
HFONT hFontTitle;
HFONT hFontStatus;
HFONT hFontNormal;
HFONT hFontSmall;
HFONT hFontOverlay;
DWORD lastToggleTime = 0;
int skipTimerCycles = 0;

// Colors
HBRUSH hBrushBg;
HBRUSH hBrushOverlayMuted;
HBRUSH hBrushOverlayLive;
COLORREF colorBg = RGB(30, 30, 40);
COLORREF colorText = RGB(220, 220, 230);
COLORREF colorMuted = RGB(239, 68, 68);
COLORREF colorLive = RGB(34, 197, 94);
COLORREF colorOverlayBgMuted = RGB(180, 40, 40);
COLORREF colorOverlayBgLive = RGB(30, 150, 60);

// Overlay dragging
bool isDragging = false;
POINT dragStart;

// Function Prototypes
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK OverlayWndProc(HWND, UINT, WPARAM, LPARAM);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon();
void UpdateTrayIcon(bool isMuted);
void ToggleMute();
void UpdateUIState();
void ManageStartup(bool enable);
bool IsStartupEnabled();
void CreateOverlayWindow(HINSTANCE hInstance);
void UpdateOverlay();
void SaveOverlayPosition();
void LoadOverlayPosition(int* x, int* y);
void SaveOverlayEnabled(bool enabled);
bool LoadOverlayEnabled();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    InitializeAudio();

    // Create brushes
    hBrushBg = CreateSolidBrush(colorBg);
    hBrushOverlayMuted = CreateSolidBrush(colorOverlayBgMuted);
    hBrushOverlayLive = CreateSolidBrush(colorOverlayBgLive);

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
    wcOverlay.hbrBackground = hBrushOverlayLive;
    wcOverlay.lpszClassName = "MicMuteS_Overlay";
    RegisterClassEx(&wcOverlay);

    // Window size and position
    int width = 450;
    int height = 380;
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

    // Check startup and overlay settings
    isRunOnStartup = IsStartupEnabled();
    showOverlay = LoadOverlayEnabled();
    
    // First run dialog
    if (!isRunOnStartup) {
        int result = MessageBox(hMainWnd, 
            "Would you like MicMute-S to start automatically with Windows?\n\nThis is recommended for seamless microphone control.",
            "Enable Startup?",
            MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            ManageStartup(true);
            isRunOnStartup = true;
        }
        
        // Also ask about overlay
        result = MessageBox(hMainWnd,
            "Would you like to show a floating mute button on screen?\n\nThis small draggable button stays visible for quick mute/unmute access.",
            "Enable Floating Button?",
            MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            showOverlay = true;
            SaveOverlayEnabled(true);
        }
    }

    // Create overlay if enabled
    if (showOverlay) {
        CreateOverlayWindow(hInstance);
    }

    AddTrayIcon(hMainWnd);
    UpdateUIState();

    // Timer for UI updates
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
    DeleteObject(hBrushOverlayMuted);
    DeleteObject(hBrushOverlayLive);
    DeleteObject(hFontTitle);
    DeleteObject(hFontStatus);
    DeleteObject(hFontNormal);
    DeleteObject(hFontSmall);
    DeleteObject(hFontOverlay);
    UninitializeAudio();
    
    return (int)msg.wParam;
}

void CreateOverlayWindow(HINSTANCE hInstance) {
    int overlayX, overlayY;
    LoadOverlayPosition(&overlayX, &overlayY);
    
    // Create topmost, borderless overlay window
    hOverlayWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "MicMuteS_Overlay",
        NULL,
        WS_POPUP,
        overlayX, overlayY, 100, 40,
        NULL, NULL, hInstance, NULL
    );
    
    if (hOverlayWnd) {
        ShowWindow(hOverlayWnd, SW_SHOW);
        UpdateOverlay();
    }
}

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            RECT rect;
            GetClientRect(hWnd, &rect);
            
            bool muted = IsDefaultMicMuted();
            
            // Fill background with color based on state
            HBRUSH brush = muted ? hBrushOverlayMuted : hBrushOverlayLive;
            FillRect(hdc, &rect, brush);
            
            // Draw rounded border
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, 0, 0, rect.right, rect.bottom, 10, 10);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(pen);
            
            // Draw text
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, hFontOverlay);
            
            const char* text = muted ? "MUTED" : "LIVE";
            DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            EndPaint(hWnd, &ps);
            return 0;
        }
        
        case WM_LBUTTONDOWN: {
            // Start dragging
            isDragging = true;
            SetCapture(hWnd);
            GetCursorPos(&dragStart);
            RECT rect;
            GetWindowRect(hWnd, &rect);
            dragStart.x -= rect.left;
            dragStart.y -= rect.top;
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            if (isDragging) {
                POINT pt;
                GetCursorPos(&pt);
                SetWindowPos(hWnd, NULL, pt.x - dragStart.x, pt.y - dragStart.y, 0, 0, 
                    SWP_NOSIZE | SWP_NOZORDER);
            }
            return 0;
        }
        
        case WM_LBUTTONUP: {
            if (isDragging) {
                isDragging = false;
                ReleaseCapture();
                SaveOverlayPosition();
                
                // Check if it was a click (not a drag)
                POINT pt;
                GetCursorPos(&pt);
                RECT rect;
                GetWindowRect(hWnd, &rect);
                int dx = abs((pt.x - rect.left) - dragStart.x);
                int dy = abs((pt.y - rect.top) - dragStart.y);
                
                // If moved less than 5 pixels, treat as click
                if (dx < 5 && dy < 5) {
                    ToggleMute();
                }
            }
            return 0;
        }
        
        case WM_RBUTTONUP: {
            // Right-click to hide overlay
            showOverlay = false;
            SaveOverlayEnabled(false);
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        }
        
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

void UpdateOverlay() {
    if (hOverlayWnd && IsWindowVisible(hOverlayWnd)) {
        InvalidateRect(hOverlayWnd, NULL, TRUE);
    }
}

void SaveOverlayPosition() {
    if (!hOverlayWnd) return;
    
    RECT rect;
    GetWindowRect(hOverlayWnd, &rect);
    
    HKEY hKey;
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, "OverlayX", 0, REG_DWORD, (BYTE*)&rect.left, sizeof(DWORD));
        RegSetValueEx(hKey, "OverlayY", 0, REG_DWORD, (BYTE*)&rect.top, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

void LoadOverlayPosition(int* x, int* y) {
    *x = 50;  // Default position
    *y = 50;
    
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        RegQueryValueEx(hKey, "OverlayX", NULL, NULL, (BYTE*)x, &size);
        RegQueryValueEx(hKey, "OverlayY", NULL, NULL, (BYTE*)y, &size);
        RegCloseKey(hKey);
    }
}

void SaveOverlayEnabled(bool enabled) {
    HKEY hKey;
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD value = enabled ? 1 : 0;
        RegSetValueEx(hKey, "OverlayEnabled", 0, REG_DWORD, (BYTE*)&value, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

bool LoadOverlayEnabled() {
    HKEY hKey;
    DWORD value = 0;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        RegQueryValueEx(hKey, "OverlayEnabled", NULL, NULL, (BYTE*)&value, &size);
        RegCloseKey(hKey);
    }
    return value != 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hStartupCheck;
    static HWND hOverlayCheck;

    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
            
            // App Title
            HWND hTitle = CreateWindow("STATIC", "MicMute-S", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 25, 450, 50, hWnd, NULL, hInst, NULL);
            SendMessage(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            // Status indicator
            HWND hStatus = CreateWindow("STATIC", "Checking...", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 80, 450, 35, hWnd, (HMENU)ID_STATUS_LABEL, hInst, NULL);
            SendMessage(hStatus, WM_SETFONT, (WPARAM)hFontStatus, TRUE);

            // Divider line
            CreateWindow("STATIC", "", 
                WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ, 
                50, 130, 350, 2, hWnd, NULL, hInst, NULL);

            // Startup checkbox
            hStartupCheck = CreateWindow("BUTTON", "  Launch on Windows startup", 
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 
                80, 150, 300, 28, hWnd, (HMENU)ID_RUN_STARTUP, hInst, NULL);
            SendMessage(hStartupCheck, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessage(hStartupCheck, BM_SETCHECK, isRunOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);

            // Overlay checkbox
            hOverlayCheck = CreateWindow("BUTTON", "  Show floating mute button", 
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 
                80, 185, 300, 28, hWnd, (HMENU)ID_SHOW_OVERLAY, hInst, NULL);
            SendMessage(hOverlayCheck, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SendMessage(hOverlayCheck, BM_SETCHECK, showOverlay ? BST_CHECKED : BST_UNCHECKED, 0);

            // Instructions
            HWND hInstr1 = CreateWindow("STATIC", "Click tray icon or floating button to toggle", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 235, 450, 20, hWnd, NULL, hInst, NULL);
            SendMessage(hInstr1, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

            HWND hInstr2 = CreateWindow("STATIC", "Drag floating button to move  |  Right-click to hide", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 255, 450, 20, hWnd, NULL, hInst, NULL);
            SendMessage(hInstr2, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

            HWND hInstr3 = CreateWindow("STATIC", "Minimize to hide  |  Right-click tray for menu", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 275, 450, 20, hWnd, NULL, hInst, NULL);
            SendMessage(hInstr3, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

            // Author
            HWND hAuthor = CreateWindow("STATIC", "by Suvojeet Sengupta", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 310, 450, 20, hWnd, NULL, hInst, NULL);
            SendMessage(hAuthor, WM_SETFONT, (WPARAM)hFontSmall, TRUE);
            
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            
            SetBkColor(hdcStatic, colorBg);
            
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
            else if (wmId == ID_SHOW_OVERLAY) {
                showOverlay = (SendMessage(hOverlayCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SaveOverlayEnabled(showOverlay);
                
                if (showOverlay) {
                    if (!hOverlayWnd) {
                        CreateOverlayWindow(GetModuleHandle(NULL));
                    } else {
                        ShowWindow(hOverlayWnd, SW_SHOW);
                    }
                    UpdateOverlay();
                } else if (hOverlayWnd) {
                    ShowWindow(hOverlayWnd, SW_HIDE);
                }
            }
            else if (wmId == ID_TRAY_OPEN) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            }
            else if (wmId == ID_TRAY_EXIT) {
                if (hOverlayWnd) {
                    DestroyWindow(hOverlayWnd);
                }
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
    UpdateOverlay();

    HWND hStatus = GetDlgItem(hMainWnd, ID_STATUS_LABEL);
    if (hStatus) {
        SetWindowText(hStatus, muted ? "MICROPHONE MUTED" : "MICROPHONE LIVE");
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
