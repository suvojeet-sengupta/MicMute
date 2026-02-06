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
    HKEY hKey;
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, "RecorderX", 0, REG_DWORD, (BYTE*)&rect.left, sizeof(DWORD));
        RegSetValueEx(hKey, "RecorderY", 0, REG_DWORD, (BYTE*)&rect.top, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

void LoadRecorderPosition(int* x, int* y) {
    *x = 200; *y = 200;
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        RegQueryValueEx(hKey, "RecorderX", NULL, NULL, (BYTE*)x, &size);
        RegQueryValueEx(hKey, "RecorderY", NULL, NULL, (BYTE*)y, &size);
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

void CreateRecorderWindow(HINSTANCE hInstance) {
    int x, y;
    LoadRecorderPosition(&x, &y);
    
    // Scale factor
    float scale = GetWindowScale(NULL);
    
    hRecorderWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        "MicMuteS_Recorder", "Call Recorder", WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, (int)(200 * scale), (int)(100 * scale),
        NULL, NULL, hInstance, NULL
    );
    
    if (hRecorderWnd) {
        // Set opacity
        SetLayeredWindowAttributes(hRecorderWnd, 0, 240, LWA_ALPHA);
        
        // Buttons
        HWND hBtn = CreateWindow("BUTTON", "Start", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 
            (int)(10 * scale), (int)(10 * scale), (int)(80 * scale), (int)(40 * scale), hRecorderWnd, (HMENU)BTN_START_PAUSE, hInstance, NULL);
        SendMessage(hBtn, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        HWND hBtn2 = CreateWindow("BUTTON", "Stop/Save", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 
            (int)(110 * scale), (int)(10 * scale), (int)(80 * scale), (int)(40 * scale), hRecorderWnd, (HMENU)BTN_STOP_SAVE, hInstance, NULL);
        SendMessage(hBtn2, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        EnableWindow(hBtn2, FALSE);

        ShowWindow(hRecorderWnd, SW_SHOW);
        UpdateWindow(hRecorderWnd);
    }
}

void ShowRecorderWindow(bool show) {
    if (show) {
        if (!hRecorderWnd) CreateRecorderWindow(GetModuleHandle(NULL));
        else ShowWindow(hRecorderWnd, SW_SHOW);
    } else {
        if (hRecorderWnd) ShowWindow(hRecorderWnd, SW_HIDE);
    }
}

LRESULT CALLBACK RecorderWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CTLCOLORSTATIC:
            SetBkMode((HDC)wParam, TRANSPARENT);
            SetTextColor((HDC)wParam, colorText);
            return (LRESULT)hBrushBg;
            
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
                char szFile[260] = "recording.wav";
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
                   // User cancelled save. The buffer is still there until they click Start again.
                   // Optionally we could auto-clear or keep it. Current logic keeps it until next start.
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
            FillRect(hdc, &rect, hBrushBg);
            
            // Draw Status Text
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, colorText);
            SelectObject(hdc, hFontSmall);
            
            RECT textRect = {10, 60, 190, 80};
            if (recorder.IsRecording()) {
                if (recorder.IsPaused()) DrawText(hdc, "PAUSED", -1, &textRect, DT_CENTER);
                else DrawText(hdc, "RECORDING...", -1, &textRect, DT_CENTER);
            } else {
                DrawText(hdc, "Ready", -1, &textRect, DT_CENTER);
            }
            
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

