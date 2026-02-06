#include "overlay.h"
#include "globals.h"
#include "settings.h"
#include "audio.h"
#include "ui.h"
#include <dwmapi.h>
#include <cmath> // for abs, log10f

// Animation state
static float animProgress = 0.0f; // 0.0 = Live, 1.0 = Muted
static float animTarget = 0.0f;
static bool isAnimating = false;

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
        // Init animation state
        bool muted = IsDefaultMicMuted();
        animTarget = muted ? 1.0f : 0.0f;
        animProgress = animTarget;

        // Set initial transparency
        SetLayeredWindowAttributes(hOverlayWnd, colorChroma, (BYTE)overlayOpacity, LWA_ALPHA | LWA_COLORKEY);
        ShowWindow(hOverlayWnd, SW_SHOW);
        UpdateOverlay();
    }
}

void CreateMeterWindow(HINSTANCE hInstance) {
    int meterX, meterY;
    LoadMeterPosition(&meterX, &meterY);
    
    // Explicitly zero-out history to prevent visual glitches on startup
    memset(levelHistory, 0, sizeof(levelHistory));
    memset(speakerLevelHistory, 0, sizeof(speakerLevelHistory));
    levelHistoryIndex = 0;
    
    // Increased height to 100 to accommodate dual meters (Advisor + CX)
    hMeterWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "MicMuteS_Meter", NULL, WS_POPUP,
        meterX, meterY, 180, 100, 
        NULL, NULL, hInstance, NULL
    );
    
    if (hMeterWnd) {
        ShowWindow(hMeterWnd, SW_SHOW);
    }
}

// Helper to interpolate colors
COLORREF InterpColor(COLORREF c1, COLORREF c2, float t) {
    int r = (int)(GetRValue(c1) + (GetRValue(c2) - GetRValue(c1)) * t);
    int g = (int)(GetGValue(c1) + (GetGValue(c2) - GetGValue(c1)) * t);
    int b = (int)(GetBValue(c1) + (GetBValue(c2) - GetBValue(c1)) * t);
    return RGB(r, g, b);
}

void UpdateOverlay() {
    if (hOverlayWnd && IsWindowVisible(hOverlayWnd)) {
        // Check state once when requested
        bool muted = IsDefaultMicMuted();
        animTarget = muted ? 1.0f : 0.0f;
        
        // Start animation timer
        SetTimer(hOverlayWnd, 3, 16, NULL); // ~60 FPS
    }
}

void UpdateMeter() {
    if (hMeterWnd && IsWindowVisible(hMeterWnd)) {
        // Get current levels
        float micLevel = GetMicLevel();
        float spkLevel = GetSpeakerLevel();
        
        levelHistory[levelHistoryIndex] = micLevel;
        speakerLevelHistory[levelHistoryIndex] = spkLevel;
        
        levelHistoryIndex = (levelHistoryIndex + 1) % LEVEL_HISTORY_SIZE;
        
        InvalidateRect(hMeterWnd, NULL, TRUE);
    }
}

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TIMER:
            if (wParam == 3) {
                // Animation logic
                float speed = 0.15f; // Adjust for speed
                if (animProgress < animTarget) {
                    animProgress += speed;
                    if (animProgress > animTarget) animProgress = animTarget;
                } else if (animProgress > animTarget) {
                    animProgress -= speed;
                    if (animProgress < animTarget) animProgress = animTarget;
                }
                
                InvalidateRect(hWnd, NULL, TRUE);
                
                if (abs(animProgress - animTarget) < 0.001f) {
                    KillTimer(hWnd, 3);
                }
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rect;
            GetClientRect(hWnd, &rect);
            
            // Setup
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, hFontOverlay);
            
            // Interpolate color
            COLORREF bgCol = InterpColor(colorOverlayBgLive, colorOverlayBgMuted, animProgress);
            HBRUSH bgBrush = CreateSolidBrush(bgCol);
            
            SelectObject(hdc, bgBrush);
            SelectObject(hdc, GetStockObject(NULL_PEN)); 
            RoundRect(hdc, 0, 0, rect.right, rect.bottom, 40, 40); // Full rounded corners
            
            DeleteObject(bgBrush);
            
            // Draw White Border
            HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            SelectObject(hdc, borderPen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, 1, 1, rect.right-1, rect.bottom-1, 40, 40);
            DeleteObject(borderPen);
            
            // Determine icon based on progress (snap at 50%)
            bool showMuted = (animProgress > 0.5f);
            HICON hIcon = showMuted ? hIconMicOff : hIconMicOn;
            DrawIconEx(hdc, 8, 10, hIcon, 20, 20, 0, NULL, DI_NORMAL);
            
            // Draw Text
            RECT textRect = rect;
            textRect.left += 32; // Shift text right
            DrawText(hdc, showMuted ? "MUTED" : "LIVE", -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            
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

// Helper to draw a single channel waveform
void DrawWaveform(HDC hdc, RECT rect, float* history, int historyIndex, bool isMuted, COLORREF colorLow, COLORREF colorHigh) {
    int centerY = (rect.bottom + rect.top) / 2;
    int maxHalfHeight = (rect.bottom - rect.top) / 2 - 2;
    
    float stepX = (float)(rect.right - rect.left - 4) / (float)(LEVEL_HISTORY_SIZE - 1);
    int startX = rect.left + 2;

    for (int i = 0; i < LEVEL_HISTORY_SIZE; i++) {
        int bufferIdx = (historyIndex + i) % LEVEL_HISTORY_SIZE;
        float rawLevel = history[bufferIdx];
        
        // Logarithmic Scaling
        float displayLevel = 0.0f;
        if (rawLevel > 0.000001f) {
            float db = 20.0f * log10f(rawLevel);
            // Map -60dB -> 0.0, 0dB -> 1.0
            displayLevel = (db + 60.0f) / 60.0f;
        }
        if (displayLevel < 0.0f) displayLevel = 0.0f;
        if (displayLevel > 1.0f) displayLevel = 1.0f;
        
        if (isMuted) displayLevel = 0.0f;

        int halfHeight = (int)(displayLevel * maxHalfHeight);
        if (halfHeight < 1) halfHeight = 1;

        // Color Logic
        int r, g, b;
        if (displayLevel < 0.5f) {
            // Low -> Mid
            r = (int)(GetRValue(colorLow) + (GetRValue(colorHigh) - GetRValue(colorLow)) * displayLevel * 2.0f);
            g = (int)(GetGValue(colorLow) + (GetGValue(colorHigh) - GetGValue(colorLow)) * displayLevel * 2.0f);
            b = (int)(GetBValue(colorLow) + (GetBValue(colorHigh) - GetBValue(colorLow)) * displayLevel * 2.0f);
        } else {
            // Mid -> High (Red)
            r = (int)(GetRValue(colorHigh) + (255 - GetRValue(colorHigh)) * (displayLevel - 0.5f) * 2.0f);
            g = (int)(GetGValue(colorHigh) + (0 - GetGValue(colorHigh)) * (displayLevel - 0.5f) * 2.0f);
            b = (int)(GetBValue(colorHigh) + (0 - GetBValue(colorHigh)) * (displayLevel - 0.5f) * 2.0f);
        }

        COLORREF col = RGB(r, g, b);
        if (isMuted) col = RGB(50, 50, 50);

        HPEN linePen = CreatePen(PS_SOLID, 1, col);
        SelectObject(hdc, linePen);
        
        int x = startX + (int)(i * stepX);
        MoveToEx(hdc, x, centerY - halfHeight, NULL);
        LineTo(hdc, x, centerY + halfHeight);
        
        DeleteObject(linePen);
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
            
            // Split area into top (Advisor) and bottom (CX)
            int halfHeight = rect.bottom / 2;
            
            RECT rectAdvisor = { 0, 0, rect.right, halfHeight };
            RECT rectCX = { 0, halfHeight, rect.right, rect.bottom };
            
            // Draw Labels
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(180, 180, 180));
            SelectObject(hdc, hFontSmall);
            
            RECT labelAdvisor = rectAdvisor; 
            labelAdvisor.left += 6; labelAdvisor.top += 2;
            DrawText(hdc, "Advisor", -1, &labelAdvisor, DT_LEFT | DT_TOP | DT_SINGLELINE);
            
            RECT labelCX = rectCX; 
            labelCX.left += 6; labelCX.top += 2;
            DrawText(hdc, "CX", -1, &labelCX, DT_LEFT | DT_TOP | DT_SINGLELINE);

            // Draw Waveforms
            // Advisor (Mic) - uses standard Green/Yellow colors (Live)
            DrawWaveform(hdc, rectAdvisor, levelHistory, levelHistoryIndex, IsDefaultMicMuted(), RGB(0, 255, 0), RGB(255, 255, 0));
            
            // CX (Speaker) - uses Blue/Cyan scheme for distinction
            DrawWaveform(hdc, rectCX, speakerLevelHistory, levelHistoryIndex, false, RGB(0, 200, 255), RGB(0, 100, 255));
            
            // Divider Line
            HPEN divPen = CreatePen(PS_SOLID, 1, RGB(50, 50, 60));
            SelectObject(hdc, divPen);
            MoveToEx(hdc, 4, halfHeight, NULL);
            LineTo(hdc, rect.right - 4, halfHeight);
            DeleteObject(divPen);

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
