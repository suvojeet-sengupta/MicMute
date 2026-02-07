#include "recorder.h"
#include "WasapiRecorder.h"
#include "call_recorder.h"
#include "globals.h"
#include "settings.h"
#include <commdlg.h>
#include <shlobj.h>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comdlg32.lib")

HWND hRecorderWnd = NULL;
static WasapiRecorder recorder; // Global instance

// Button IDs (Local)
#define BTN_START_PAUSE 2001
#define BTN_STOP_SAVE   2002
#define BTN_FOLDER      2003

// Tooltips
HWND hToolTip = NULL;

// Auto-record status notification
static std::string lastSavedFileName;
static DWORD savedNotifyStartTime = 0;
static const DWORD SAVED_NOTIFY_DURATION_MS = 3000; // Show "Saved" for 3 seconds
static int animationFrame = 0; // For pulsing animation

void SaveRecorderPosition() {
    if (!hRecorderWnd) return;
    RECT rect; GetWindowRect(hRecorderWnd, &rect);
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    
    HKEY hKey;
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\MicMute-S", &hKey) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, "RecorderX", 0, REG_DWORD, (BYTE*)&rect.left, sizeof(DWORD));
        RegSetValueEx(hKey, "RecorderY", 0, REG_DWORD, (BYTE*)&rect.top, sizeof(DWORD));
        // W and H are fixed/calculated now, but saving them doesn't hurt
        RegCloseKey(hKey);
    }
}

void LoadRecorderPosition(int* x, int* y, int* w, int* h) {
    *x = 200; *y = 200;
    *w = 420; *h = 60; // Default Horizontal Toolbar Size
    
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

// Called by CallAutoRecorder when a call is saved
void NotifyAutoRecordSaved(const std::string& filename) {
    lastSavedFileName = filename;
    savedNotifyStartTime = GetTickCount();
    if (hRecorderWnd) {
        InvalidateRect(hRecorderWnd, NULL, FALSE);
    }
}

std::string BrowseFolder(HWND owner) {
    char path[MAX_PATH];
    BROWSEINFO bi = { 0 };
    bi.lpszTitle = "Select Recording Output Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.hwndOwner = owner;
    
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl != 0) {
        if (SHGetPathFromIDList(pidl, path)) {
            CoTaskMemFree(pidl);
            return std::string(path);
        }
        CoTaskMemFree(pidl);
    }
    return "";
}

// Helpers for file naming
std::string GetCurrentTimestampString() {
    auto t = std::time(nullptr);
    struct tm tm;
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

// Helper: Get current date as YYYY-MM-DD
std::string GetCurrentDateString() {
    auto t = std::time(nullptr);
    struct tm tm;
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

// Helper: Create directory if it doesn't exist
bool CreateDirectoryIfNeeded(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return true; // Already exists
    }
    return CreateDirectoryA(path.c_str(), NULL) != 0;
}

// Get date folder path and create if needed
std::string GetDateFolderPath() {
    if (recordingFolder.empty()) return "";
    std::string dateFolder = recordingFolder + "\\" + GetCurrentDateString();
    CreateDirectoryIfNeeded(dateFolder);
    return dateFolder;
}

static time_t recordingStartTime = 0;

void DrawIconBtn(LPDRAWITEMSTRUCT lpDrawItem, int type) {
    HDC hdc = lpDrawItem->hDC;
    RECT rc = lpDrawItem->rcItem;
    bool selected = lpDrawItem->itemState & ODS_SELECTED;
    
    // Background (Dark rounded)
    HBRUSH hBr = CreateSolidBrush(selected ? RGB(80, 80, 80) : RGB(50, 50, 50));
    FillRect(hdc, &rc, hBr);
    DeleteObject(hBr);
    
    // Draw Icon Shape
    HBRUSH hShape = CreateSolidBrush(RGB(240, 240, 240));
    HBRUSH hRed = CreateSolidBrush(RGB(255, 60, 60));
    
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    
    if (type == 0) { // Start (Triangle) / Pause (Bars)
        if (recorder.IsRecording() && !recorder.IsPaused()) {
            // Pause Icon (Two vertical bars)
            RECT r1 = {cx - 6, cy - 8, cx - 2, cy + 8};
            RECT r2 = {cx + 2, cy - 8, cx + 6, cy + 8};
            FillRect(hdc, &r1, hShape);
            FillRect(hdc, &r2, hShape);
        } else {
             // Play Icon (Triangle)
             POINT pts[3] = {{cx - 5, cy - 8}, {cx - 5, cy + 8}, {cx + 7, cy}};
             HRGN rgn = CreatePolygonRgn(pts, 3, WINDING);
             FillRgn(hdc, rgn, hShape);
             DeleteObject(rgn);
        }
    } else if (type == 1) { // Stop (Square)
        RECT r = {cx - 6, cy - 6, cx + 6, cy + 6};
        FillRect(hdc, &r, hRed);
    } else if (type == 2) { // Folder
        // Simple folder shape
        RECT rBody = {cx - 8, cy - 5, cx + 8, cy + 7};
        RECT rTab = {cx - 8, cy - 8, cx - 2, cy - 5};
        FillRect(hdc, &rBody, hShape);
        FillRect(hdc, &rTab, hShape);
    }
    
    DeleteObject(hShape);
    DeleteObject(hRed);
}


void UpdateRecorderUI(HWND hWnd) {
    // Invalidate buttons to redraw icons
    InvalidateRect(GetDlgItem(hWnd, BTN_START_PAUSE), NULL, FALSE);
    InvalidateRect(GetDlgItem(hWnd, BTN_STOP_SAVE), NULL, FALSE);
    
    HWND hBtnStop = GetDlgItem(hWnd, BTN_STOP_SAVE);
    EnableWindow(hBtnStop, recorder.IsRecording());
    
    // Force repaint of status text area
    InvalidateRect(hWnd, NULL, FALSE);
}

void LayoutRecorderUI(HWND hWnd) {
    RECT rect;
    GetClientRect(hWnd, &rect);
    int newW = rect.right - rect.left;
    int newH = rect.bottom - rect.top;
    
    float scale = GetWindowScale(hWnd);
    int btnSize = (int)(40 * scale);
    int margin = (int)(10 * scale);
    
    // Right-aligned buttons
    int x = newW - margin - btnSize;
    int y = (newH - btnSize) / 2;
    
    MoveWindow(GetDlgItem(hWnd, BTN_FOLDER), x, y, btnSize, btnSize, TRUE);
    x -= (btnSize + margin);
    MoveWindow(GetDlgItem(hWnd, BTN_STOP_SAVE), x, y, btnSize, btnSize, TRUE);
    x -= (btnSize + margin);
    MoveWindow(GetDlgItem(hWnd, BTN_START_PAUSE), x, y, btnSize, btnSize, TRUE);
}

void CreateRecorderWindow(HINSTANCE hInstance) {
    int x, y, w, h;
    LoadRecorderPosition(&x, &y, &w, &h);
    
    // Enforce size (thin bar)
    float scale = GetWindowScale(NULL);
    w = (int)(420 * scale);
    h = (int)(60 * scale);
    
    hRecorderWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        "MicMuteS_Recorder", "Call Recorder", 
        WS_POPUP | WS_SYSMENU, // No caption, but keeping sysmenu for transparency?
        x, y, w, h,
        NULL, NULL, hInstance, NULL
    );
    
    if (hRecorderWnd) {
        // Set opacity
        SetLayeredWindowAttributes(hRecorderWnd, 0, 240, LWA_ALPHA);
        
        // Create Buttons (Owner Draw)
        CreateWindow("BUTTON", "Start", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 
            0, 0, 0, 0, hRecorderWnd, (HMENU)BTN_START_PAUSE, hInstance, NULL);

        CreateWindow("BUTTON", "Stop", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 
            0, 0, 0, 0, hRecorderWnd, (HMENU)BTN_STOP_SAVE, hInstance, NULL);
            
        CreateWindow("BUTTON", "Folder", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 
            0, 0, 0, 0, hRecorderWnd, (HMENU)BTN_FOLDER, hInstance, NULL);

        LayoutRecorderUI(hRecorderWnd);
        ShowWindow(hRecorderWnd, SW_SHOW);
        UpdateWindow(hRecorderWnd);
    }
}

LRESULT CALLBACK RecorderWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            // Could create tooltips here
            break;

        case WM_NCHITTEST: {
             // Allow dragging by clicking anywhere on background
             LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);
             if (hit == HTCLIENT) return HTCAPTION;
             return hit;
        }
        
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            int type = 0;
            if (lpDrawItem->CtlID == BTN_START_PAUSE) type = 0;
            else if (lpDrawItem->CtlID == BTN_STOP_SAVE) type = 1;
            else if (lpDrawItem->CtlID == BTN_FOLDER) type = 2;
            DrawIconBtn(lpDrawItem, type);
            return TRUE;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            
            if (id == BTN_FOLDER) {
                std::string newPath = BrowseFolder(hWnd);
                if (!newPath.empty()) {
                    recordingFolder = newPath;
                    SaveSettings(); // Persist immediately
                    MessageBox(hWnd, "Recording folder updated!", "MicMute-S", MB_OK);
                }
            }
            else if (id == BTN_START_PAUSE) {
                if (!recorder.IsRecording()) {
                    // Check Folder
                    if (recordingFolder.empty()) {
                         int ret = MessageBox(hWnd, "Please select a folder to save recordings.", "First Time Setup", MB_OKCANCEL);
                         if (ret == IDOK) {
                             std::string newPath = BrowseFolder(hWnd);
                             if (!newPath.empty()) {
                                 recordingFolder = newPath;
                                 SaveSettings();
                             } else {
                                 return 0; // Cancelled
                             }
                         } else {
                             return 0; // Cancelled
                         }
                    }
                    
                    if (!recordingFolder.empty()) {
                         if (recorder.Start()) {
                             recordingStartTime = std::time(nullptr);
                         } else {
                             MessageBox(hWnd, "Failed to initialize recording.", "Error", MB_ICONERROR);
                         }
                    }
                } else {
                    if (recorder.IsPaused()) recorder.Resume();
                    else recorder.Pause();
                }
                UpdateRecorderUI(hWnd);
            }
            else if (id == BTN_STOP_SAVE) {
                if (recorder.IsRecording()) {
                    // STOP and SAVE
                    recorder.Stop();
                    time_t endTime = std::time(nullptr);
                    
                    std::string timestamp = GetCurrentTimestampString();
                    std::string dateFolder = GetDateFolderPath();
                    std::string filename = "Recording_" + timestamp + ".wav";
                    std::string fullPath = dateFolder + "\\" + filename;
                    
                    if (recorder.SaveToFile(fullPath)) {
                        // Create Metadata TXT
                        std::string txtPath = dateFolder + "\\" + "Recording_" + timestamp + ".txt";
                        std::ofstream txtFile(txtPath);
                        if (txtFile.is_open()) {
                            struct tm tmStart, tmEnd;
                            localtime_s(&tmStart, &recordingStartTime);
                            localtime_s(&tmEnd, &endTime);
                            
                            txtFile << "Recording Metadata\n";
                            txtFile << "==================\n";
                            txtFile << "File: " << filename << "\n";
                            txtFile << "Start Time: " << std::put_time(&tmStart, "%Y-%m-%d %H:%M:%S") << "\n";
                            txtFile << "End Time: " << std::put_time(&tmEnd, "%Y-%m-%d %H:%M:%S") << "\n";
                            double duration = difftime(endTime, recordingStartTime);
                            txtFile << "Duration: " << duration << " seconds\n";
                            txtFile.close();
                        }
                        
                        // Using a simple message box for now, could be a toast
                        // MessageBox(hWnd, "Recording Saved!", "MicMute-S", MB_OK);
                    } else {
                        MessageBox(hWnd, "Failed to save recording file.", "Error", MB_ICONERROR);
                    }
                    UpdateRecorderUI(hWnd);
                }
            }
            break;
        }
        
        case WM_ERASEBKGND:
            return 1; // Prevent background erasure to reduce flickering

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rect; GetClientRect(hWnd, &rect);
            
            // Double Buffering
            HDC hMemDC = CreateCompatibleDC(hdc);
            HBITMAP hMemBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hMemBitmap);
            
            // Draw Background (Mica-like dark gray)
            FillRect(hMemDC, &rect, hBrushBg); // Uses global bg color
            
            // Draw Text
            SetBkMode(hMemDC, TRANSPARENT);
            SetTextColor(hMemDC, colorText);
            SelectObject(hMemDC, hFontNormal);
            
            RECT textRect = rect;
            textRect.left += 20; // Margin
            textRect.right -= 150; // Space for buttons
            
            // Check for saved notification first (shows for 3 seconds)
            DWORD now = GetTickCount();
            bool showingSavedNotify = (savedNotifyStartTime > 0 && 
                                       (now - savedNotifyStartTime) < SAVED_NOTIFY_DURATION_MS);
            
            if (showingSavedNotify) {
                // Show "Saved: filename" with green color
                SetTextColor(hMemDC, RGB(100, 255, 100)); // Green
                std::string msg = "Saved: " + lastSavedFileName;
                DrawText(hMemDC, msg.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            }
            else if (g_CallRecorder && g_CallRecorder->IsEnabled() && 
                     g_CallRecorder->GetState() == CallAutoRecorder::State::RECORDING) {
                // Auto-recording in progress - show animated indicator
                animationFrame = (animationFrame + 1) % 3;
                
                // Pulsing red color for recording indicator
                int pulse = 180 + (animationFrame * 25); // Varies from 180 to 230
                SetTextColor(hMemDC, RGB(pulse, 60, 60));
                
                // Draw pulsing dot indicator
                const char* dots[] = {"●  ", " ● ", "  ●"};
                std::string msg = std::string(dots[animationFrame]) + " Auto Recording...";
                DrawText(hMemDC, msg.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            }
            else if (recorder.IsRecording()) {
                // Manual recording
                if (recorder.IsPaused()) {
                    SetTextColor(hMemDC, RGB(255, 200, 0)); // Yellow
                    DrawText(hMemDC, "Recording Paused", -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                } else {
                    SetTextColor(hMemDC, RGB(255, 60, 60)); // Red
                    DrawText(hMemDC, "Recording...", -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                }
            } else if (g_CallRecorder && g_CallRecorder->IsEnabled()) {
                // Auto-record enabled but detecting (waiting for voice)
                SetTextColor(hMemDC, RGB(100, 180, 255)); // Blue
                DrawText(hMemDC, "Auto: Listening...", -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            } else {
                SetTextColor(hMemDC, RGB(200, 200, 200));
                DrawText(hMemDC, "Ready", -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            }
            
            // Copy from memory DC to screen DC
            BitBlt(hdc, 0, 0, rect.right, rect.bottom, hMemDC, 0, 0, SRCCOPY);
            
            // Cleanup
            SelectObject(hMemDC, hOldBitmap);
            DeleteObject(hMemBitmap);
            DeleteDC(hMemDC);
            
            EndPaint(hWnd, &ps);
            return 0;
        }
        
        case WM_CLOSE:
            // Prevent closing via Alt+F4 if we want "always open" logic?
            // User requested "always open rhega if ... enabled".
            // So if enabled, don't close. Only close if disabled in settings.
            if (showRecorder) {
                // Ignore close, maybe minimize?
                return 0;
            }
            ShowWindow(hWnd, SW_HIDE);
            return 0;

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}


