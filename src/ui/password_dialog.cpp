#include "ui/password_dialog.h"
#include "core/globals.h"
#include <string>

#define ID_EDIT_PASS 5001
#define ID_BTN_OK    5002
#define ID_BTN_CANCEL 5003

static std::string inputPassword;
static bool passwordCorrect = false;

static LRESULT CALLBACK PasswordDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Label
            CreateWindow("STATIC", "Enter Password:", 
                WS_CHILD | WS_VISIBLE, 
                20, 20, 200, 20, hWnd, nullptr, ((LPCREATESTRUCT)lParam)->hInstance, nullptr);

            // Edit Box (Password)
            CreateWindow("EDIT", "", 
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL, 
                20, 50, 240, 25, hWnd, (HMENU)ID_EDIT_PASS, ((LPCREATESTRUCT)lParam)->hInstance, nullptr);

            // OK Button
            CreateWindow("BUTTON", "OK", 
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 
                60, 90, 80, 30, hWnd, (HMENU)ID_BTN_OK, ((LPCREATESTRUCT)lParam)->hInstance, nullptr);

            // Cancel Button
            CreateWindow("BUTTON", "Cancel", 
                WS_CHILD | WS_VISIBLE, 
                150, 90, 80, 30, hWnd, (HMENU)ID_BTN_CANCEL, ((LPCREATESTRUCT)lParam)->hInstance, nullptr);
            
            // Set focus to edit box
            SetFocus(GetDlgItem(hWnd, ID_EDIT_PASS));
            break;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_BTN_OK) {
                char buf[256];
                GetWindowText(GetDlgItem(hWnd, ID_EDIT_PASS), buf, 256);
                inputPassword = buf;
                
                if (inputPassword == APP_PASSWORD) {
                    passwordCorrect = true;
                    DestroyWindow(hWnd);
                } else {
                    MessageBox(hWnd, "Incorrect Password", "Error", MB_ICONERROR);
                    SetWindowText(GetDlgItem(hWnd, ID_EDIT_PASS), "");
                    SetFocus(GetDlgItem(hWnd, ID_EDIT_PASS));
                }
            } else if (id == ID_BTN_CANCEL) {
                passwordCorrect = false;
                DestroyWindow(hWnd);
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

bool PromptForPassword(HWND hParent) {
    passwordCorrect = false; // Reset for each attempt
    inputPassword = "";
    
    // Register class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = PasswordDlgProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "MicMuteS_PasswordDlg";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassEx(&wc);

    // Calculate center
    RECT rcParent;
    GetWindowRect(hParent, &rcParent);
    int w = 300, h = 180;
    int x = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;

    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "MicMuteS_PasswordDlg", "Security Check",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, w, h,
        hParent, nullptr, GetModuleHandle(nullptr), nullptr
    );

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    EnableWindow(hParent, FALSE);

    // Modal message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_QUIT) break; // Window destroyed
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            // Handle Enter key for OK button
             if (GetForegroundWindow() == hDlg || IsChild(hDlg, GetForegroundWindow())) {
                 SendMessage(hDlg, WM_COMMAND, ID_BTN_OK, 0);
                 continue;
             }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);
    
    return passwordCorrect;
}
