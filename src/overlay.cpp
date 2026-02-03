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
            
            // Draw waveform bars
            int barWidth = 4;
            int gap = 2;
            int startX = 10;
            int maxHeight = rect.bottom - 16;
            int centerY = rect.bottom / 2;
            
            bool muted = IsDefaultMicMuted();

            
            for (int i = 0; i < LEVEL_HISTORY_SIZE; i++) {
                int idx = (levelHistoryIndex - LEVEL_HISTORY_SIZE + i + LEVEL_HISTORY_SIZE) % LEVEL_HISTORY_SIZE;
                float level = levelHistory[idx];
                
                // Visual boost: Use square root to make low volume more visible (log-like behavior)
                // A signal of 0.1 (common speech) becomes 0.31, making it clearly visible
                float displayLevel = (level > 0.0f) ? sqrt(level) : 0.0f;
                
                if (muted) displayLevel = 0;
                
                int barHeight = (int)(displayLevel * maxHeight);
                if (barHeight < 2) barHeight = 2;
                
                // Dynamic coloring logic
                COLORREF barColor;
                if (muted) {
                    barColor = RGB(60, 60, 60); // Dark Gray
                } else {
                    if (displayLevel > 0.90f) {
                        barColor = RGB(255, 50, 50);   // Red (Clipping)
                    } else if (displayLevel > 0.65f) {
                        barColor = RGB(255, 200, 0);   // Yellow (Loud)
                    } else {
                        barColor = colorLive;          // standard color (likely Green/Blue)
                    }
                }
                
                HBRUSH currentBarBrush = CreateSolidBrush(barColor);

                RECT barRect;
                barRect.left = startX + i * (barWidth + gap);
                barRect.right = barRect.left + barWidth;
                barRect.top = centerY - barHeight / 2;
                barRect.bottom = centerY + barHeight / 2;
                
                FillRect(hdc, &barRect, currentBarBrush);
                DeleteObject(currentBarBrush);
            }
            // Removed redundant DeleteObject(barBrush) since we create/delete inside loop now
            
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
