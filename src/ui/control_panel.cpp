#include "ui/control_panel.h"
#include "ui/player_window.h"
#include "ui/password_dialog.h"
#include "core/globals.h"
#include "core/settings.h"
#include "audio/audio.h"
#include "ui/ui.h"
#include "audio/recorder.h"
#include "audio/call_recorder.h"
#include "network/http_server.h"
#include "ui/ui_controls.h"
#include "audio/WasapiRecorder.h"
#include <dwmapi.h>
#include <cmath>
#include <cstdio>
#include <ctime>
#include "core/resource.h"
#include <iomanip>
#include <sstream>
#include <string>

#pragma comment(lib, "dwmapi.lib")

// Local button IDs
#define CPANEL_BTN_MUTE       3001
#define CPANEL_BTN_REC_START  3002
#define CPANEL_BTN_REC_STOP   3003
#define CPANEL_BTN_SETTINGS   3004
#define CPANEL_BTN_PLAYER     3005

// Animation state
static int cpAnimFrame = 0;
static bool cpDragging = false;
static POINT cpDragStart = {0, 0};
static POINT cpClickStart = {0, 0};

// Hover state
static int cpHoverItem = -1; // -1: None, 0: Mute, 1: RecStart, 2: RecStop, 3: Folder, 4: Settings, 5: MiniModeToggle

// Mini Mode
static bool cpMiniMode = false;


// Notification for saved recordings
static std::string cpLastSavedFile;
static ULONGLONG cpSavedNotifyTime = 0;
static const ULONGLONG SAVED_NOTIFY_MS = 3000;

// External functions from recorder.cpp
extern WasapiRecorder* GetManualRecorder();
extern bool IsManualRecording();
extern bool IsManualPaused();
extern void HandleManualStartPause(HWND parent);
extern void HandleManualStop(HWND parent);

extern void HandleManualStartPause(HWND parent);
extern void HandleManualStop(HWND parent);

static int GetContentWidth(int height) {
    int margin = 8;
    int w = margin;
    
    if (cpMiniMode) {
        // Mini mode: Mute Btn + Expand Toggle
        // Mute btn size matches height - margin*2
        int btnSize = height - margin * 2;
        w += btnSize + margin;
        
        // Expand toggle (small width)
        w += 20 + margin; 
        return w;
    }
    
    if (showMuteBtn) {
        int btnSize = height - margin * 2;
        w += btnSize + margin;
    }

    // Separator logic match
    if (showMuteBtn && (showVoiceMeter || (isDevModeEnabled && (showRecStatus || showManualRec || showCallStats)))) {
        w += margin;
    }

    if (showVoiceMeter) {
         w += 120 + margin;
         // Separator after meter
         if (isDevModeEnabled && (showRecStatus || showManualRec || showCallStats)) {
             w += margin;
         }
    } else if (showMuteBtn && (isDevModeEnabled && (showRecStatus || showManualRec || showCallStats))) {
        // If meter is hidden but there are items after, they need a separator if mute btn is there
        // (Handled by the first separator logic above? No, that just added margin. 
        //  The paint logic adds separator line then advances drawX.
        //  Let's stick to simple additive logic matching Paint:
        //  Paint: Mute -> Separator -> Meter -> Separator -> Rec -> ...
    }
    
    // Let's refine based on Paint exactly:
    w = margin; // Start
    
    // Mute
    if (showMuteBtn) {
         int btnSize = height - margin * 2;
         w += btnSize + margin;
         
         // Separator after mute?
         if (showVoiceMeter || (isDevModeEnabled && (showRecStatus || showManualRec || showCallStats))) {
             w += margin;
         }
    }
    
    // Meter
    if (showVoiceMeter) {
        w += 120 + margin;
        // Separator after meter?
        // Paint logic: if (showVoiceMeter) ... drawX += 120 + margin; ... Separator ... drawX += margin;
        // Wait, Paint logic has Separator inside showVoiceMeter block?
        // Let's check original Paint logic:
        // if (showVoiceMeter) { ... drawX += 120 + margin; ... Separator ... drawX += margin; }
        // YES, it has a separator at the end of its block!
        w += margin; 
    }
    
    // Rec Status
    if (isDevModeEnabled && showRecStatus) {
        w += 140 + margin;
    }
    
    // Manual Rec Buttons (Start/Pause + Stop)
    if (isDevModeEnabled && showManualRec) {
        w += 28 + 4 + 28 + margin;
    }

    // Folder Button (Visible if Manual OR Auto is enabled)
    if (isDevModeEnabled && (showManualRec || autoRecordCalls)) {
        w += 28 + margin;
    }
    
    // Separator after Rec/Folder controls
    if (isDevModeEnabled && (showManualRec || autoRecordCalls) && showCallStats) {
         w += margin;
    }
    
    // Call Stats
    if (isDevModeEnabled && showCallStats) {
        w += 100 + margin;
    }
    
    // Settings Button (Always there?)
    {
        w += margin; // Separator
        w += 28 + margin;
    }

    // Player Button (New logic: visible if autoRecordCalls is enabled)
    if (autoRecordCalls) {
        w += 28 + margin;
    }
    
    // Collapse Toggle (End)
    w += margin; 
    w += 20 + margin;
    
    return w;
}

void GetPanelDimensions(int mode, float scale, int* outW, int* outH) {
    // Mode impacts Height primarily
    int baseH;
    switch (mode) {
        case 0: baseH = 56; break;
        case 2: baseH = 72; break;
        default: baseH = 64; break;
    }
    *outH = (int)(baseH * scale);
    
    // Dynamic Width
    int contentW = GetContentWidth(*outH);
    *outW = contentW; // Already scaled essentially because height is scaled and items depend on height or fixed px
    // Wait, fixed px items (120, 140) are NOT scaled in Paint?
    // In Paint: "int meterW = 120;" -> It's fixed pixels!
    // So my GetContentWidth returns total pixels.
    // The previous implementation multiplied 600 * scale. 
    // Now we sum up pixels.
}

void SaveControlPanelPosition() {
    if (!hControlPanel) return;
    RECT rect; GetWindowRect(hControlPanel, &rect);
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\MicMute-S", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, "PanelX", 0, REG_DWORD, (BYTE*)&rect.left, sizeof(DWORD));
        RegSetValueEx(hKey, "PanelY", 0, REG_DWORD, (BYTE*)&rect.top, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

void LoadControlPanelPosition(int* x, int* y, int* w, int* h) {
    float scale = GetWindowScale(nullptr);
    GetPanelDimensions(panelSizeMode, scale, w, h);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    *x = (screenW - *w) / 2;
    *y = screenH - *h - 80;

    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\MicMute-S", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        int savedX = *x, savedY = *y;
        RegQueryValueEx(hKey, "PanelX", nullptr, nullptr, (BYTE*)&savedX, &size);
        RegQueryValueEx(hKey, "PanelY", nullptr, nullptr, (BYTE*)&savedY, &size);
        RegCloseKey(hKey);

        int virtLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int virtTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int virtRight = virtLeft + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int virtBottom = virtTop + GetSystemMetrics(SM_CYVIRTUALSCREEN);

        if (savedX >= virtLeft && savedX < virtRight - 50 &&
            savedY >= virtTop && savedY < virtBottom - 50) {
            *x = savedX;
            *y = savedY;
        }
    }
}

// Draw the circular mute toggle button
static void DrawMuteButton(HDC hdc, RECT area, bool isMuted) {
    int cx = (area.left + area.right) / 2;
    int cy = (area.top + area.bottom) / 2;
    int radius = min(area.right - area.left, area.bottom - area.top) / 2 - 4;

    // Outer glow ring
    COLORREF glowColor = isMuted ? RGB(255, 50, 50) : RGB(50, 220, 80);
    HPEN glowPen = CreatePen(PS_SOLID, 2, glowColor);
    HPEN oldPen = (HPEN)SelectObject(hdc, glowPen);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, cx - radius - 2, cy - radius - 2, cx + radius + 2, cy + radius + 2);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(glowPen);

    // Filled circle background
    COLORREF fillColor = isMuted ? RGB(180, 30, 30) : RGB(30, 160, 60);
    HBRUSH fillBr = CreateSolidBrush(fillColor);
    HPEN borderPen = CreatePen(PS_SOLID, 1, glowColor);
    SelectObject(hdc, fillBr);
    SelectObject(hdc, borderPen);
    Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
    SelectObject(hdc, GetStockObject(NULL_BRUSH)); // Restore default
    
    // Icon
    HICON hIcon = isMuted ? hIconMicOff : hIconMicOn;
    if (hIcon) {
        int iconSize = radius;
        DrawIconEx(hdc, cx - iconSize / 2, cy - iconSize / 2, hIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
    } else {
        // Fallback text
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, hFontBold);
        const char* label = isMuted ? "M" : "L";
        RECT tr = area;
        DrawText(hdc, label, -1, &tr, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }
    
    // Cleanup
    SelectObject(hdc, GetStockObject(BLACK_PEN));
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    DeleteObject(fillBr);
    DeleteObject(borderPen);

    if (cpHoverItem == 0) {
        // Tooltip
        SetTextColor(hdc, colorTextDim);
        SelectObject(hdc, hFontSmall);
        RECT rcTip = {area.left, area.bottom - 10, area.right, area.bottom + 4};
        DrawText(hdc, isMuted ? "Unmute" : "Mute", -1, &rcTip, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }
}

// Draw mini waveform inline
static void DrawMiniWaveform(HDC hdc, RECT area, float* history, int histIndex, bool isMuted, 
                              COLORREF colorLow, COLORREF colorHigh) {
    int centerY = (area.top + area.bottom) / 2;
    int height = area.bottom - area.top;
    int maxHalf = height / 2 - 3;

    // Center line
    HPEN gridPen = CreatePen(PS_DOT, 1, RGB(40, 40, 55));
    SelectObject(hdc, gridPen);
    MoveToEx(hdc, area.left, centerY, nullptr);
    LineTo(hdc, area.right, centerY);
    DeleteObject(gridPen);

    float stepX = (float)(area.right - area.left) / (float)(LEVEL_HISTORY_SIZE - 1);

    for (int i = 0; i < (int)LEVEL_HISTORY_SIZE; i++) {
        int bufIdx = (histIndex + i) % LEVEL_HISTORY_SIZE;
        float raw = history[bufIdx];

        float display = 0.0f;
        float minDb = -48.0f;
        if (raw > 0.0f) {
            float db = 20.0f * log10f(raw);
            if (db < minDb) db = minDb;
            display = (db - minDb) / (0.0f - minDb);
        }
        if (display < 0.0f) display = 0.0f;
        if (display > 1.0f) display = 1.0f;
        if (isMuted) display = 0.0f;

        int half = (int)(display * maxHalf);

        int r = (int)(GetRValue(colorLow) + (GetRValue(colorHigh) - GetRValue(colorLow)) * display);
        int g = (int)(GetGValue(colorLow) + (GetGValue(colorHigh) - GetGValue(colorLow)) * display);
        int b = (int)(GetBValue(colorLow) + (GetBValue(colorHigh) - GetBValue(colorLow)) * display);
        COLORREF col = isMuted ? RGB(35, 35, 45) : RGB(r, g, b);

        HPEN lp = CreatePen(PS_SOLID, 1, col);
        HPEN op = (HPEN)SelectObject(hdc, lp);
        int x = area.left + (int)(i * stepX);
        MoveToEx(hdc, x, centerY - half, nullptr);
        LineTo(hdc, x, centerY + half);
        SelectObject(hdc, op);
        DeleteObject(lp);
    }
}

// Get today's stats string
static std::string GetCallStatsString() {
    if (!g_CallRecorder) return "";

    auto t = std::time(nullptr);
    struct tm tm;
    localtime_s(&tm, &t);
    char dateBuf[32];
    strftime(dateBuf, sizeof(dateBuf), "%d %b", &tm);

    int count = g_CallRecorder->GetTodayCallCount();
    char buf[64];
    sprintf_s(buf, "%s: %d call%s", dateBuf, count, count == 1 ? "" : "s");
    return std::string(buf);
}

void CreateControlPanel(HINSTANCE hInstance) {
    int x, y, w, h;
    LoadControlPanelPosition(&x, &y, &w, &h);

    // Register window class
    static bool registered = false;
    if (!registered) {
        WNDCLASSEX wc = {0};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ControlPanelWndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = "MicMuteS_ControlPanel";
        RegisterClassEx(&wc);
        registered = true;
    }

    hControlPanel = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        "MicMuteS_ControlPanel", nullptr,
        WS_POPUP | WS_CLIPCHILDREN,
        x, y, w, h,
        nullptr, nullptr, hInstance, nullptr
    );

    if (hControlPanel) {
        SetLayeredWindowAttributes(hControlPanel, 0, 240, LWA_ALPHA);

        // Rounded corners (Win11)
        HMODULE hDwm = GetModuleHandle("dwmapi.dll");
        if (hDwm) {
            typedef HRESULT(WINAPI* DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
            auto pSetAttr = (DwmSetWindowAttributeFn)GetProcAddress(hDwm, "DwmSetWindowAttribute");
            if (pSetAttr) {
                int cornerPref = 2; // DWMWCP_ROUND
                pSetAttr(hControlPanel, 33, &cornerPref, sizeof(cornerPref));
            }
        }

        ShowWindow(hControlPanel, SW_SHOW);
        UpdateWindow(hControlPanel);
    }
}

void UpdateControlPanel() {
    if (hControlPanel && IsWindowVisible(hControlPanel)) {
        // Resize if content changed
        int w, h;
        RECT r; GetWindowRect(hControlPanel, &r);
        float scale = GetWindowScale(hControlPanel);
        GetPanelDimensions(panelSizeMode, scale, &w, &h);
        
        if (r.right - r.left != w || r.bottom - r.top != h) {
             // Center horizontally? Or keep left/right? 
             // Usually center.
             int currentCenterX = (r.left + r.right) / 2;
             int newLeft = currentCenterX - w / 2;
             SetWindowPos(hControlPanel, nullptr, newLeft, r.top, w, h, SWP_NOZORDER);
        }
        
        InvalidateRect(hControlPanel, nullptr, FALSE);
    }
}

void SetControlPanelSavedStatus(const std::string& filename) {
    cpLastSavedFile = filename;
    cpSavedNotifyTime = GetTickCount64();
    UpdateControlPanel();
}

LRESULT CALLBACK ControlPanelWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rect;
            GetClientRect(hWnd, &rect);

            // Double buffer
            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

            // Background
            HBRUSH bgBr = CreateSolidBrush(colorPanelBg);
            FillRect(mem, &rect, bgBr);
            DeleteObject(bgBr);

            // Border
            HPEN borderPen = CreatePen(PS_SOLID, 1, colorPanelBorder);
            SelectObject(mem, borderPen);
            SelectObject(mem, GetStockObject(NULL_BRUSH));
            RoundRect(mem, 0, 0, rect.right, rect.bottom, 16, 16);
            DeleteObject(borderPen);

            SetBkMode(mem, TRANSPARENT);

            int margin = 8;
            int drawX = margin;
            int panelH = rect.bottom;
            bool isMuted = IsDefaultMicMuted();

            // === Section 1: Mute Button ===
            if (showMuteBtn || cpMiniMode) { // Always show in mini mode (if enabled? assume yes or fallback)
                // Actually, if showMuteBtn is false, mini mode might be weird. 
                // But let's assume if they use mini mode they want the mute button.
                int btnSize = panelH - margin * 2;
                RECT muteArea = {drawX, margin, drawX + btnSize, panelH - margin};
                DrawMuteButton(mem, muteArea, isMuted);
                drawX += btnSize + margin;
            }

            // === Mini Mode Toggle Button ===
            if (cpMiniMode) {
                // Draw Expand Button
                int toggleW = 20;
                RECT rc = {drawX, 0, drawX + toggleW, panelH};
                
                if (cpHoverItem == 5) {
                     HBRUSH hHover = CreateSolidBrush(RGB(50, 50, 65));
                     FillRect(mem, &rc, hHover);
                     DeleteObject(hHover);
                }

                // Draw Arrow >
                int cx = (rc.left + rc.right) / 2;
                int cy = (rc.top + rc.bottom) / 2;
                
                HPEN arrowPen = CreatePen(PS_SOLID, 2, colorTextDim);
                SelectObject(mem, arrowPen);
                
                // > shape
                MoveToEx(mem, cx - 3, cy - 5, nullptr);
                LineTo(mem, cx + 2, cy);
                LineTo(mem, cx - 3, cy + 5);
                
                DeleteObject(arrowPen);
                
                // Blit and return early
                BitBlt(hdc, 0, 0, rect.right, rect.bottom, mem, 0, 0, SRCCOPY);
                SelectObject(mem, oldBmp);
                DeleteObject(bmp);
                DeleteDC(mem);
                EndPaint(hWnd, &ps);
                return 0;
            }

            // === Separator ===
            if (showMuteBtn && (showVoiceMeter || (isDevModeEnabled && (showRecStatus || showManualRec || showCallStats)))) {
                HPEN sepPen = CreatePen(PS_SOLID, 1, colorPanelBorder);
                SelectObject(mem, sepPen);
                MoveToEx(mem, drawX, 6, nullptr);
                LineTo(mem, drawX, panelH - 6);
                DeleteObject(sepPen);
                drawX += margin;
            }

            // === Section 2: Voice Meter ===
            if (showVoiceMeter) {
                int meterW = 120;
                int halfH = (panelH - 4) / 2;

                RECT micArea = {drawX, 2, drawX + meterW, halfH};
                RECT spkArea = {drawX, halfH + 2, drawX + meterW, panelH - 2};

                DrawMiniWaveform(mem, micArea, levelHistory.data(), levelHistoryIndex, isMuted,
                                  RGB(0, 255, 0), RGB(255, 255, 0));
                DrawMiniWaveform(mem, spkArea, speakerLevelHistory.data(), levelHistoryIndex, false,
                                  RGB(0, 200, 255), RGB(0, 100, 255));

                // Labels
                SetTextColor(mem, RGB(120, 120, 140));
                SelectObject(mem, hFontSmall);
                RECT lblMic = {drawX + 2, 1, drawX + meterW, 14};
                DrawText(mem, "MIC", -1, &lblMic, DT_LEFT | DT_TOP | DT_SINGLELINE);
                RECT lblSpk = {drawX + 2, halfH + 1, drawX + meterW, halfH + 14};
                DrawText(mem, "SPK", -1, &lblSpk, DT_LEFT | DT_TOP | DT_SINGLELINE);

                drawX += meterW + margin;

                // Separator
                HPEN sepPen2 = CreatePen(PS_SOLID, 1, colorPanelBorder);
                SelectObject(mem, sepPen2);
                MoveToEx(mem, drawX, 6, nullptr);
                LineTo(mem, drawX, panelH - 6);
                DeleteObject(sepPen2);
                drawX += margin;
            }

            // === Section 3: Recording Status ===
            if (isDevModeEnabled && showRecStatus) {
                int statusW = 140;
                RECT statusRect = {drawX, 4, drawX + statusW, panelH - 4};

                ULONGLONG now = GetTickCount64();
                bool showingSaved = (cpSavedNotifyTime > 0 && (now - cpSavedNotifyTime) < SAVED_NOTIFY_MS);

                SelectObject(mem, hFontSmall);

                if (showingSaved) {
                    SetTextColor(mem, RGB(100, 255, 120));
                    std::string msg = "Saved: " + cpLastSavedFile;
                    DrawText(mem, msg.c_str(), -1, &statusRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
                }
                else if (g_CallRecorder && g_CallRecorder->IsEnabled() &&
                         g_CallRecorder->GetState() == CallAutoRecorder::State::RECORDING) {
                    cpAnimFrame = (cpAnimFrame + 1) % 3;
                    int pulse = 180 + (cpAnimFrame * 25);
                    SetTextColor(mem, RGB(pulse, 55, 55));
                    const char* dots[] = {"●  ", " ● ", "  ●"};
                    
                    ULONGLONG ms = g_CallRecorder->GetRecordingDuration();
                    int seconds = (int)((ms / 1000) % 60);
                    int minutes = (int)((ms / 1000) / 60);
                    char timeBuf[32];
                    sprintf_s(timeBuf, "Rec %02d:%02d", minutes, seconds);
                    
                    std::string msg = std::string(dots[cpAnimFrame]) + timeBuf;
                    DrawText(mem, msg.c_str(), -1, &statusRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                }
                else if (IsManualRecording()) {
                    if (IsManualPaused()) { // Fix for manual timer if needed later
                        SetTextColor(mem, RGB(255, 200, 0));
                        DrawText(mem, "⏸ Paused", -1, &statusRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                    } else {
                        SetTextColor(mem, RGB(255, 70, 70));
                        
                        // Get manual duration
                        WasapiRecorder* pRec = GetManualRecorder();
                        int totalSec = 0; 
                        if (pRec) totalSec = (int)pRec->GetDurationSeconds();
                        
                        int minutes = totalSec / 60;
                        int seconds = totalSec % 60;
                        char timeBuf[32];
                        sprintf_s(timeBuf, "Rec %02d:%02d", minutes, seconds);
                        
                        std::string msg = "● " + std::string(timeBuf);
                        DrawText(mem, msg.c_str(), -1, &statusRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT); 
                    }
                }
                else if (g_CallRecorder && g_CallRecorder->IsEnabled()) {
                    if (IsExtensionConnected()) {
                        SetTextColor(mem, RGB(100, 180, 255));
                        DrawText(mem, "Auto: Ready", -1, &statusRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                        
                        // Green status dot
                        HBRUSH dotBr = CreateSolidBrush(RGB(50, 220, 80));
                        RECT rcDot = {statusRect.left + 70, statusRect.top + 8, statusRect.left + 76, statusRect.top + 14};
                        FillRect(mem, &rcDot, dotBr);
                        DeleteObject(dotBr);
                    } else {
                        SetTextColor(mem, RGB(255, 165, 60));
                        DrawText(mem, "Waiting Ext...", -1, &statusRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                        
                        // Red/Orange status dot
                        HBRUSH dotBr = CreateSolidBrush(RGB(255, 100, 50));
                        RECT rcDot = {statusRect.left + 80, statusRect.top + 8, statusRect.left + 86, statusRect.top + 14};
                        FillRect(mem, &rcDot, dotBr);
                        DeleteObject(dotBr);
                    }
                } else {
                    SetTextColor(mem, colorTextDim);
                    DrawText(mem, "Idle", -1, &statusRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                }

                drawX += statusW + margin;
            }

            // === Section 4: Manual Record Buttons ===
            if (isDevModeEnabled && showManualRec) {
                int btnW = 28;
                int btnH = 28;
                int bY = (panelH - btnH) / 2;

                // Start/Pause button
                {
                    RECT rc = {drawX, bY, drawX + btnW, bY + btnH};
                    bool recording = IsManualRecording();
                    bool paused = IsManualPaused();

                    HBRUSH br = CreateSolidBrush(RGB(45, 45, 60));
                    FillRect(mem, &rc, br);
                    DeleteObject(br);

                    HBRUSH shapeBr = CreateSolidBrush(RGB(240, 240, 245));
                    int cx = (rc.left + rc.right) / 2;
                    int cy = (rc.top + rc.bottom) / 2;

                    if (recording && !paused) {
                        // Pause icon
                        RECT r1 = {cx - 5, cy - 6, cx - 2, cy + 6};
                        RECT r2 = {cx + 2, cy - 6, cx + 5, cy + 6};
                        FillRect(mem, &r1, shapeBr);
                        FillRect(mem, &r2, shapeBr);
                        
                        if (cpHoverItem == 1) { // Tooltip
                            SetTextColor(mem, colorTextDim);
                            SelectObject(mem, hFontSmall);
                            RECT rcTip = {rc.left, rc.bottom, rc.right, rc.bottom + 12};
                            DrawText(mem, "Pause", -1, &rcTip, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                        }
                    } else {
                        // Play triangle
                        POINT pts[3] = {{cx - 4, cy - 6}, {cx - 4, cy + 6}, {cx + 5, cy}};
                        HRGN rgn = CreatePolygonRgn(pts, 3, WINDING);
                        FillRgn(mem, rgn, shapeBr);
                        DeleteObject(rgn);

                        if (cpHoverItem == 1) { // Tooltip
                            SetTextColor(mem, colorTextDim);
                            SelectObject(mem, hFontSmall);
                            RECT rcTip = {rc.left, rc.bottom, rc.right, rc.bottom + 12};
                            DrawText(mem, "Rec", -1, &rcTip, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                        }
                    }
                    DeleteObject(shapeBr);
                    drawX += btnW + 4;
                }

                // Stop button
                {
                    RECT rc = {drawX, bY, drawX + btnW, bY + btnH};
                    HBRUSH br = CreateSolidBrush(RGB(45, 45, 60));
                    FillRect(mem, &rc, br);
                    DeleteObject(br);

                    int cx = (rc.left + rc.right) / 2;
                    int cy = (rc.top + rc.bottom) / 2;
                    RECT sq = {cx - 5, cy - 5, cx + 5, cy + 5};
                    HBRUSH red = CreateSolidBrush(RGB(255, 70, 70));
                    FillRect(mem, &sq, red);
                    DeleteObject(red);
                    drawX += btnW + margin;
                }
            }

            // === Section 4b: Folder Button (Manual OR Auto) ===
            if (isDevModeEnabled && (showManualRec || autoRecordCalls)) {
                int btnW = 28;
                int btnH = 28;
                int bY = (panelH - btnH) / 2;

                // Folder button
                {
                    RECT rc = {drawX, bY, drawX + btnW, bY + btnH};
                    HBRUSH br = CreateSolidBrush(RGB(45, 45, 60));
                    if (cpHoverItem == 3) {
                         DeleteObject(br);
                         br = CreateSolidBrush(RGB(65, 65, 80));
                    }
                    FillRect(mem, &rc, br);
                    DeleteObject(br);

                    int cx = (rc.left + rc.right) / 2;
                    int cy = (rc.top + rc.bottom) / 2;
                    
                    // Draw simple folder shape
                    HBRUSH folderBr = CreateSolidBrush(RGB(200, 200, 220));
                    RECT rBody = {cx - 7, cy - 4, cx + 7, cy + 6};
                    RECT rTab = {cx - 7, cy - 7, cx - 2, cy - 4};
                    FillRect(mem, &rBody, folderBr);
                    FillRect(mem, &rTab, folderBr);
                    DeleteObject(folderBr);
                    
                    if (cpHoverItem == 3) { // Tooltip
                        SetTextColor(mem, colorTextDim);
                        SelectObject(mem, hFontSmall);
                        RECT rcTip = {rc.left, rc.bottom, rc.right, rc.bottom + 12};
                        DrawText(mem, "Folder", -1, &rcTip, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                    }

                    drawX += btnW + margin;
                }
            }

            // Separator check needs to allow for Folder button being present
            if (isDevModeEnabled && (showManualRec || autoRecordCalls) && showCallStats) {
                 // Gradient Separator
                for(int i=6; i<panelH-6; i++) {
                    int alpha = 255 - abs(i - panelH/2) * 5;
                    if(alpha < 0) alpha = 0;
                    SetPixel(mem, drawX, i, RGB(60, 60, 80));
                }
                drawX += margin;
            }

            // === Section 5: Call Stats ===
            if (isDevModeEnabled && showCallStats) {
                std::string stats = GetCallStatsString();
                if (!stats.empty()) {
                    // Stats badge
                    int badgeW = 100;
                    RECT badgeRect = {drawX, 4, drawX + badgeW, panelH - 4};

                    // Badge background
                    HBRUSH badgeBr = CreateSolidBrush(RGB(35, 35, 50));
                    RECT badgeBg = {drawX, (panelH - 24) / 2, drawX + badgeW, (panelH + 24) / 2};
                    FillRect(mem, &badgeBg, badgeBr);
                    DeleteObject(badgeBr);

                    SetTextColor(mem, colorAccent);
                    SelectObject(mem, hFontSmall);
                    DrawText(mem, stats.c_str(), -1, &badgeRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                }
                drawX += 100 + margin;
            }

            // === Section 6: Settings Button ===
            {
                // Separator before Settings
                HPEN sepPen = CreatePen(PS_SOLID, 1, colorPanelBorder);
                SelectObject(mem, sepPen);
                MoveToEx(mem, drawX, 6, nullptr);
                LineTo(mem, drawX, panelH - 6);
                DeleteObject(sepPen);
                drawX += margin;

                int btnW = 28;
                int btnH = 28;
                int bY = (panelH - btnH) / 2;
                RECT rc = {drawX, bY, drawX + btnW, bY + btnH};

                HBRUSH br = CreateSolidBrush(RGB(45, 45, 60));
                if (cpHoverItem == 4) {
                    DeleteObject(br);
                    br = CreateSolidBrush(RGB(65, 65, 85)); // Lighter on hover
                }
                FillRect(mem, &rc, br);
                DeleteObject(br);

                // Draw a simple gear/cog or home icon
                int cx = (rc.left + rc.right) / 2;
                int cy = (rc.top + rc.bottom) / 2;
                
                HPEN gearPen = CreatePen(PS_SOLID, 2, colorText);
                SelectObject(mem, gearPen);
                SelectObject(mem, GetStockObject(NULL_BRUSH));
                
                // Gear body
                Ellipse(mem, cx - 6, cy - 6, cx + 6, cy + 6);
                
                // Teeth
                for (int i = 0; i < 8; i++) {
                    double angle = i * 3.14159 / 4.0;
                    int x1 = cx + (int)(6 * cos(angle));
                    int y1 = cy + (int)(6 * sin(angle));
                    int x2 = cx + (int)(9 * cos(angle));
                    int y2 = cy + (int)(9 * sin(angle));
                    MoveToEx(mem, x1, y1, nullptr);
                    LineTo(mem, x2, y2);
                }
                
                SelectObject(mem, GetStockObject(BLACK_PEN));
                DeleteObject(gearPen);

                drawX += btnW + margin;
            }

             // === Section 7: Player Button (if autoRecordCalls) ===
            if (autoRecordCalls) {
                int btnW = 28;
                int bY = (panelH - btnW) / 2;
                RECT pRc = {drawX, bY, drawX + btnW, bY + btnW};
                
                HBRUSH pBr = CreateSolidBrush(RGB(45, 45, 60));
                if (cpHoverItem == 6) { // 6 for Player
                    DeleteObject(pBr);
                    pBr = CreateSolidBrush(RGB(65, 65, 85));
                }
                FillRect(mem, &pRc, pBr);
                DeleteObject(pBr);

                // Draw Play Icon (Triangle)
                int pcx = (pRc.left + pRc.right) / 2;
                int pcy = (pRc.top + pRc.bottom) / 2;
                
                HBRUSH playBr = CreateSolidBrush(colorText); // RGB(220, 220, 220)
                POINT pts[3] = {{pcx - 3, pcy - 5}, {pcx - 3, pcy + 5}, {pcx + 5, pcy}};
                HRGN rgn = CreatePolygonRgn(pts, 3, WINDING);
                FillRgn(mem, rgn, playBr);
                DeleteObject(rgn);
                DeleteObject(playBr);

                if (cpHoverItem == 6) { // Tooltip
                    SetTextColor(mem, colorTextDim);
                    SelectObject(mem, hFontSmall);
                    RECT rcTip = {pRc.left - 20, pRc.bottom, pRc.right + 20, pRc.bottom + 12};
                    DrawText(mem, "Player", -1, &rcTip, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                }

                drawX += btnW + margin;
            }

            // === Section 7: Collapse Button ===
            {
                 // Add a collapse button at the very end
                 // Separator
                 HPEN sepPen = CreatePen(PS_SOLID, 1, colorPanelBorder);
                 SelectObject(mem, sepPen);
                 MoveToEx(mem, drawX, 6, nullptr);
                 LineTo(mem, drawX, panelH - 6);
                 DeleteObject(sepPen);
                 drawX += margin;
                 
                 int toggleW = 20;
                 RECT rc = {drawX, 0, drawX + toggleW, panelH};
                 
                 if (cpHoverItem == 5) {
                     HBRUSH hHover = CreateSolidBrush(RGB(50, 50, 65));
                     FillRect(mem, &rc, hHover);
                     DeleteObject(hHover);
                 }
                 
                 // Draw Arrow <
                int cx = (rc.left + rc.right) / 2;
                int cy = (rc.top + rc.bottom) / 2;
                
                HPEN arrowPen = CreatePen(PS_SOLID, 2, colorTextDim);
                SelectObject(mem, arrowPen);
                
                // < shape
                MoveToEx(mem, cx + 2, cy - 5, nullptr);
                LineTo(mem, cx - 3, cy);
                LineTo(mem, cx + 2, cy + 5);
                
                DeleteObject(arrowPen);
            }

            // Blit
            BitBlt(hdc, 0, 0, rect.right, rect.bottom, mem, 0, 0, SRCCOPY);

            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            
            // Re-calculate layout to find what we are hovering over
            int panelH = 0;
            {
                RECT r; GetClientRect(hWnd, &r);
                panelH = r.bottom;
            }
            int margin = 8;
            int drawX = margin;
            
            int newHover = -1;

            if (cpMiniMode) {
                 int btnSize = panelH - margin * 2;
                 if (x >= drawX && x <= drawX + btnSize) newHover = 0; // Mute
                 drawX += btnSize + margin;
                 
                 if (x >= drawX && x <= drawX + 20) newHover = 5; // Expand
                 
                 if (newHover != cpHoverItem) {
                    cpHoverItem = newHover;
                    InvalidateRect(hWnd, nullptr, FALSE);
                 }
                
                if (cpDragging) {
                    POINT pt; GetCursorPos(&pt);
                    SetWindowPos(hWnd, nullptr, pt.x - cpDragStart.x, pt.y - cpDragStart.y,
                                 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                }
                return 0;
            }

            if (showMuteBtn) {
                int btnSize = panelH - margin * 2;
                if (x >= drawX && x <= drawX + btnSize && y >= margin && y <= panelH - margin) {
                    newHover = 0;
                }
                drawX += btnSize + margin;
                // Separator
                if (showVoiceMeter || (isDevModeEnabled && (showRecStatus || showManualRec || showCallStats))) {
                    drawX += margin;
                }
            }
            
            if (showVoiceMeter) drawX += 120 + margin * 2;
            if (showRecStatus && isDevModeEnabled) drawX += 140 + margin;
            
            if (showManualRec && isDevModeEnabled) {
                int btnW = 28;
                int bY = (panelH - btnW) / 2;
                
                // Start/Pause
                if (x >= drawX && x <= drawX + btnW && y >= bY && y <= bY + btnW) newHover = 1;
                drawX += btnW + 4;
                
                // Stop
                if (x >= drawX && x <= drawX + btnW && y >= bY && y <= bY + btnW) newHover = 2;
                drawX += btnW + margin;
            }

            // Folder
            if ((showManualRec || autoRecordCalls) && isDevModeEnabled) {
                int btnW = 28;
                int bY = (panelH - btnW) / 2;
                if (x >= drawX && x <= drawX + btnW && y >= bY && y <= bY + btnW) newHover = 3;
                drawX += btnW + margin;
                
                // Separator check needs to allow for Folder button being present
                if (showCallStats) {
                     drawX += margin;
                }
            }

            if (showCallStats && isDevModeEnabled) drawX += 100 + margin;
            
            // Settings
            {
                drawX += margin; // Separator
                int btnW = 28;
                int bY = (panelH - btnW) / 2;
                if (x >= drawX && x <= drawX + btnW && y >= bY && y <= bY + btnW) newHover = 4;
                drawX += btnW + margin;
            }

            // Player Button
            if (autoRecordCalls) {
                int btnW = 28;
                int bY = (panelH - btnW) / 2;
                if (x >= drawX && x <= drawX + btnW && y >= bY && y <= bY + btnW) newHover = 6;
                drawX += btnW + margin;
            }
            
            // Collapse Button
            {
                drawX += margin;
                if (x >= drawX && x <= drawX + 20) {
                    newHover = 5;
                }
            }

            if (newHover != cpHoverItem) {
                cpHoverItem = newHover;
                InvalidateRect(hWnd, nullptr, FALSE);
            }

            if (cpDragging) {
                POINT pt; GetCursorPos(&pt);
                SetWindowPos(hWnd, nullptr, pt.x - cpDragStart.x, pt.y - cpDragStart.y,
                             0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            
            // Request mouse leave tracking
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hWnd, 0};
            TrackMouseEvent(&tme);
            
            return 0;
        }

        case WM_MOUSELEAVE:
            if (cpHoverItem != -1) {
                cpHoverItem = -1;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            
            // Re-calc layout
            int panelH = 0;
            {
                 RECT r; GetClientRect(hWnd, &r);
                 panelH = r.bottom;
            }
            int margin = 8;
            int drawX = margin;
            
            if (cpMiniMode) {
                int btnSize = panelH - margin * 2;
                // Mute
                if (x >= drawX && x <= drawX + btnSize) {
                     ToggleMute();
                     return 0;
                }
                drawX += btnSize + margin;
                
                // Expand
                if (x >= drawX && x <= drawX + 20) {
                    cpMiniMode = false;
                    UpdateControlPanel();
                    SaveControlPanelPosition(); // maybe save state?
                    return 0;
                }
                
                // Dragging in mini mode
                 SetCapture(hWnd);
                 cpDragging = true;
                 cpDragStart.x = x;
                 cpDragStart.y = y;
                 RECT rect; // Declare rect here
                 GetWindowRect(hWnd, &rect);
                 cpClickStart.x = rect.left;
                 cpClickStart.y = rect.top;
                 return 0;
            }

            if (showMuteBtn) {
                int btnSize = panelH - margin * 2;
                if (x >= drawX && x <= drawX + btnSize && y >= margin && y <= panelH - margin) {
                    ToggleMute();
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                drawX += btnSize + margin;
                
                // Separator
                if (showVoiceMeter || (isDevModeEnabled && (showRecStatus || showManualRec || showCallStats))) {
                    drawX += margin;
                }
            }
            
            if (showVoiceMeter) drawX += 120 + margin * 2;
            if (showRecStatus && isDevModeEnabled) drawX += 140 + margin;
            
            if (showManualRec && isDevModeEnabled) {
                int btnW = 28;
                int bY = (panelH - btnW) / 2;
                
                if (x >= drawX && x <= drawX + btnW && y >= bY && y <= bY + btnW) {
                    HandleManualStartPause(hWnd);
                    return 0;
                }
                drawX += btnW + 4;
                
                if (x >= drawX && x <= drawX + btnW && y >= bY && y <= bY + btnW) {
                    HandleManualStop(hWnd);
                    return 0;
                }
                drawX += btnW + margin;
            }

            if ((showManualRec || autoRecordCalls) && isDevModeEnabled) {
                int btnW = 28;
                int bY = (panelH - btnW) / 2;

                if (x >= drawX && x <= drawX + btnW && y >= bY && y <= bY + btnW) {
                    extern void ChangeRecordingFolder(HWND parent);
                    ChangeRecordingFolder(hWnd);
                    return 0;
                }
                drawX += btnW + margin;
                
                // Separator check needs to allow for Folder button being present
                if (showCallStats) {
                     drawX += margin;
                }
            }
            
            if (showCallStats && isDevModeEnabled) drawX += 100 + margin;
            
            // Start Section 6: Settings
            {
                drawX += margin; // sep
                int btnW = 28;
                int bY = (panelH - btnW) / 2;
                if (x >= drawX && x <= drawX + btnW && y >= bY && y <= bY + btnW) {
                    // Open settings
                    SendMessage(hMainWnd, WM_COMMAND, ID_TRAY_OPEN, 0);
                    return 0;
                }
                drawX += btnW + margin;
            }

            // Player Button
            if (autoRecordCalls) {
                int btnW = 28;
                int bY = (panelH - btnW) / 2;
                if (x >= drawX && x <= drawX + btnW && y >= bY && y <= bY + btnW) {
                    if (IsPlayerWindowVisible()) ClosePlayerWindow();
                    else {
                        // Security: require password if dev mode is not active
                        if (!isDevModeEnabled && !PromptForPassword(hWnd)) return 0;
                        ShowPlayerWindow();
                    }
                    return 0;
                }
                drawX += btnW + margin;
            }
            
            // Section 7: Collapse
            {
                drawX += margin;
                if (x >= drawX && x <= drawX + 20) {
                    cpMiniMode = true;
                    UpdateControlPanel();
                    return 0;
                }
            }
            // Fallthrough: start dragging
            cpDragging = true;
            SetCapture(hWnd);
            GetCursorPos(&cpClickStart);
            GetCursorPos(&cpDragStart);
            {
                RECT r; GetWindowRect(hWnd, &r);
                cpDragStart.x -= r.left;
                cpDragStart.y -= r.top;
            }
            return 0;
        }

        case WM_LBUTTONUP:
            if (cpDragging) {
                cpDragging = false;
                ReleaseCapture();
                SaveControlPanelPosition();
            }
            return 0;

        case WM_RBUTTONUP:
            // Hide control panel
            ShowWindow(hWnd, SW_HIDE);
            return 0;

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}
