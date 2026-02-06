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
        
        // Initial resize based on DPI
        float scale = GetWindowScale(hOverlayWnd);
        SetWindowPos(hOverlayWnd, NULL, 0, 0, (int)(100 * scale), (int)(40 * scale), SWP_NOMOVE | SWP_NOZORDER);

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
    
    // Scaled initially? We need window handle first usually, OR assume system DPI
    float scale = GetWindowScale(NULL);
    
    // Increased height to 100 to accommodate dual meters (Advisor + CX)
    hMeterWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "MicMuteS_Meter", NULL, WS_POPUP,
        meterX, meterY, (int)(180 * scale), (int)(100 * scale), 
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
        case WM_DPICHANGED: {
             // Resize logic when moved to another monitor
             RECT* const prcNewWindow = (RECT*)lParam;
             SetWindowPos(hWnd,
                 NULL,
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
            float scale = GetWindowScale(hWnd);
            
            // Setup
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, hFontOverlay); // Note: Font size needs handling too...
            // Creating a scaled font on fly or pre-creating?
            // For now, let's keep font as is (Windows might scale it if we aren't careful, but we are DPI aware so...
            // GDI fonts defined in main.cpp with explicit sizes. We might need to select a larger font here.)
            
            // Interpolate color
            COLORREF bgCol = InterpColor(colorOverlayBgLive, colorOverlayBgMuted, animProgress);
            HBRUSH bgBrush = CreateSolidBrush(bgCol);
            
            SelectObject(hdc, bgBrush);
            SelectObject(hdc, GetStockObject(NULL_PEN)); 
            int corner = (int)(40 * scale);
            RoundRect(hdc, 0, 0, rect.right, rect.bottom, corner, corner); // Full rounded corners
            
            DeleteObject(bgBrush);
            
            // Draw White Border
            HPEN borderPen = CreatePen(PS_SOLID, (int)(2 * scale), RGB(255, 255, 255));
            SelectObject(hdc, borderPen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, 1, 1, rect.right-1, rect.bottom-1, corner, corner);
            DeleteObject(borderPen);
            
            // Determine icon based on progress (snap at 50%)
            bool showMuted = (animProgress > 0.5f);
            HICON hIcon = showMuted ? hIconMicOff : hIconMicOn;
            int iconSize = (int)(20 * scale);
            int iconX = (int)(8 * scale);
            int iconY = (rect.bottom - iconSize) / 2; //(int)(10 * scale);
            DrawIconEx(hdc, iconX, iconY, hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
            
            // Draw Text
            RECT textRect = rect;
            textRect.left += (int)(32 * scale); // Shift text right
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
    int height = rect.bottom - rect.top;
    int maxHalfHeight = height / 2 - 2;
    
    // Draw Grid (dB lines)
    HPEN gridPen = CreatePen(PS_DOT, 1, RGB(40, 40, 50));
    SelectObject(hdc, gridPen);
    // Draw lines at 25%, 50%, 75% amplitude
    int y0 = centerY;
    int y1 = centerY - maxHalfHeight / 2;
    int y2 = centerY + maxHalfHeight / 2;
    // Actually, let's draw simpler horizontal grid lines
    // -6dB, -12dB, -24dB lines?
    // Map -6dB -> 0.5 amplitude roughly
    
    // Just draw a center line
    MoveToEx(hdc, rect.left, centerY, NULL);
    LineTo(hdc, rect.right, centerY);
    DeleteObject(gridPen);

    float stepX = (float)(rect.right - rect.left - 4) / (float)(LEVEL_HISTORY_SIZE - 1);
    int startX = rect.left + 2;

    for (int i = 0; i < LEVEL_HISTORY_SIZE; i++) {
        int bufferIdx = (historyIndex + i) % LEVEL_HISTORY_SIZE;
        float rawLevel = history[bufferIdx];
        
        // Improved Logarithmic Scaling
        // Target: Show smallest sounds but avoid noise floor.
        float displayLevel = 0.0f;
        float minDb = -72.0f; // More realistic for standard microphones
        
        if (rawLevel > 0.0f) {
            float db = 20.0f * log10f(rawLevel);
            if (db < minDb) db = minDb;
            
            // Normalize 0..1
            // (-72 -> 0, 0 -> 1)
            float normalized = (db - minDb) / (0.0f - minDb);
            
            // Linear scaling is safer for noisy mics, but let's keep a mild boost
            displayLevel = powf(normalized, 0.9f); 
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
        MoveToEx(hdc, x, centerY - halfHeight, NULL);
        LineTo(hdc, x, centerY + halfHeight);
        
        SelectObject(hdc, oldPen);
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
