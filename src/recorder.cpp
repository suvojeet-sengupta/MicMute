#include "recorder.h"
#include "WasapiRecorder.h"
#include "globals.h"
#include "settings.h"
#include <commdlg.h>
#include <cstdio>

#pragma comment(lib, "winmm.lib") // Still needed for some system calls if any, but not MCI
#pragma comment(lib, "comdlg32.lib")

HWND hRecorderWnd = NULL;
static WasapiRecorder recorder; // Global instance

// Button IDs (Local)
#define BTN_START_PAUSE 2001
#define BTN_STOP_SAVE   2002

void SaveRecorderPosition() {
    if (!hRecorderWnd) return;
    RECT rect; GetWindowRect(hRecorderWnd, &rect);
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    
    HKEY hKey;
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, "RecorderX", 0, REG_DWORD, (BYTE*)&rect.left, sizeof(DWORD));
        RegSetValueEx(hKey, "RecorderY", 0, REG_DWORD, (BYTE*)&rect.top, sizeof(DWORD));
        RegSetValueEx(hKey, "RecorderW", 0, REG_DWORD, (BYTE*)&w, sizeof(DWORD));
        RegSetValueEx(hKey, "RecorderH", 0, REG_DWORD, (BYTE*)&h, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

void LoadRecorderPosition(int* x, int* y, int* w, int* h) {
    *x = 200; *y = 200;
    *w = 320; *h = 240; // Defaults
    
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        RegQueryValueEx(hKey, "RecorderX", NULL, NULL, (BYTE*)x, &size);
        RegQueryValueEx(hKey, "RecorderY", NULL, NULL, (BYTE*)y, &size);
        RegQueryValueEx(hKey, "RecorderW", NULL, NULL, (BYTE*)w, &size);
        RegQueryValueEx(hKey, "RecorderH", NULL, NULL, (BYTE*)h, &size);
        RegCloseKey(hKey);
    }
}

void InitRecorder() {
    // No explicit global init needed for WasapiRecorder
}

void CleanupRecorder() {
    recorder.Stop();
}

void UpdateRecorderUI(HWND hWnd) {
    HWND hBtnStart = GetDlgItem(hWnd, BTN_START_PAUSE);
    HWND hBtnStop = GetDlgItem(hWnd, BTN_STOP_SAVE);
    
    if (recorder.IsRecording()) {
        if (recorder.IsPaused()) {
            SetWindowText(hBtnStart, "Resume");
        } else {
            SetWindowText(hBtnStart, "Pause");
        }
        EnableWindow(hBtnStop, TRUE);
    } else {
        SetWindowText(hBtnStart, "Start");
        EnableWindow(hBtnStop, FALSE);
    }
}

void LayoutRecorderUI(HWND hWnd) {
    RECT rect;
    GetClientRect(hWnd, &rect);
    int newW = rect.right - rect.left;
    int newH = rect.bottom - rect.top;
    float scale = GetWindowScale(hWnd);

    int btnW = (int)(100 * scale);
    int btnH = (int)(36 * scale);
    int margin = (int)(20 * scale);
    int startY = newH - btnH - margin;

    HWND hBtnStart = GetDlgItem(hWnd, BTN_START_PAUSE);
    HWND hBtnStop = GetDlgItem(hWnd, BTN_STOP_SAVE);
    
    if (hBtnStart) {
        MoveWindow(hBtnStart, margin, startY, btnW, btnH, TRUE);
    }
    if (hBtnStop) {
        MoveWindow(hBtnStop, newW - btnW - margin, startY, btnW, btnH, TRUE);
    }
}

// Include for timestamp
#include <ctime>
#include <iomanip>
#include <sstream>

void CreateRecorderWindow(HINSTANCE hInstance) {
    int x, y, w, h;
    LoadRecorderPosition(&x, &y, &w, &h);
    
    // Scale factor checks (optional, but if loaded w/h are small, might scale)
    float scale = GetWindowScale(NULL);
    if (w < 200) w = (int)(320 * scale);
    if (h < 150) h = (int)(240 * scale);
    
    hRecorderWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        "MicMuteS_Recorder", "Call Recorder", 
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, // Resizable
        x, y, w, h,
        NULL, NULL, hInstance, NULL
    );
    
    if (hRecorderWnd) {
        // Set opacity (High opacity for usability)
        SetLayeredWindowAttributes(hRecorderWnd, 0, 250, LWA_ALPHA);
        
        // Initial control creation
        // We defer positioning to WM_SIZE usually, but let's create them here first
        
        HWND hBtn = CreateWindow("BUTTON", "Start", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT, 
            0, 0, 0, 0, hRecorderWnd, (HMENU)BTN_START_PAUSE, hInstance, NULL);
        SendMessage(hBtn, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        HWND hBtn2 = CreateWindow("BUTTON", "Stop/Save", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT, 
            0, 0, 0, 0, hRecorderWnd, (HMENU)BTN_STOP_SAVE, hInstance, NULL);
        SendMessage(hBtn2, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        EnableWindow(hBtn2, FALSE);

        // Perform initial layout
        LayoutRecorderUI(hRecorderWnd);

        ShowWindow(hRecorderWnd, SW_SHOW);
        UpdateWindow(hRecorderWnd);
    }
}

// ... helper for timestamp ...
std::string GetTimestampedFilename() {
    auto t = std::time(nullptr);
    struct tm tm;
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << "Recording_" << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".wav";
    return oss.str();
}

LRESULT CALLBACK RecorderWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CTLCOLORSTATIC:
            SetBkMode((HDC)wParam, TRANSPARENT);
            SetTextColor((HDC)wParam, colorText);
            return (LRESULT)hBrushBg;
            
        case WM_SIZE: {
            LayoutRecorderUI(hWnd);
            // Repaint without erase to prevent flicker
            InvalidateRect(hWnd, NULL, FALSE);
            break; 
        }

        case WM_ERASEBKGND:
            return 1; // Prevent flicker - we handle background in WM_PAINT

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == BTN_START_PAUSE) {
                if (!recorder.IsRecording()) {
                    // Start Recording
                    if (recorder.Start()) {
                        // Success
                    } else {
                        MessageBox(hWnd, "Failed to start recording. Check microphone permissions.", "Error", MB_ICONERROR);
                    }
                } else {
                    // Pause/Resume
                    if (recorder.IsPaused()) {
                        recorder.Resume();
                    } else {
                        recorder.Pause();
                    }
                }
                UpdateRecorderUI(hWnd);
            }
            else if (id == BTN_STOP_SAVE) {
                // Stop
                recorder.Stop(); // Ensure consistent state
                
                // Save Dialog
                OPENFILENAME ofn;
                char szFile[260]; 
                std::string defaultName = GetTimestampedFilename();
                strcpy_s(szFile, defaultName.c_str());
                
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hWnd;
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = sizeof(szFile);
                ofn.lpstrFilter = "WAV Audio\0*.wav\0All Files\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.lpstrDefExt = "wav";
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

                if (GetSaveFileName(&ofn) == TRUE) {
                    if (recorder.SaveToFile(ofn.lpstrFile)) {
                        MessageBox(hWnd, "Recording Saved!", "MicMute-S", MB_OK);
                    } else {
                        MessageBox(hWnd, "Failed to write file associated data.", "Error", MB_ICONERROR);
                    }
                } else {
                   // User cancelled save.
                }
                
                UpdateRecorderUI(hWnd);
            }
            break;
        }
        
        case WM_CLOSE:
            if (recorder.IsRecording()) {
                if (MessageBox(hWnd, "Recording is in progress. Stop and discard?", "Warning", MB_YESNO) == IDNO) {
                    return 0;
                }
                recorder.Stop();
            }
            showRecorder = false;
            SaveSettings();
            ShowWindow(hWnd, SW_HIDE);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            RECT rect; GetClientRect(hWnd, &rect);
            
            // Double-buffering to prevent flicker
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);
            
            // Background
            FillRect(hdcMem, &rect, hBrushBg);
            
            // Draw Status Text
            SetBkMode(hdcMem, TRANSPARENT);
            SetTextColor(hdcMem, colorText);
            SelectObject(hdcMem, hFontStatus);
            
            // Dynamically calculate centering
            float scale = GetWindowScale(hWnd);
            int btnAreaHeight = (int)(36 * scale + 40 * scale);
            RECT textRect = rect;
            textRect.bottom -= btnAreaHeight;
            
            if (recorder.IsRecording()) {
                if (recorder.IsPaused()) DrawText(hdcMem, "PAUSED", -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                else DrawText(hdcMem, "RECORDING...", -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                DrawText(hdcMem, "Ready", -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            
            // Blit to screen
            BitBlt(hdc, 0, 0, rect.right, rect.bottom, hdcMem, 0, 0, SRCCOPY);
            
            // Cleanup
            SelectObject(hdcMem, hbmOld);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);
            
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN:
            SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;

        case WM_EXITSIZEMOVE:
            SaveRecorderPosition();
            return 0;

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

