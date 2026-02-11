#include "ui/developer_options.h"
#include "core/globals.h"
#include "core/resource.h"
#include "core/settings.h"
#include "ui/ui.h"
#include <dwmapi.h>
#include <commctrl.h>
#include <string>

// Local IDs for this window
#define ID_DEV_AUTO_REC      2001
#define ID_DEV_MANUAL_BTN    2003
#define ID_DEV_DELETE_LABEL  2004
#define ID_DEV_DELETE_COMBO  2005

static HWND hDevOptionsWnd = nullptr;

static LRESULT CALLBACK DeveloperOptionsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
            
            // Background brush for this window (matches app theme)
            SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(colorBg));

            int x = 40;
            int y = 60;
            int gap = 40;

            // Title
            CreateWindow("STATIC", "Developer Options", 
                WS_CHILD | WS_VISIBLE | SS_CENTER, 
                0, 15, 400, 30, hWnd, nullptr, hInst, nullptr);

            // Auto Record Calls
            HWND hAuto = CreateWindow("BUTTON", "Auto record calls (Ozonetel)", 
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
                x, y, 320, 24, hWnd, (HMENU)ID_DEV_AUTO_REC, hInst, nullptr);
            SendMessage(hAuto, BM_SETCHECK, autoRecordCalls ? BST_CHECKED : BST_UNCHECKED, 0);
            
            // Show Manual Record Buttons
            HWND hManual = CreateWindow("BUTTON", "Show Manual Record Buttons", 
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
                x, y + gap, 320, 24, hWnd, (HMENU)ID_DEV_MANUAL_BTN, hInst, nullptr);
            SendMessage(hManual, BM_SETCHECK, showManualRec ? BST_CHECKED : BST_UNCHECKED, 0);

            // Auto-delete label
            CreateWindow("STATIC", "Auto-delete recordings after:", 
                WS_CHILD | WS_VISIBLE | SS_LEFT, 
                x, y + gap*2 + 4, 180, 20, hWnd, (HMENU)ID_DEV_DELETE_LABEL, hInst, nullptr);

            // Auto-delete combo
            HWND hCombo = CreateWindow("COMBOBOX", "",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                x + 190, y + gap*2, 120, 200, hWnd, (HMENU)ID_DEV_DELETE_COMBO, hInst, nullptr);
            
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"Never");
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"7 days");
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"14 days");
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"30 days");
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"60 days");
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"90 days");

            int comboIdx = 0;
            if (autoDeleteDays == 7) comboIdx = 1;
            else if (autoDeleteDays == 14) comboIdx = 2;
            else if (autoDeleteDays == 30) comboIdx = 3;
            else if (autoDeleteDays == 60) comboIdx = 4;
            else if (autoDeleteDays == 90) comboIdx = 5;
            SendMessage(hCombo, CB_SETCURSEL, comboIdx, 0);

            // Fonts
            if (hFontTitle) SendMessage(GetWindow(hWnd, GW_CHILD), WM_SETFONT, (WPARAM)hFontTitle, TRUE);
            if (hFontNormal) {
                EnumChildWindows(hWnd, [](HWND hChild, LPARAM lParam) -> BOOL {
                    // Skip the title which is first
                    if (GetWindow(GetParent(hChild), GW_CHILD) != hChild) {
                        SendMessage(hChild, WM_SETFONT, (WPARAM)lParam, TRUE);
                    }
                    return TRUE;
                }, (LPARAM)hFontNormal);
            }

            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, colorText);
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }

        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, colorText);
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH); // Parent background
        }
        
        // For Combo List
        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, colorText);
            SetBkColor(hdc, RGB(30, 30, 40));
            return (LRESULT)GetStockObject(BLACK_BRUSH); // Simpler brush
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);

            if (id == ID_DEV_AUTO_REC && code == BN_CLICKED) {
                autoRecordCalls = (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SaveSettings(); 
                // Toggle actual recorder state
                extern void InitCallRecorder();
                extern void CleanupCallRecorder();
                // To apply instantly we might need to re-init
                CleanupCallRecorder();
                if (autoRecordCalls) InitCallRecorder();
            }
            else if (id == ID_DEV_MANUAL_BTN && code == BN_CLICKED) {
                showManualRec = (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SaveSettings();
                if (hControlPanel) InvalidateRect(hControlPanel, nullptr, FALSE);
            }
            else if (id == ID_DEV_DELETE_COMBO && code == CBN_SELCHANGE) {
                int idx = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                switch(idx) {
                    case 0: autoDeleteDays = 0; break;
                    case 1: autoDeleteDays = 7; break;
                    case 2: autoDeleteDays = 14; break;
                    case 3: autoDeleteDays = 30; break;
                    case 4: autoDeleteDays = 60; break;
                    case 5: autoDeleteDays = 90; break;
                }
                SaveSettings();
            }
            break;
        }

        case WM_CLOSE:
            hDevOptionsWnd = nullptr;
            DestroyWindow(hWnd);
            break;

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void RegisterDeveloperOptionsClass(HINSTANCE hInstance) {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = DeveloperOptionsWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // Will be overridden in WM_CREATE
    wc.lpszClassName = "MicMuteS_DevOptions";
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    RegisterClassEx(&wc);
}

void CreateDeveloperOptionsWindow(HINSTANCE hInstance, HWND hParent) {
    if (hDevOptionsWnd) {
        SetForegroundWindow(hDevOptionsWnd);
        return;
    }

    int w = 400;
    int h = 300;
    
    // Center on parent
    RECT rc; GetWindowRect(hParent, &rc);
    int x = rc.left + (rc.right - rc.left - w) / 2;
    int y = rc.top + (rc.bottom - rc.top - h) / 2;

    hDevOptionsWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "MicMuteS_DevOptions", "Developer Options",
        WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        x, y, w, h,
        hParent, nullptr, hInstance, nullptr
    );

    // Apply DWM rounding
    HMODULE hDwm = GetModuleHandle("dwmapi.dll");
    if (hDwm) {
        typedef HRESULT(WINAPI* DwmSetAttrFn)(HWND, DWORD, LPCVOID, DWORD);
        auto pSetAttr = (DwmSetAttrFn)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pSetAttr) {
            int cornerPref = 2; // Round
            pSetAttr(hDevOptionsWnd, 33, &cornerPref, sizeof(cornerPref));
            
            // Dark mode
            BOOL val = TRUE;
            pSetAttr(hDevOptionsWnd, 20, &val, sizeof(val));
        }
    }
}
