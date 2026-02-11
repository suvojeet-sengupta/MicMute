#include "ui/disclaimer_dialog.h"
#include "core/globals.h"
#include <string>

static bool disclaimerAgreed = false;
static bool dHoverAgree = false;
static bool dHoverDecline = false;
static const char* currentDisclaimerText = nullptr;

// Theme colors
static const COLORREF kDiscBg       = RGB(22, 22, 35);
static const COLORREF kDiscBorder   = RGB(45, 45, 65);
static const COLORREF kDiscText     = RGB(210, 210, 225);
static const COLORREF kDiscTextDim  = RGB(140, 140, 160);
static const COLORREF kDiscWarning  = RGB(245, 185, 60);
static const COLORREF kDiscAccent   = RGB(86, 140, 245);
static const COLORREF kDiscAccentHv = RGB(110, 160, 255);
static const COLORREF kDiscDecline  = RGB(50, 50, 68);
static const COLORREF kDiscDeclineHv= RGB(65, 65, 82);

static const char* kDisclaimerText =
    "Call recording laws vary by jurisdiction. In many "
    "areas, you must notify all parties that the call is "
    "being recorded.\n\n"
    "By enabling this feature, you acknowledge that you "
    "understand and will comply with all applicable local, "
    "state, and federal laws regarding call recording.\n\n"
    "Unauthorized recording may result in legal consequences.";

static void GetDisclaimerButtonRects(HWND hWnd, RECT* rcAgree, RECT* rcDecline) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int btnW = 120, btnH = 34, gap = 14;
    int totalW = btnW * 2 + gap;
    int startX = (rc.right - totalW) / 2;
    int btnY = rc.bottom - 54;

    rcAgree->left = startX; rcAgree->top = btnY;
    rcAgree->right = startX + btnW; rcAgree->bottom = btnY + btnH;

    rcDecline->left = startX + btnW + gap; rcDecline->top = btnY;
    rcDecline->right = startX + btnW * 2 + gap; rcDecline->bottom = btnY + btnH;
}

static void DrawWarningTriangle(HDC hdc, int cx, int cy) {
    // Triangle outline
    POINT pts[3] = {
        { cx, cy - 16 },
        { cx - 18, cy + 12 },
        { cx + 18, cy + 12 }
    };

    HPEN warnPen = CreatePen(PS_SOLID, 2, kDiscWarning);
    HPEN oldPen = (HPEN)SelectObject(hdc, warnPen);
    HBRUSH warnBr = CreateSolidBrush(RGB(60, 45, 15));
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, warnBr);

    Polygon(hdc, pts, 3);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(warnPen);
    DeleteObject(warnBr);

    // Exclamation mark
    SetTextColor(hdc, kDiscWarning);
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, hFontBold ? hFontBold : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
    RECT rcEx = { cx - 6, cy - 12, cx + 6, cy + 10 };
    DrawText(hdc, "!", -1, &rcEx, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}

static LRESULT CALLBACK DisclaimerDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);

            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

            // Background
            HBRUSH bgBr = CreateSolidBrush(kDiscBg);
            FillRect(mem, &rc, bgBr);
            DeleteObject(bgBr);

            // Border
            HPEN borderPen = CreatePen(PS_SOLID, 1, kDiscBorder);
            SelectObject(mem, borderPen);
            SelectObject(mem, GetStockObject(NULL_BRUSH));
            Rectangle(mem, 0, 0, rc.right, rc.bottom);
            DeleteObject(borderPen);

            SetBkMode(mem, TRANSPARENT);

            // Warning triangle icon
            DrawWarningTriangle(mem, rc.right / 2, 32);

            // Title
            SelectObject(mem, hFontBold ? hFontBold : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            SetTextColor(mem, kDiscWarning);
            RECT rcTitle = { 0, 52, rc.right, 74 };
            DrawText(mem, "Legal Disclaimer", -1, &rcTitle, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            // Disclaimer text (wrapped)
            SelectObject(mem, hFontSmall ? hFontSmall : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            SetTextColor(mem, kDiscText);
            RECT rcBody = { 30, 82, rc.right - 30, rc.bottom - 68 };
            if (currentDisclaimerText) {
                DrawText(mem, currentDisclaimerText, -1, &rcBody, DT_WORDBREAK | DT_LEFT);
            }

            // Buttons
            RECT rcAgree, rcDecline;
            GetDisclaimerButtonRects(hWnd, &rcAgree, &rcDecline);

            // Agree button (Accent)
            COLORREF agreeCol = dHoverAgree ? kDiscAccentHv : kDiscAccent;
            HBRUSH agreeBr = CreateSolidBrush(agreeCol);
            SelectObject(mem, agreeBr);
            SelectObject(mem, GetStockObject(NULL_PEN));
            RoundRect(mem, rcAgree.left, rcAgree.top, rcAgree.right, rcAgree.bottom, 8, 8);
            DeleteObject(agreeBr);

            SetTextColor(mem, RGB(255, 255, 255));
            SelectObject(mem, hFontNormal ? hFontNormal : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            DrawText(mem, "I Agree", -1, &rcAgree, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            // Decline button
            COLORREF declineCol = dHoverDecline ? kDiscDeclineHv : kDiscDecline;
            HBRUSH declineBr = CreateSolidBrush(declineCol);
            SelectObject(mem, declineBr);
            RoundRect(mem, rcDecline.left, rcDecline.top, rcDecline.right, rcDecline.bottom, 8, 8);
            DeleteObject(declineBr);

            SetTextColor(mem, kDiscTextDim);
            DrawText(mem, "Decline", -1, &rcDecline, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            RECT rcAgree, rcDecline;
            GetDisclaimerButtonRects(hWnd, &rcAgree, &rcDecline);

            bool newAgree = (mx >= rcAgree.left && mx <= rcAgree.right && my >= rcAgree.top && my <= rcAgree.bottom);
            bool newDecline = (mx >= rcDecline.left && mx <= rcDecline.right && my >= rcDecline.top && my <= rcDecline.bottom);

            if (newAgree != dHoverAgree || newDecline != dHoverDecline) {
                dHoverAgree = newAgree;
                dHoverDecline = newDecline;
                InvalidateRect(hWnd, nullptr, FALSE);
            }

            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE:
            if (dHoverAgree || dHoverDecline) {
                dHoverAgree = false;
                dHoverDecline = false;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONDOWN: {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            RECT rcAgree, rcDecline;
            GetDisclaimerButtonRects(hWnd, &rcAgree, &rcDecline);

            if (mx >= rcAgree.left && mx <= rcAgree.right && my >= rcAgree.top && my <= rcAgree.bottom) {
                disclaimerAgreed = true;
                DestroyWindow(hWnd);
            } else if (mx >= rcDecline.left && mx <= rcDecline.right && my >= rcDecline.top && my <= rcDecline.bottom) {
                disclaimerAgreed = false;
                DestroyWindow(hWnd);
            }
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                disclaimerAgreed = false;
                DestroyWindow(hWnd);
                return 0;
            }
            break;

        case WM_CLOSE:
            disclaimerAgreed = false;
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


bool ShowDisclaimerDialog(HWND hParent, const char* customText) {
    disclaimerAgreed = false;
    dHoverAgree = false;
    dHoverDecline = false;
    currentDisclaimerText = customText ? customText : kDisclaimerText;

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = DisclaimerDlgProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hbrBackground = CreateSolidBrush(kDiscBg);
    wc.lpszClassName = "MicMuteS_DisclaimerDlg";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassEx(&wc);

    // Center on parent
    RECT rcParent;
    GetWindowRect(hParent, &rcParent);
    int w = 420, h = 320;
    int x = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;

    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "MicMuteS_DisclaimerDlg", nullptr,
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
            int cornerPref = 2;
            pSetAttr(hDlg, 33, &cornerPref, sizeof(cornerPref));
        }
    }

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    EnableWindow(hParent, FALSE);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    return disclaimerAgreed;
}
