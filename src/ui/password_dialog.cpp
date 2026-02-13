#include "ui/password_dialog.h"
#include "core/globals.h"
#include <string>
#include <cmath>

#define ID_EDIT_PASS  5001
#define ID_BTN_OK     5002
#define ID_BTN_CANCEL 5003

static std::string inputPassword;
static bool passwordCorrect = false;
static bool errorFlash = false;
static ULONGLONG errorFlashTime = 0;
static HBRUSH hEditBrush = nullptr;

// Theme colors (matching app navy-dark theme)
static const COLORREF kDlgBg       = RGB(22, 22, 35);
static const COLORREF kDlgBorder   = RGB(45, 45, 65);
static const COLORREF kFieldBg     = RGB(30, 30, 48);
static const COLORREF kFieldBorder = RGB(60, 60, 85);
static const COLORREF kFieldError  = RGB(200, 50, 50);
static const COLORREF kTextPrimary = RGB(220, 220, 235);
static const COLORREF kTextDim     = RGB(140, 140, 160);
static const COLORREF kAccent      = RGB(86, 140, 245);
static const COLORREF kAccentHover = RGB(110, 160, 255);
static const COLORREF kBtnCancel   = RGB(50, 50, 68);
static const COLORREF kBtnCancelHv = RGB(65, 65, 82);
static const COLORREF kLockColor   = RGB(86, 140, 245);

static bool hoverOK = false;
static bool hoverCancel = false;

// Get button rects based on window client area
static void GetButtonRects(HWND hWnd, RECT* rcOK, RECT* rcCancel) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int w = rc.right;
    int btnW = 110, btnH = 34;
    int gap = 12;
    int totalW = btnW * 2 + gap;
    int startX = (w - totalW) / 2;
    int btnY = rc.bottom - 52;

    rcOK->left = startX; rcOK->top = btnY;
    rcOK->right = startX + btnW; rcOK->bottom = btnY + btnH;

    rcCancel->left = startX + btnW + gap; rcCancel->top = btnY;
    rcCancel->right = startX + btnW * 2 + gap; rcCancel->bottom = btnY + btnH;
}

static void DrawLockIcon(HDC hdc, int cx, int cy) {
    // Shackle (arc)
    HPEN shacklePen = CreatePen(PS_SOLID, 3, kLockColor);
    HPEN oldPen = (HPEN)SelectObject(hdc, shacklePen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Arc(hdc, cx - 10, cy - 22, cx + 10, cy - 2, cx + 10, cy - 12, cx - 10, cy - 12);
    
    // Body (rounded rect)
    HBRUSH bodyBr = CreateSolidBrush(kLockColor);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, bodyBr);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    RoundRect(hdc, cx - 13, cy - 6, cx + 13, cy + 14, 4, 4);

    // Keyhole
    HBRUSH holeBr = CreateSolidBrush(kDlgBg);
    SelectObject(hdc, holeBr);
    Ellipse(hdc, cx - 3, cy - 1, cx + 3, cy + 5);
    RECT keySlot = { cx - 1, cy + 3, cx + 2, cy + 10 };
    FillRect(hdc, &keySlot, holeBr);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(shacklePen);
    DeleteObject(bodyBr);
    DeleteObject(holeBr);
}

static LRESULT CALLBACK PasswordDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

            // Password input field
            HWND hEdit = CreateWindowEx(0, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL,
                50, 106, 260, 28, hWnd, (HMENU)ID_EDIT_PASS, hInst, nullptr);

            // Set font on the edit control
            if (hFontNormal) SendMessage(hEdit, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            SetFocus(hEdit);
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);

            // Double buffer
            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

            // Background
            HBRUSH bgBr = CreateSolidBrush(kDlgBg);
            FillRect(mem, &rc, bgBr);
            DeleteObject(bgBr);

            // Border
            HPEN borderPen = CreatePen(PS_SOLID, 1, kDlgBorder);
            SelectObject(mem, borderPen);
            SelectObject(mem, GetStockObject(NULL_BRUSH));
            Rectangle(mem, 0, 0, rc.right, rc.bottom);
            DeleteObject(borderPen);

            SetBkMode(mem, TRANSPARENT);

            // Lock icon
            DrawLockIcon(mem, rc.right / 2, 38);

            // Title
            SelectObject(mem, hFontBold ? hFontBold : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            SetTextColor(mem, kTextPrimary);
            RECT rcTitle = { 0, 56, rc.right, 78 };
            DrawText(mem, "Security Check", -1, &rcTitle, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            // Subtitle
            SelectObject(mem, hFontSmall ? hFontSmall : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            SetTextColor(mem, kTextDim);
            RECT rcSub = { 0, 78, rc.right, 98 };
            DrawText(mem, "Enter password to continue", -1, &rcSub, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            // Edit field border (drawn around the EDIT control area)
            ULONGLONG now = GetTickCount64();
            bool showError = errorFlash && (now - errorFlashTime < 1500);
            COLORREF borderCol = showError ? kFieldError : kFieldBorder;
            HPEN editBorderPen = CreatePen(PS_SOLID, 2, borderCol);
            SelectObject(mem, editBorderPen);
            SelectObject(mem, GetStockObject(NULL_BRUSH));
            RoundRect(mem, 48, 103, 312, 137, 6, 6);
            DeleteObject(editBorderPen);

            // Error message
            if (showError) {
                SetTextColor(mem, kFieldError);
                SelectObject(mem, hFontSmall ? hFontSmall : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
                RECT rcErr = { 50, 140, 310, 158 };
                DrawText(mem, "Incorrect password", -1, &rcErr, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            }

            // Buttons
            RECT rcOK, rcCancel;
            GetButtonRects(hWnd, &rcOK, &rcCancel);

            // OK Button (Accent)
            COLORREF okColor = hoverOK ? kAccentHover : kAccent;
            HBRUSH okBr = CreateSolidBrush(okColor);
            SelectObject(mem, okBr);
            SelectObject(mem, GetStockObject(NULL_PEN));
            RoundRect(mem, rcOK.left, rcOK.top, rcOK.right, rcOK.bottom, 8, 8);
            DeleteObject(okBr);

            SetTextColor(mem, RGB(255, 255, 255));
            SelectObject(mem, hFontNormal ? hFontNormal : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            DrawText(mem, "Verify", -1, &rcOK, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            // Cancel Button (Muted)
            COLORREF cancelColor = hoverCancel ? kBtnCancelHv : kBtnCancel;
            HBRUSH cancelBr = CreateSolidBrush(cancelColor);
            SelectObject(mem, cancelBr);
            RoundRect(mem, rcCancel.left, rcCancel.top, rcCancel.right, rcCancel.bottom, 8, 8);
            DeleteObject(cancelBr);

            SetTextColor(mem, kTextDim);
            DrawText(mem, "Cancel", -1, &rcCancel, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            // Blit
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, kTextPrimary);
            SetBkColor(hdcEdit, kFieldBg);
            if (hEditBrush) DeleteObject(hEditBrush);
            hEditBrush = CreateSolidBrush(kFieldBg);
            return (LRESULT)hEditBrush;
        }

        case WM_MOUSEMOVE: {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            RECT rcOK, rcCancel;
            GetButtonRects(hWnd, &rcOK, &rcCancel);

            bool newHoverOK = (mx >= rcOK.left && mx <= rcOK.right && my >= rcOK.top && my <= rcOK.bottom);
            bool newHoverCancel = (mx >= rcCancel.left && mx <= rcCancel.right && my >= rcCancel.top && my <= rcCancel.bottom);

            if (newHoverOK != hoverOK || newHoverCancel != hoverCancel) {
                hoverOK = newHoverOK;
                hoverCancel = newHoverCancel;
                InvalidateRect(hWnd, nullptr, FALSE);
            }

            // Track mouse leave
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE:
            if (hoverOK || hoverCancel) {
                hoverOK = false;
                hoverCancel = false;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONDOWN: {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            RECT rcOK, rcCancel;
            GetButtonRects(hWnd, &rcOK, &rcCancel);

            if (mx >= rcOK.left && mx <= rcOK.right && my >= rcOK.top && my <= rcOK.bottom) {
                SendMessage(hWnd, WM_COMMAND, ID_BTN_OK, 0);
            } else if (mx >= rcCancel.left && mx <= rcCancel.right && my >= rcCancel.top && my <= rcCancel.bottom) {
                SendMessage(hWnd, WM_COMMAND, ID_BTN_CANCEL, 0);
            }
            return 0;
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
                    errorFlash = true;
                    errorFlashTime = GetTickCount64();
                    SetWindowText(GetDlgItem(hWnd, ID_EDIT_PASS), "");
                    SetFocus(GetDlgItem(hWnd, ID_EDIT_PASS));
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
            } else if (id == ID_BTN_CANCEL) {
                passwordCorrect = false;
                DestroyWindow(hWnd);
            }
            break;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                passwordCorrect = false;
                DestroyWindow(hWnd);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
            if (hEditBrush) { DeleteObject(hEditBrush); hEditBrush = nullptr; }
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

bool PromptForPassword(HWND hParent) {
    passwordCorrect = false;
    inputPassword = "";
    errorFlash = false;
    hoverOK = false;
    hoverCancel = false;

    // Register class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = PasswordDlgProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hbrBackground = CreateSolidBrush(kDlgBg);
    wc.lpszClassName = "MicMuteS_PasswordDlg";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassEx(&wc);

    // Center on parent
    RECT rcParent;
    GetWindowRect(hParent, &rcParent);
    int w = 360, h = 220;
    int x = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;

    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "MicMuteS_PasswordDlg", nullptr,
        WS_POPUP,
        x, y, w, h,
        hParent, nullptr, GetModuleHandle(nullptr), nullptr
    );

    // Rounded corners (Win11)
    HMODULE hDwm = GetModuleHandle("dwmapi.dll");
    if (hDwm) {
        typedef HRESULT(WINAPI* DwmSetAttrFn)(HWND, DWORD, LPCVOID, DWORD);
        auto pSetAttr = (DwmSetAttrFn)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pSetAttr) {
            int cornerPref = 2; // DWMWCP_ROUND
            pSetAttr(hDlg, 33, &cornerPref, sizeof(cornerPref));
        }
    }

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    EnableWindow(hParent, FALSE);

    // Modal message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_QUIT) break;
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            if (IsChild(hDlg, msg.hwnd) || msg.hwnd == hDlg) {
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
