#include "overlay.h"
#include "globals.h"
#include "settings.h"
#include "audio.h"
#include "ui.h"
#include <dwmapi.h>
#include <cmath> // for abs, log10f
#include <cstdio>
#include <string>

// Animation state
static float animProgress = 0.0f; // 0.0 = Live, 1.0 = Muted
static float animTarget = 0.0f;
static bool isAnimating = false;

void CreateOverlayWindow(HINSTANCE hInstance) {
    int overlayX, overlayY;
    LoadOverlayPosition(&overlayX, &overlayY);
    
    hOverlayWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        "MicMuteS_Overlay", nullptr, WS_POPUP,
        overlayX, overlayY, 100, 40,
        nullptr, nullptr, hInstance, nullptr
    );
    
    if (hOverlayWnd) {
        // Init animation state
        bool muted = IsDefaultMicMuted();
        animTarget = muted ? 1.0f : 0.0f;
        animProgress = animTarget;

        // Set initial transparency
        SetLayeredWindowAttributes(hOverlayWnd, colorChroma, (BYTE)overlayOpacity, LWA_ALPHA | LWA_COLORKEY);
        
        // Initial resize based on DPI
        float scale = GetWindowScale(hOverlayWnd);
        SetWindowPos(hOverlayWnd, nullptr, 0, 0, (int)(100 * scale), (int)(40 * scale), SWP_NOMOVE | SWP_NOZORDER);

        ShowWindow(hOverlayWnd, SW_SHOW);
        UpdateOverlay();
    }
}

void CreateMeterWindow(HINSTANCE hInstance) {
    int meterX, meterY, meterW, meterH;
    LoadMeterPosition(&meterX, &meterY, &meterW, &meterH);
    
    // Explicitly zero-out history to prevent visual glitches on startup
    levelHistory.fill(0.0f);
    speakerLevelHistory.fill(0.0f);
    levelHistoryIndex = 0;
    
    // Scale factor
    float scale = GetWindowScale(nullptr);
    if (meterW < 100) meterW = (int)(180 * scale);
    if (meterH < 50) meterH = (int)(100 * scale);
    
    // Use WS_THICKFRAME for resizing support
    hMeterWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "MicMuteS_Meter", nullptr, 
        WS_POPUP | WS_THICKFRAME,
        meterX, meterY, meterW, meterH, 
        nullptr, nullptr, hInstance, nullptr
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
        SetTimer(hOverlayWnd, 3, 16, nullptr); // ~60 FPS
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
        
        // Invalidate without erasing background to prevent flicker
        InvalidateRect(hMeterWnd, nullptr, FALSE);
    }
}

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DPICHANGED: {
             // Resize logic when moved to another monitor
             RECT* const prcNewWindow = (RECT*)lParam;
             SetWindowPos(hWnd,
                 nullptr,
                 prcNewWindow->left,
                 prcNewWindow->top,
                 prcNewWindow->right - prcNewWindow->left,
                 prcNewWindow->bottom - prcNewWindow->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
             return 0;
        }
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
                
                InvalidateRect(hWnd, nullptr, TRUE);
                
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
            float scale = GetWindowScale(hWnd);
            
            // Setup
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, hFontOverlay);
            
            // Interpolate color
            COLORREF bgCol = InterpColor(colorOverlayBgLive, colorOverlayBgMuted, animProgress);
            HBRUSH bgBrush = CreateSolidBrush(bgCol);
            
            // Create "Pill" shape (Capsule)
            // Windows 11 style: Fully rounded ends
            int cornerRadius = rect.bottom; // Full height for pill shape
            if (cornerRadius > rect.right) cornerRadius = rect.right;

            SelectObject(hdc, bgBrush);
            SelectObject(hdc, GetStockObject(NULL_PEN)); 
            
            RoundRect(hdc, 0, 0, rect.right, rect.bottom, cornerRadius, cornerRadius);
            
            DeleteObject(bgBrush);
            
            // Draw Border (Subtle)
            HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255)); // Thin white border
            SelectObject(hdc, borderPen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, 0, 0, rect.right-1, rect.bottom-1, cornerRadius, cornerRadius);
            DeleteObject(borderPen);
            
            // Determine icon based on progress
            bool showMuted = (animProgress > 0.5f);
            HICON hIcon = showMuted ? hIconMicOff : hIconMicOn;
            
            // Layout (Center Icon + Text)
            // Icon ~ 20px, Text ~ 50px?
            int iconSize = (int)(20 * scale);
            int padding = (int)(10 * scale);
            
            // Calc text width
            const char* text = showMuted ? "MUTED" : "LIVE";
            SIZE textSize;
            GetTextExtentPoint32(hdc, text, (int)strlen(text), &textSize);
            
            int contentWidth = iconSize + padding/2 + textSize.cx;
            int startX = (rect.right - contentWidth) / 2;
            
            // Draw Icon
            int iconY = (rect.bottom - iconSize) / 2;
            DrawIconEx(hdc, startX, iconY, hIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
            
            // Draw Text
            RECT textRect = {startX + iconSize + padding/2, 0, rect.right, rect.bottom};
            DrawText(hdc, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            
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
                SetWindowPos(hWnd, nullptr, pt.x - dragStart.x, pt.y - dragStart.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
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
    int height = rect.bottom - rect.top;
    int maxHalfHeight = height / 2 - 8; // Added more vertical padding (4px on each side)
    
    // Draw Grid (dB lines)
    HPEN gridPen = CreatePen(PS_DOT, 1, RGB(40, 40, 50));
    SelectObject(hdc, gridPen);
    MoveToEx(hdc, rect.left, centerY, nullptr);
    LineTo(hdc, rect.right, centerY);
    DeleteObject(gridPen);

    float stepX = (float)(rect.right - rect.left - 4) / (float)(LEVEL_HISTORY_SIZE - 1);
    int startX = rect.left + 2;

    for (int i = 0; i < LEVEL_HISTORY_SIZE; i++) {
        int bufferIdx = (historyIndex + i) % LEVEL_HISTORY_SIZE;
        float rawLevel = history[bufferIdx];
        
        // Target: Show smallest sounds but avoid noise floor.
        float displayLevel = 0.0f;
        float minDb = -48.0f; // Significant noise floor reduction
        
        if (rawLevel > 0.0f) {
            float db = 20.0f * log10f(rawLevel);
            if (db < minDb) db = minDb;
            
            // Normalize 0..1
            float normalized = (db - minDb) / (0.0f - minDb);
            
            // Linear scaling to prevent boosting noise
            displayLevel = normalized; 
        }
        
        if (displayLevel < 0.0f) displayLevel = 0.0f;
        if (displayLevel > 1.0f) displayLevel = 1.0f;
        
        if (isMuted) displayLevel = 0.0f;

        int halfHeight = (int)(displayLevel * maxHalfHeight);
        if (halfHeight < 1) halfHeight = 0; // Allow 0 for silence

        // Color Logic with smoother gradient
        int r, g, b;
        // Interpolate between Low and High based on displayLevel
        r = (int)(GetRValue(colorLow) + (GetRValue(colorHigh) - GetRValue(colorLow)) * displayLevel);
        g = (int)(GetGValue(colorLow) + (GetGValue(colorHigh) - GetGValue(colorLow)) * displayLevel);
        b = (int)(GetBValue(colorLow) + (GetBValue(colorHigh) - GetBValue(colorLow)) * displayLevel);

        COLORREF col = RGB(r, g, b);
        if (isMuted) col = RGB(40, 40, 40);

        // Use a rectangle/bar instead of a line for "Solid" look? 
        // Or thick lines. History view usually implies lines.
        // Let's stick to lines but maybe cleaner.
        HPEN linePen = CreatePen(PS_SOLID, 1, col); // Width 2 for better visibility?
        HPEN oldPen = (HPEN)SelectObject(hdc, linePen);
        
        int x = startX + (int)(i * stepX);
        
        // Draw mirrored bar around center
        MoveToEx(hdc, x, centerY - halfHeight, nullptr);
        LineTo(hdc, x, centerY + halfHeight);
        
        SelectObject(hdc, oldPen);
        DeleteObject(linePen);
    }
}

LRESULT CALLBACK MeterWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);
            if (hit == HTCLIENT) {
                // Resize border implementation for borderless window
                POINT pt; GetCursorPos(&pt);
                RECT rc; GetWindowRect(hWnd, &rc);
                int border = 8;
                if (pt.x < rc.left + border && pt.y < rc.top + border) return HTTOPLEFT;
                if (pt.x > rc.right - border && pt.y < rc.top + border) return HTTOPRIGHT;
                if (pt.x < rc.left + border && pt.y > rc.bottom - border) return HTBOTTOMLEFT;
                if (pt.x > rc.right - border && pt.y > rc.bottom - border) return HTBOTTOMRIGHT;
                if (pt.x < rc.left + border) return HTLEFT;
                if (pt.x > rc.right - border) return HTRIGHT;
                if (pt.y < rc.top + border) return HTTOP;
                if (pt.y > rc.bottom - border) return HTBOTTOM;
                return HTCAPTION; // Allow drag
            }
            return hit;
        }

        case WM_EXITSIZEMOVE:
            SaveMeterPosition();
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rect;
            GetClientRect(hWnd, &rect);
            
            // Double Buffering
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

            // Background
            FillRect(hdcMem, &rect, hBrushMeterBg);
            
            // Border
            HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 70));
            SelectObject(hdcMem, borderPen);
            SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
            RoundRect(hdcMem, 0, 0, rect.right, rect.bottom, 8, 8);
            DeleteObject(borderPen);
            
            // Split area into top (Advisor) and bottom (CX)
            int halfHeight = rect.bottom / 2;
            
            RECT rectAdvisor = { 0, 0, rect.right, halfHeight };
            RECT rectCX = { 0, halfHeight, rect.right, rect.bottom };
            
            // Draw Labels
            SetBkMode(hdcMem, TRANSPARENT);
            SetTextColor(hdcMem, RGB(180, 180, 180));
            SelectObject(hdcMem, hFontSmall);
            
            std::wstring micName = GetMicDeviceName();
            char micBuf[128];
            sprintf_s(micBuf, "Advisor: %.20ls", micName.c_str());

            RECT labelAdvisor = rectAdvisor; 
            labelAdvisor.left += 6; labelAdvisor.top += 2;
            DrawText(hdcMem, micBuf, -1, &labelAdvisor, DT_LEFT | DT_TOP | DT_SINGLELINE);
            
            RECT labelCX = rectCX; 
            labelCX.left += 6; labelCX.top += 2;
            DrawText(hdcMem, "Audio Output (CX)", -1, &labelCX, DT_LEFT | DT_TOP | DT_SINGLELINE);

            // Draw Waveforms
            DrawWaveform(hdcMem, rectAdvisor, levelHistory.data(), levelHistoryIndex, IsDefaultMicMuted(), RGB(0, 255, 0), RGB(255, 255, 0));
            DrawWaveform(hdcMem, rectCX, speakerLevelHistory.data(), levelHistoryIndex, false, RGB(0, 200, 255), RGB(0, 100, 255));
            
            // Divider Line
            HPEN divPen = CreatePen(PS_SOLID, 1, RGB(50, 50, 60));
            SelectObject(hdcMem, divPen);
            MoveToEx(hdcMem, 4, halfHeight, nullptr);
            LineTo(hdcMem, rect.right - 4, halfHeight);
            DeleteObject(divPen);

            // Blit to screen
            BitBlt(hdc, 0, 0, rect.right, rect.bottom, hdcMem, 0, 0, SRCCOPY);

            // Cleanup
            SelectObject(hdcMem, hbmOld);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);

            EndPaint(hWnd, &ps);
            return 0;
        }
        
        case WM_ERASEBKGND:
            return 1; // Prevent flickering
        
        case WM_RBUTTONUP:
            showMeter = false;
            SaveSettings();
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}
