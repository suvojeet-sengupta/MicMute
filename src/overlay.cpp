#include "overlay.h"
#include "globals.h"
#include "settings.h"
#include "audio.h"
#include "ui.h"
#include <dwmapi.h>
#include <cmath> // for abs

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
        SetLayeredWindowAttributes(hOverlayWnd, colorChroma, (BYTE)overlayOpacity, LWA_ALPHA | LWA_COLORKEY);
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

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rect;
            GetClientRect(hWnd, &rect);
            
            // Setup
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, hFontOverlay);
            
            bool muted = IsDefaultMicMuted();
            
            // Draw Pill Background
            HBRUSH bgBrush = muted ? hBrushOverlayMuted : hBrushOverlayLive;
            SelectObject(hdc, bgBrush);
            SelectObject(hdc, GetStockObject(NULL_PEN)); 
            RoundRect(hdc, 0, 0, rect.right, rect.bottom, 40, 40); // Full rounded corners
            
            // Draw White Border
            HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            SelectObject(hdc, borderPen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, 1, 1, rect.right-1, rect.bottom-1, 40, 40);
            DeleteObject(borderPen);
            
            // Draw Icon (Smaller and tighter)
            HICON hIcon = muted ? hIconMicOff : hIconMicOn;
            DrawIconEx(hdc, 8, 10, hIcon, 20, 20, 0, NULL, DI_NORMAL);
            
            // Draw Text
            RECT textRect = rect;
            textRect.left += 32; // Shift text right
            DrawText(hdc, muted ? "MUTED" : "LIVE", -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            
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
            
            int centerY = (rect.bottom - rect.top) / 2;
            int availHeight = rect.bottom - rect.top - 4; // padding
            int maxHalfHeight = availHeight / 2;
            
            bool muted = IsDefaultMicMuted();
            int numSamples = LEVEL_HISTORY_SIZE;
            
            // Draw waveform
            // We draw vertical lines for each history sample
            // Spacing depends on window width and history size.
            // For smoother look, we might draw fewer lines if window is small, 
            // but here we just map 1:1 or similarly.
            
            float stepX = (float)(rect.right - rect.left - 16) / (float)(numSamples - 1);
            int startX = 8;

            for (int i = 0; i < numSamples; i++) {
                // Get oldest to newest? Or newest to oldest?
                // Usually newest is at right.
                // Circular buffer logic:
                int idx = (levelHistoryIndex - 1 - i + numSamples) % numSamples; // Newest is at index 0 (visual right)
                // Actually let's draw Left=Oldest, Right=Newest
                // Oldest is at (levelHistoryIndex)
                int bufferIdx = (levelHistoryIndex + i) % numSamples;
                
                float rawLevel = levelHistory[bufferIdx];
                
                // Logarithmic Scaling (dB)
                // Range: -60dB -> 0dB mapped to 0.0 -> 1.0
                float displayLevel = 0.0f;
                if (rawLevel > 0.000001f) { // Avoid log(0)
                    float db = 20.0f * log10f(rawLevel);
                    // Map -60 to 0 -> 0 to 1
                    // db = -60 => 0
                    // db = 0 => 1
                     displayLevel = (db + 60.0f) / 60.0f;
                }
                if (displayLevel < 0.0f) displayLevel = 0.0f;
                if (displayLevel > 1.0f) displayLevel = 1.0f;
                
                if (muted) displayLevel = 0.0f;

                // Calculate Bar Height
                int halfHeight = (int)(displayLevel * maxHalfHeight);
                if (halfHeight < 1) halfHeight = 1; // Minimum visualization (center line)

                // Color Logic (Gradient)
                // Low(0.0) -> Green, Mid(0.7) -> Yellow, Hi(1.0) -> Red
                int r = 0, g = 255, b = 0;
                if (displayLevel < 0.5f) {
                    // Green to Yellow
                    // 0.0: 0,255,0
                    // 0.5: 255,255,0
                    r = (int)(displayLevel * 2.0f * 255.0f);
                    g = 255;
                } else {
                    // Yellow to Red
                    // 0.5: 255,255,0
                    // 1.0: 255,0,0
                    r = 255;
                    g = 255 - (int)((displayLevel - 0.5f) * 2.0f * 255.0f);
                }
                
                // Dim if very old sample? No, standard view is uniform brightness.
                
                COLORREF col = RGB(r, g, b);
                if (muted) col = RGB(50, 50, 50);

                HPEN linePen = CreatePen(PS_SOLID, 1, col); // Thicker lines? 1px is fine for 128 samples in 180px width
                
                // Draw Vertical Line centered
                int x = startX + (int)(i * stepX);
                
                SelectObject(hdc, linePen);
                MoveToEx(hdc, x, centerY - halfHeight, NULL);
                LineTo(hdc, x, centerY + halfHeight);
                
                DeleteObject(linePen);
            }
            
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
