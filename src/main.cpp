#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <string>
#include "resource.h"
#include "audio.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")

// Global Variables
NOTIFYICONDATA nid;
HWND hMainWnd;
HWND hOverlayWnd = NULL;
HWND hMeterWnd = NULL;
bool isRunOnStartup = false;
bool showOverlay = false;
bool showMeter = false;
HFONT hFontTitle;
HFONT hFontStatus;
HFONT hFontNormal;
HFONT hFontSmall;
HFONT hFontOverlay;
DWORD lastToggleTime = 0;
int skipTimerCycles = 0;

// Cached icons to prevent memory leak
HICON hIconMicOn = NULL;
HICON hIconMicOff = NULL;

// Overlay animation
int overlayOpacity = 220;

// Level history for waveform
#define LEVEL_HISTORY_SIZE 30
float levelHistory[LEVEL_HISTORY_SIZE] = {0};
int levelHistoryIndex = 0;

// Colors
HBRUSH hBrushBg;
HBRUSH hBrushOverlayMuted;
HBRUSH hBrushOverlayLive;
HBRUSH hBrushMeterBg;
COLORREF colorBg = RGB(30, 30, 40);
COLORREF colorText = RGB(220, 220, 230);
COLORREF colorMuted = RGB(239, 68, 68);
COLORREF colorLive = RGB(34, 197, 94);
COLORREF colorOverlayBgMuted = RGB(180, 40, 40);
COLORREF colorOverlayBgLive = RGB(30, 150, 60);
COLORREF colorMeterBg = RGB(25, 25, 35);

// Dragging
bool isDragging = false;
POINT dragStart;
POINT clickStart;  // Initial click position to detect drag vs click
bool isMeterDragging = false;
POINT meterDragStart;

// Function Prototypes
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK OverlayWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK MeterWndProc(HWND, UINT, WPARAM, LPARAM);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon();
void UpdateTrayIcon(bool isMuted);
void ToggleMute();
void UpdateUIState();
void ManageStartup(bool enable);
bool IsStartupEnabled();
void CreateOverlayWindow(HINSTANCE hInstance);
void CreateMeterWindow(HINSTANCE hInstance);
void UpdateOverlay();
void UpdateMeter();
void SaveOverlayPosition();
void LoadOverlayPosition(int* x, int* y);
void SaveMeterPosition();
void LoadMeterPosition(int* x, int* y);
void SaveSettings();
void LoadSettings();

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

    // Pre-load icons to prevent memory leak from repeated LoadIcon calls
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
    DeleteObject(hFontTitle);
    DeleteObject(hFontStatus);
    DeleteObject(hFontNormal);
    DeleteObject(hFontSmall);
    DeleteObject(hFontOverlay);
    UninitializeAudio();
    CloseHandle(hMutex);
    
    return (int)msg.wParam;
}

void CreateOverlayWindow(HINSTANCE hInstance) {
    int overlayX, overlayY;
    LoadOverlayPosition(&overlayX, &overlayY);
    
    hOverlayWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        "MicMuteS_Overlay", NULL, WS_POPUP,
        overlayX, overlayY, 100, 40,
        NULL, NULL, hInstance, NULL
    );
    
    if (hOverlayWnd) {
        // Set initial transparency
        SetLayeredWindowAttributes(hOverlayWnd, 0, (BYTE)overlayOpacity, LWA_ALPHA);
        ShowWindow(hOverlayWnd, SW_SHOW);
        UpdateOverlay();
    }
}

void CreateMeterWindow(HINSTANCE hInstance) {
    int meterX, meterY;
    LoadMeterPosition(&meterX, &meterY);
    
    hMeterWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "MicMuteS_Meter", NULL, WS_POPUP,
        meterX, meterY, 180, 50,
        NULL, NULL, hInstance, NULL
    );
    
    if (hMeterWnd) {
        ShowWindow(hMeterWnd, SW_SHOW);
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
            FillRect(hdc, &rect, muted ? hBrushOverlayMuted : hBrushOverlayLive);
            
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            SelectObject(hdc, pen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, 0, 0, rect.right, rect.bottom, 10, 10);
            DeleteObject(pen);
            
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, hFontOverlay);
            DrawText(hdc, muted ? "MUTED" : "LIVE", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            EndPaint(hWnd, &ps);
            return 0;
        }
        
        case WM_LBUTTONDOWN:
            isDragging = true;
            SetCapture(hWnd);
            GetCursorPos(&clickStart);  // Store initial click position
            GetCursorPos(&dragStart);
            {
                RECT r; GetWindowRect(hWnd, &r);
                dragStart.x -= r.left;
                dragStart.y -= r.top;
            }
            return 0;
        
        case WM_MOUSEMOVE:
            if (isDragging) {
                POINT pt; GetCursorPos(&pt);
                SetWindowPos(hWnd, NULL, pt.x - dragStart.x, pt.y - dragStart.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            return 0;
        
        case WM_LBUTTONUP:
            if (isDragging) {
                isDragging = false;
                ReleaseCapture();
                SaveOverlayPosition();
                POINT pt; GetCursorPos(&pt);
                // Only toggle mute if mouse didn't move much (click, not drag)
                if (abs(pt.x - clickStart.x) < 10 && abs(pt.y - clickStart.y) < 10) {
                    ToggleMute();
                }
            }
            return 0;
        
        case WM_RBUTTONUP:
            showOverlay = false;
            SaveSettings();
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

LRESULT CALLBACK MeterWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rect;
            GetClientRect(hWnd, &rect);
            
            // Background
            FillRect(hdc, &rect, hBrushMeterBg);
            
            // Border
            HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 70));
            SelectObject(hdc, borderPen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, 0, 0, rect.right, rect.bottom, 8, 8);
            DeleteObject(borderPen);
            
            // Draw waveform bars
            int barWidth = 4;
            int gap = 2;
            int startX = 10;
            int maxHeight = rect.bottom - 16;
            int centerY = rect.bottom / 2;
            
            bool muted = IsDefaultMicMuted();
            HBRUSH barBrush = CreateSolidBrush(muted ? RGB(100, 100, 100) : colorLive);
            
            for (int i = 0; i < LEVEL_HISTORY_SIZE; i++) {
                int idx = (levelHistoryIndex - LEVEL_HISTORY_SIZE + i + LEVEL_HISTORY_SIZE) % LEVEL_HISTORY_SIZE;
                float level = levelHistory[idx];
                if (muted) level = 0;
                
                int barHeight = (int)(level * maxHeight);
                if (barHeight < 2) barHeight = 2;
                
                RECT barRect;
                barRect.left = startX + i * (barWidth + gap);
                barRect.right = barRect.left + barWidth;
                barRect.top = centerY - barHeight / 2;
                barRect.bottom = centerY + barHeight / 2;
                
                FillRect(hdc, &barRect, barBrush);
            }
            DeleteObject(barBrush);
            
            EndPaint(hWnd, &ps);
            return 0;
        }
        
        case WM_LBUTTONDOWN:
            isMeterDragging = true;
            SetCapture(hWnd);
            GetCursorPos(&meterDragStart);
            RECT r; GetWindowRect(hWnd, &r);
            meterDragStart.x -= r.left;
            meterDragStart.y -= r.top;
            return 0;
        
        case WM_MOUSEMOVE:
            if (isMeterDragging) {
                POINT pt; GetCursorPos(&pt);
                SetWindowPos(hWnd, NULL, pt.x - meterDragStart.x, pt.y - meterDragStart.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            return 0;
        
        case WM_LBUTTONUP:
            if (isMeterDragging) {
                isMeterDragging = false;
                ReleaseCapture();
                SaveMeterPosition();
            }
            return 0;
        
        case WM_RBUTTONUP:
            showMeter = false;
            SaveSettings();
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

void UpdateOverlay() {
    if (hOverlayWnd && IsWindowVisible(hOverlayWnd)) {
        InvalidateRect(hOverlayWnd, NULL, TRUE);
    }
}

void UpdateMeter() {
    if (hMeterWnd && IsWindowVisible(hMeterWnd)) {
        // Get current level
        float level = GetMicLevel();
        levelHistory[levelHistoryIndex] = level;
        levelHistoryIndex = (levelHistoryIndex + 1) % LEVEL_HISTORY_SIZE;
        
        InvalidateRect(hMeterWnd, NULL, TRUE);
    }
}

void SaveOverlayPosition() {
    if (!hOverlayWnd) return;
    RECT rect; GetWindowRect(hOverlayWnd, &rect);
    HKEY hKey;
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, "OverlayX", 0, REG_DWORD, (BYTE*)&rect.left, sizeof(DWORD));
        RegSetValueEx(hKey, "OverlayY", 0, REG_DWORD, (BYTE*)&rect.top, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

void LoadOverlayPosition(int* x, int* y) {
    *x = 50; *y = 50;
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        RegQueryValueEx(hKey, "OverlayX", NULL, NULL, (BYTE*)x, &size);
        RegQueryValueEx(hKey, "OverlayY", NULL, NULL, (BYTE*)y, &size);
        RegCloseKey(hKey);
    }
}

void SaveMeterPosition() {
    if (!hMeterWnd) return;
    RECT rect; GetWindowRect(hMeterWnd, &rect);
    HKEY hKey;
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, "MeterX", 0, REG_DWORD, (BYTE*)&rect.left, sizeof(DWORD));
        RegSetValueEx(hKey, "MeterY", 0, REG_DWORD, (BYTE*)&rect.top, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

void LoadMeterPosition(int* x, int* y) {
    *x = 50; *y = 100;
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        RegQueryValueEx(hKey, "MeterX", NULL, NULL, (BYTE*)x, &size);
        RegQueryValueEx(hKey, "MeterY", NULL, NULL, (BYTE*)y, &size);
        RegCloseKey(hKey);
    }
}

void SaveSettings() {
    HKEY hKey;
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD val = showOverlay ? 1 : 0;
        RegSetValueEx(hKey, "ShowOverlay", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        val = showMeter ? 1 : 0;
        RegSetValueEx(hKey, "ShowMeter", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

void LoadSettings() {
    isRunOnStartup = IsStartupEnabled();
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD), val = 0;
        if (RegQueryValueEx(hKey, "ShowOverlay", NULL, NULL, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showOverlay = val != 0;
        if (RegQueryValueEx(hKey, "ShowMeter", NULL, NULL, (BYTE*)&val, &size) == ERROR_SUCCESS)
            showMeter = val != 0;
        RegCloseKey(hKey);
    }
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

void ToggleMute() {
    // Fade out animation
    if (hOverlayWnd && IsWindowVisible(hOverlayWnd)) {
        for (int i = overlayOpacity; i >= 100; i -= 30) {
            SetLayeredWindowAttributes(hOverlayWnd, 0, (BYTE)i, LWA_ALPHA);
            Sleep(15);
        }
    }
    
    ToggleMuteAll();
    UpdateUIState();
    
    // Fade in animation
    if (hOverlayWnd && IsWindowVisible(hOverlayWnd)) {
        for (int i = 100; i <= overlayOpacity; i += 30) {
            SetLayeredWindowAttributes(hOverlayWnd, 0, (BYTE)i, LWA_ALPHA);
            Sleep(15);
        }
        SetLayeredWindowAttributes(hOverlayWnd, 0, (BYTE)overlayOpacity, LWA_ALPHA);
    }
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
    const char* path = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKey(HKEY_CURRENT_USER, path, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            char exe[MAX_PATH];
            GetModuleFileName(NULL, exe, MAX_PATH);
            RegSetValueEx(hKey, "MicMute-S", 0, REG_SZ, (BYTE*)exe, strlen(exe) + 1);
        } else {
            RegDeleteValue(hKey, "MicMute-S");
        }
        RegCloseKey(hKey);
    }
}

bool IsStartupEnabled() {
    HKEY hKey;
    bool exists = false;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, "MicMute-S", NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
            exists = true;
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
    nid.hIcon = hIconMicOn;  // Use cached icon
    strcpy_s(nid.szTip, "MicMute-S");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void UpdateTrayIcon(bool isMuted) {
    nid.hIcon = isMuted ? hIconMicOff : hIconMicOn;  // Use cached icons
    strcpy_s(nid.szTip, isMuted ? "MicMute-S (Muted)" : "MicMute-S (Live)");
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}
