// =============================================================================
//  MicMute-S  |  Modern Recording Player  (fully custom-drawn, dark theme)
// =============================================================================
#include "ui/player_window.h"
#include "core/globals.h"
#include "core/resource.h"
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <mmsystem.h>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <mutex>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dwmapi.lib")

// ── Dimensions ──────────────────────────────────────────────────────────────
#define PW_WIDTH   560
#define PW_HEIGHT  520

// ── Sub-control ID for the hidden search edit ──────────────────────────────
#define IDC_EDIT_SEARCH  4001
#define IDC_TIMER_POS    4002

// ── Theme colours (derived from the global palette) ─────────────────────────
static COLORREF cBg;
static COLORREF cPanel;
static COLORREF cBorder;
static COLORREF cText;
static COLORREF cTextDim;
static COLORREF cAccent;
static COLORREF cAccentHover;
static COLORREF cLive;
static COLORREF cMuted;
static COLORREF cListHover;
static COLORREF cListSel;
static COLORREF cSeekBg;
static COLORREF cSeekFill;
static COLORREF cSearchBg;
static COLORREF cWaveHigh;
static COLORREF cWaveLow;

static void InitThemeColors() {
    cBg          = colorBg;                         // RGB(24,24,32)
    cPanel       = colorPanelBg;                    // RGB(22,22,30)
    cBorder      = colorPanelBorder;                // RGB(50,50,70)
    cText        = colorText;                       // RGB(245,245,250)
    cTextDim     = colorTextDim;                    // RGB(150,150,170)
    cAccent      = colorAccent;                     // RGB(98,130,255)
    cAccentHover = RGB(128, 158, 255);
    cLive        = colorLive;                       // green
    cMuted       = colorMuted;                      // red
    cListHover   = RGB(38, 38, 55);
    cListSel     = RGB(50, 55, 80);
    cSeekBg      = RGB(40, 40, 55);
    cSeekFill    = cAccent;
    cSearchBg    = RGB(32, 32, 44);
    cWaveHigh    = cAccent;
    cWaveLow     = RGB(55, 60, 90);
}

// ── Fonts (created once) ────────────────────────────────────────────────────
static HFONT fTitle   = nullptr;
static HFONT fNormal  = nullptr;
static HFONT fSmall   = nullptr;
static HFONT fMono    = nullptr;

static void InitFonts() {
    if (fTitle) return;
    fTitle  = CreateFont(-18, 0, 0, 0, FW_BOLD,  0, 0, 0, DEFAULT_CHARSET,
                         0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    fNormal = CreateFont(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                         0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    fSmall  = CreateFont(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                         0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    fMono   = CreateFont(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                         0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, "Consolas");
}

// ── Window handle ───────────────────────────────────────────────────────────
static HWND  hPlayerWnd     = nullptr;
static HWND  hSearchEdit    = nullptr;
static WNDPROC origEditProc = nullptr;

// ── Playback state ──────────────────────────────────────────────────────────
static std::string currentAudioPath;
static bool   isPlaying  = false;
static bool   isPaused   = false;
static DWORD  posCurMs   = 0;
static DWORD  posTotalMs = 0;
static float  volume     = 1.0f;     // 0..1

// ── Recording list ──────────────────────────────────────────────────────────
struct RecEntry {
    std::string path;
    std::string display;   // filename without extension
    std::string dateStr;   // parent folder name (date)
    DWORD       sizeKB;
};
static std::vector<RecEntry> recList;
static int  listScrollY  = 0;
static int  listSelIdx    = -1;
static int  listHoverIdx  = -1;
static std::mutex listMtx;

// ── Waveform visualiser (fake spectrum bars) ────────────────────────────────
#define WAVE_BARS 40
static float waveBars[WAVE_BARS] = {};
static float waveTargets[WAVE_BARS] = {};

// ── Hit-test zones (computed during paint) ──────────────────────────────────
enum HitZone { HZ_NONE=0, HZ_PLAY, HZ_STOP, HZ_PREV, HZ_NEXT,
               HZ_SEEK, HZ_VOL, HZ_SEARCH_BTN, HZ_LIST_ITEM, HZ_FOLDER };
static RECT rcPlay{}, rcStop{}, rcPrev{}, rcNext{};
static RECT rcSeek{}, rcVol{}, rcSearchBtn{}, rcListArea{}, rcFolderBtn{};
static HitZone hoverZone = HZ_NONE;
static bool seekDragging = false;
static bool volDragging  = false;

// ── Search state ────────────────────────────────────────────────────────────
static std::string searchQuery;
static std::mutex searchMtx;
static std::string searchStatus;

// ── Forward declarations ────────────────────────────────────────────────────
static void UpdatePosition();
static void PaintPlayer(HWND hWnd);
static void LoadRecordingList();
static void PlayFile(const std::string& path);

// ─────────────────────────── Helpers ─────────────────────────────────────────
static std::string FormatTime(DWORD ms) {
    int sec = (int)(ms / 1000);
    int m = sec / 60, s = sec % 60;
    char buf[16];
    sprintf_s(buf, "%d:%02d", m, s);
    return buf;
}

static void FillRoundRect(HDC hdc, RECT rc, int r, HBRUSH br) {
    HRGN rgn = CreateRoundRectRgn(rc.left, rc.top, rc.right+1, rc.bottom+1, r, r);
    FillRgn(hdc, rgn, br);
    DeleteObject(rgn);
}

static void DrawTextCentered(HDC hdc, const char* txt, RECT rc, HFONT font, COLORREF col) {
    SelectObject(hdc, font);
    SetTextColor(hdc, col);
    DrawTextA(hdc, txt, -1, &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
}

// ── GDI icon drawing helpers (replace broken UTF-8 emoji) ─────────────────
static void DrawPlayTriangle(HDC hdc, int cx, int cy, int sz, COLORREF col) {
    POINT pts[3] = {{cx - sz/3, cy - sz/2}, {cx - sz/3, cy + sz/2}, {cx + sz/2, cy}};
    HBRUSH br = CreateSolidBrush(col);
    HRGN rgn = CreatePolygonRgn(pts, 3, WINDING);
    FillRgn(hdc, rgn, br);
    DeleteObject(rgn); DeleteObject(br);
}

static void DrawPauseIcon(HDC hdc, int cx, int cy, int sz, COLORREF col) {
    HBRUSH br = CreateSolidBrush(col);
    int bw = max(2, sz / 5);
    int gap = max(2, sz / 4);
    RECT r1 = {cx - gap - bw, cy - sz/2, cx - gap, cy + sz/2};
    RECT r2 = {cx + gap, cy - sz/2, cx + gap + bw, cy + sz/2};
    FillRect(hdc, &r1, br); FillRect(hdc, &r2, br);
    DeleteObject(br);
}

static void DrawStopSquare(HDC hdc, int cx, int cy, int sz, COLORREF col) {
    HBRUSH br = CreateSolidBrush(col);
    int half = sz / 3;
    RECT r = {cx - half, cy - half, cx + half, cy + half};
    FillRect(hdc, &r, br);
    DeleteObject(br);
}

static void DrawPrevIcon(HDC hdc, int cx, int cy, int sz, COLORREF col) {
    HBRUSH br = CreateSolidBrush(col);
    // Bar on left
    int bw = max(2, sz / 6);
    RECT bar = {cx - sz/3 - bw, cy - sz/3, cx - sz/3, cy + sz/3};
    FillRect(hdc, &bar, br);
    // Triangle pointing left
    POINT pts[3] = {{cx + sz/3, cy - sz/3}, {cx + sz/3, cy + sz/3}, {cx - sz/3, cy}};
    HRGN rgn = CreatePolygonRgn(pts, 3, WINDING);
    FillRgn(hdc, rgn, br);
    DeleteObject(rgn); DeleteObject(br);
}

static void DrawNextIcon(HDC hdc, int cx, int cy, int sz, COLORREF col) {
    HBRUSH br = CreateSolidBrush(col);
    // Bar on right
    int bw = max(2, sz / 6);
    RECT bar = {cx + sz/3, cy - sz/3, cx + sz/3 + bw, cy + sz/3};
    FillRect(hdc, &bar, br);
    // Triangle pointing right
    POINT pts[3] = {{cx - sz/3, cy - sz/3}, {cx - sz/3, cy + sz/3}, {cx + sz/3, cy}};
    HRGN rgn = CreatePolygonRgn(pts, 3, WINDING);
    FillRgn(hdc, rgn, br);
    DeleteObject(rgn); DeleteObject(br);
}

static void DrawSearchLens(HDC hdc, int cx, int cy, int sz, COLORREF col) {
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HBRUSH nbr = (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectObject(hdc, pen); SelectObject(hdc, nbr);
    int r = sz / 3;
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
    MoveToEx(hdc, cx + r - 1, cy + r - 1, nullptr);
    LineTo(hdc, cx + sz/2, cy + sz/2);
    DeleteObject(pen);
}

static void DrawFolderIcon(HDC hdc, int cx, int cy, int sz, COLORREF col) {
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    HBRUSH nbr = (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectObject(hdc, pen); SelectObject(hdc, nbr);
    int hw = sz/2, hh = sz/3;
    // Folder body
    Rectangle(hdc, cx - hw, cy - hh + 2, cx + hw, cy + hh);
    // Tab
    MoveToEx(hdc, cx - hw, cy - hh + 2, nullptr);
    LineTo(hdc, cx - hw, cy - hh);
    LineTo(hdc, cx - hw/3, cy - hh);
    LineTo(hdc, cx, cy - hh + 2);
    DeleteObject(pen);
}

static void DrawSpeakerIcon(HDC hdc, int cx, int cy, int sz, COLORREF col) {
    HBRUSH br = CreateSolidBrush(col);
    // Speaker body (small rect)
    int bw = sz/5, bh = sz/3;
    RECT body = {cx - sz/3, cy - bh/2, cx - sz/3 + bw, cy + bh/2};
    FillRect(hdc, &body, br);
    // Cone (triangle)
    POINT pts[3] = {{cx - sz/3 + bw, cy - sz/3}, {cx - sz/3 + bw, cy + sz/3}, {cx + sz/6, cy}};
    // Actually let me make a wider cone
    POINT cone[4] = {{cx - sz/3 + bw, cy - bh/2}, {cx - sz/3 + bw, cy + bh/2}, {cx, cy + sz/3}, {cx, cy - sz/3}};
    HRGN rgn = CreatePolygonRgn(cone, 4, WINDING);
    FillRgn(hdc, rgn, br);
    DeleteObject(rgn); DeleteObject(br);
    // Sound waves
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    SelectObject(hdc, pen);
    SelectObject(hdc, (HBRUSH)GetStockObject(NULL_BRUSH));
    Arc(hdc, cx+2, cy-sz/4, cx+sz/3+2, cy+sz/4, cx+sz/4, cy-sz/4, cx+sz/4, cy+sz/4);
    DeleteObject(pen);
}

static void DrawMusicNote(HDC hdc, int cx, int cy, int sz, COLORREF col) {
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HBRUSH br = CreateSolidBrush(col);
    SelectObject(hdc, pen);
    // Stem
    MoveToEx(hdc, cx + 1, cy - sz/3, nullptr);
    LineTo(hdc, cx + 1, cy + sz/4);
    // Note head (small ellipse)
    SelectObject(hdc, br);
    Ellipse(hdc, cx - 2, cy + sz/6, cx + 3, cy + sz/3 + 1);
    DeleteObject(pen); DeleteObject(br);
}

// ─────────────────────────── MCI Wrappers ────────────────────────────────────
static void MCI_Close() {
    mciSendStringA("close myAudio", nullptr, 0, nullptr);
}

static DWORD MCI_GetLength() {
    char buf[64] = {};
    mciSendStringA("status myAudio length", buf, sizeof(buf), nullptr);
    return (DWORD)atoi(buf);
}

static DWORD MCI_GetPos() {
    char buf[64] = {};
    mciSendStringA("status myAudio position", buf, sizeof(buf), nullptr);
    return (DWORD)atoi(buf);
}

static void MCI_SetPos(DWORD ms) {
    char cmd[128];
    sprintf_s(cmd, "seek myAudio to %u", (unsigned)ms);
    mciSendStringA(cmd, nullptr, 0, nullptr);
}

static void MCI_SetVolume(int vol) {  // 0-1000
    char cmd[128];
    sprintf_s(cmd, "setaudio myAudio volume to %d", vol);
    mciSendStringA(cmd, nullptr, 0, nullptr);
}

static void MCI_Play(const std::string& path) {
    if (path.empty()) return;
    MCI_Close();
    std::string cmd = "open \"" + path + "\" type waveaudio alias myAudio";
    mciSendStringA(cmd.c_str(), nullptr, 0, nullptr);
    MCI_SetVolume((int)(volume * 1000));
    mciSendStringA("play myAudio notify", nullptr, 0, hPlayerWnd);
    posTotalMs = MCI_GetLength();
    isPlaying = true;
    isPaused  = false;
}

static void MCI_Pause() {
    mciSendStringA("pause myAudio", nullptr, 0, nullptr);
    isPaused  = true;
    isPlaying = false;
}

static void MCI_Resume() {
    mciSendStringA("resume myAudio", nullptr, 0, nullptr);
    isPaused  = false;
    isPlaying = true;
}

static void MCI_Stop() {
    mciSendStringA("stop myAudio", nullptr, 0, nullptr);
    MCI_Close();
    isPlaying  = false;
    isPaused   = false;
    posCurMs   = 0;
    posTotalMs = 0;
}

// ────────────────────── Recording list scan ──────────────────────────────────
static void LoadRecordingList() {
    std::vector<RecEntry> tmp;
    if (recordingFolder.empty()) return;
    try {
        for (auto& e : std::filesystem::recursive_directory_iterator(recordingFolder)) {
            if (!e.is_regular_file()) continue;
            auto ext = e.path().extension().string();
            // lowercase
            for (auto& c : ext) c = (char)tolower((unsigned char)c);
            if (ext != ".wav") continue;
            RecEntry r;
            r.path    = e.path().string();
            r.display = e.path().stem().string();
            r.dateStr = e.path().parent_path().filename().string();
            r.sizeKB  = (DWORD)(e.file_size() / 1024);
            tmp.push_back(std::move(r));
        }
    } catch (...) {}
    // Sort newest first (by filename / path descending)
    std::sort(tmp.begin(), tmp.end(), [](const RecEntry& a, const RecEntry& b) {
        return a.path > b.path;
    });
    std::lock_guard<std::mutex> lk(listMtx);
    recList = std::move(tmp);
}

// ────────────────────── Search ───────────────────────────────────────────────
static std::string FindRecordingByMetadata(const std::string& query) {
    if (query.empty() || recordingFolder.empty()) return "";
    // First: case-insensitive filename match anywhere in list
    {
        std::lock_guard<std::mutex> lk(listMtx);
        std::string ql = query;
        for (auto& c : ql) c = (char)tolower((unsigned char)c);
        for (auto& r : recList) {
            std::string dl = r.display;
            for (auto& c : dl) c = (char)tolower((unsigned char)c);
            if (dl.find(ql) != std::string::npos) return r.path;
        }
    }
    // Second: search inside .txt metadata files
    try {
        for (auto& entry : std::filesystem::recursive_directory_iterator(recordingFolder)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                std::ifstream file(entry.path());
                if (file.is_open()) {
                    std::stringstream buf;
                    buf << file.rdbuf();
                    if (buf.str().find(query) != std::string::npos) {
                        auto wav = entry.path(); wav.replace_extension(".wav");
                        if (std::filesystem::exists(wav)) return wav.string();
                    }
                }
            }
        }
    } catch (...) {}
    return "";
}

// ────────────────────── Play a file ─────────────────────────────────────────
static void PlayFile(const std::string& path) {
    MCI_Stop();
    currentAudioPath = path;
    if (!path.empty()) {
        MCI_Play(path);
        // highlight in list
        std::lock_guard<std::mutex> lk(listMtx);
        for (int i = 0; i < (int)recList.size(); i++) {
            if (recList[i].path == path) { listSelIdx = i; break; }
        }
    }
    if (hPlayerWnd) InvalidateRect(hPlayerWnd, nullptr, FALSE);
}

// ────────────────────── Navigate prev / next ────────────────────────────────
static void PlayPrev() {
    std::string p;
    {
        std::lock_guard<std::mutex> lk(listMtx);
        if (recList.empty()) return;
        listSelIdx = (listSelIdx <= 0) ? (int)recList.size()-1 : listSelIdx-1;
        p = recList[listSelIdx].path;
    }
    PlayFile(p);
}
static void PlayNext() {
    std::string p;
    {
        std::lock_guard<std::mutex> lk(listMtx);
        if (recList.empty()) return;
        listSelIdx = (listSelIdx+1 >= (int)recList.size()) ? 0 : listSelIdx+1;
        p = recList[listSelIdx].path;
    }
    PlayFile(p);
}

// ────────────────────── Waveform animation ──────────────────────────────────
static void UpdateWaveform() {
    for (int i = 0; i < WAVE_BARS; i++) {
        if (isPlaying) {
            waveTargets[i] = 0.15f + (float)(rand() % 85) / 100.0f;
        } else {
            waveTargets[i] = 0.05f;
        }
        waveBars[i] += (waveTargets[i] - waveBars[i]) * 0.25f;
    }
}

// ────────────────────── Timer tick ──────────────────────────────────────────
static void UpdatePosition() {
    if (isPlaying) {
        posCurMs = MCI_GetPos();
    }
    UpdateWaveform();
    if (hPlayerWnd) InvalidateRect(hPlayerWnd, nullptr, FALSE);
}

// ────────────────────── Subclassed Edit (Enter = search) ────────────────────
static LRESULT CALLBACK SearchEditProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        SendMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(IDC_EDIT_SEARCH + 999, 0), 0);
        return 0;
    }
    if (msg == WM_CHAR && wp == '\r') return 0; // suppress beep
    return CallWindowProc(origEditProc, hWnd, msg, wp, lp);
}

// ═══════════════════════════════════════════════════════════════════════════
//  PAINT — the entire window is custom-drawn
// ═══════════════════════════════════════════════════════════════════════════
static void PaintPlayer(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT wrc; GetClientRect(hWnd, &wrc);
    int W = wrc.right, H = wrc.bottom;

    // Double-buffer
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
    SelectObject(mem, bmp);
    SetBkMode(mem, TRANSPARENT);

    // ── Background ──
    HBRUSH brBg = CreateSolidBrush(cBg);
    FillRect(mem, &wrc, brBg);
    DeleteObject(brBg);

    int margin = 16;
    int y = margin;

    // ── Title bar area ──
    {
        // Draw a small play triangle as the title icon
        DrawPlayTriangle(mem, margin + 10, y + 14, 16, cAccent);
        SelectObject(mem, fTitle);
        SetTextColor(mem, cText);
        RECT rc = {margin + 24, y, W - margin, y + 28};
        DrawTextA(mem, "Recording Player", -1, &rc,
                  DT_SINGLELINE | DT_VCENTER);
        
        // Version tag
        SelectObject(mem, fSmall);
        SetTextColor(mem, cTextDim);
        RECT rv = {W - 120, y, W - margin, y + 28};
        DrawTextA(mem, APP_VERSION, -1, &rv, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
        y += 32;
    }

    // ── Separator ──
    {
        HPEN pen = CreatePen(PS_SOLID, 1, cBorder);
        SelectObject(mem, pen);
        MoveToEx(mem, margin, y, nullptr);
        LineTo(mem, W - margin, y);
        DeleteObject(pen);
        y += 8;
    }

    // ── Search bar ──
    int searchBarY = y;
    {
        // Background pill
        RECT sr = {margin, y, W - margin, y + 34};
        HBRUSH brS = CreateSolidBrush(cSearchBg);
        FillRoundRect(mem, sr, 8, brS);
        DeleteObject(brS);

        // Search icon (drawn lens)
        DrawSearchLens(mem, margin + 20, y + 17, 18, cTextDim);

        // The actual EDIT control is positioned over part of this pill
        if (hSearchEdit) {
            SetWindowPos(hSearchEdit, nullptr, margin + 32, y + 5, W - margin*2 - 110, 24,
                         SWP_NOZORDER);
        }

        // Search button
        int btnW = 68;
        rcSearchBtn = {W - margin - btnW, y + 2, W - margin - 2, y + 32};
        HBRUSH brBtn = CreateSolidBrush(hoverZone == HZ_SEARCH_BTN ? cAccentHover : cAccent);
        FillRoundRect(mem, rcSearchBtn, 6, brBtn);
        DeleteObject(brBtn);
        DrawTextCentered(mem, "Search", rcSearchBtn, fNormal, RGB(255,255,255));

        y += 42;
    }

    // ── Search status ──
    if (!searchStatus.empty()) {
        SelectObject(mem, fSmall);
        SetTextColor(mem, searchStatus.find("NOT") != std::string::npos ? cMuted : cLive);
        RECT rc = {margin, y, W - margin, y + 16};
        DrawTextA(mem, searchStatus.c_str(), -1, &rc, DT_SINGLELINE | DT_VCENTER);
        y += 20;
    }

    // ── Recording list ──
    int listH = 140;
    rcListArea = {margin, y, W - margin, y + listH};
    {
        // List background
        HBRUSH brList = CreateSolidBrush(cPanel);
        FillRoundRect(mem, rcListArea, 6, brList);
        DeleteObject(brList);

        // Border
        HPEN bp = CreatePen(PS_SOLID, 1, cBorder);
        HBRUSH nbr = (HBRUSH)GetStockObject(NULL_BRUSH);
        SelectObject(mem, bp); SelectObject(mem, nbr);
        RoundRect(mem, rcListArea.left, rcListArea.top, rcListArea.right, rcListArea.bottom, 6, 6);
        DeleteObject(bp);

        // Header
        int hy = y + 4;
        SelectObject(mem, fSmall);
        SetTextColor(mem, cTextDim);
        {
            RECT rh = {margin + 8, hy, margin + 250, hy + 16};
            DrawTextA(mem, "RECORDINGS", -1, &rh, DT_SINGLELINE | DT_VCENTER);
        }

        // Open folder button
        {
            int fbW = 22;
            rcFolderBtn = {W - margin - fbW - 6, hy, W - margin - 6, hy + 16};
            DrawFolderIcon(mem, (rcFolderBtn.left + rcFolderBtn.right)/2, (rcFolderBtn.top + rcFolderBtn.bottom)/2, 14, hoverZone == HZ_FOLDER ? cAccent : cTextDim);
        }

        // Count badge
        {
            std::lock_guard<std::mutex> lk(listMtx);
            char cnt[32]; sprintf_s(cnt, "(%d)", (int)recList.size());
            RECT rb = {margin + 100, hy, margin + 200, hy + 16};
            SetTextColor(mem, cTextDim);
            DrawTextA(mem, cnt, -1, &rb, DT_SINGLELINE | DT_VCENTER);
        }

        hy += 20;

        // Items
        HRGN clip = CreateRectRgn(rcListArea.left+1, hy, rcListArea.right-1, rcListArea.bottom-2);
        SelectClipRgn(mem, clip);

        int itemH = 28;
        {
            std::lock_guard<std::mutex> lk(listMtx);
            int visible = (rcListArea.bottom - hy) / itemH;
            int maxScroll = max(0, (int)recList.size() - visible);
            if (listScrollY > maxScroll) listScrollY = maxScroll;
            if (listScrollY < 0) listScrollY = 0;

            for (int i = listScrollY; i < (int)recList.size(); i++) {
                int iy = hy + (i - listScrollY) * itemH;
                if (iy >= rcListArea.bottom) break;

                RECT ir = {rcListArea.left + 2, iy, rcListArea.right - 2, iy + itemH};

                // Highlight
                if (i == listSelIdx) {
                    HBRUSH hb = CreateSolidBrush(cListSel);
                    FillRect(mem, &ir, hb);
                    DeleteObject(hb);
                } else if (i == listHoverIdx) {
                    HBRUSH hb = CreateSolidBrush(cListHover);
                    FillRect(mem, &ir, hb);
                    DeleteObject(hb);
                }

                // Icon (GDI drawn)
                {
                    COLORREF iconCol = (i == listSelIdx && isPlaying) ? cLive : cAccent;
                    int icx = ir.left + 16, icy = iy + itemH / 2;
                    if (i == listSelIdx && isPlaying)
                        DrawPlayTriangle(mem, icx, icy, 10, iconCol);
                    else
                        DrawMusicNote(mem, icx, icy, 12, iconCol);
                }

                // Name
                SetTextColor(mem, cText);
                SelectObject(mem, fNormal);
                RECT rn = {ir.left + 28, iy, ir.right - 90, iy + itemH};
                DrawTextA(mem, recList[i].display.c_str(), -1, &rn,
                          DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

                // Date + size
                SelectObject(mem, fSmall);
                SetTextColor(mem, cTextDim);
                char meta[64];
                sprintf_s(meta, "%s  %uKB", recList[i].dateStr.c_str(), recList[i].sizeKB);
                RECT rm = {ir.right - 140, iy, ir.right - 6, iy + itemH};
                DrawTextA(mem, meta, -1, &rm, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
            }
        }
        SelectClipRgn(mem, nullptr);
        DeleteObject(clip);

        y += listH + 8;
    }

    // ── Now Playing info ──
    {
        SelectObject(mem, fSmall);
        SetTextColor(mem, cTextDim);
        RECT rn = {margin, y, W - margin, y + 16};
        DrawTextA(mem, "NOW PLAYING", -1, &rn, DT_SINGLELINE | DT_VCENTER);
        y += 18;

        SelectObject(mem, fNormal);
        SetTextColor(mem, cText);
        std::string disp = currentAudioPath.empty()
            ? "No file loaded"
            : std::filesystem::path(currentAudioPath).stem().string();
        RECT rf = {margin, y, W - margin, y + 20};
        DrawTextA(mem, disp.c_str(), -1, &rf, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        y += 24;
    }

    // ── Waveform visualiser ──
    {
        int waveH = 36;
        RECT wr = {margin, y, W - margin, y + waveH};
        HBRUSH brW = CreateSolidBrush(RGB(28, 28, 38));
        FillRoundRect(mem, wr, 4, brW);
        DeleteObject(brW);

        int barW = (W - margin * 2 - WAVE_BARS) / WAVE_BARS;
        for (int i = 0; i < WAVE_BARS; i++) {
            int bx = margin + 2 + i * (barW + 1);
            int bh = (int)(waveBars[i] * (waveH - 6));
            if (bh < 2) bh = 2;
            int by = y + waveH - 3 - bh;

            // Gradient: use accent for tall bars, dim for short ones
            float t = waveBars[i];
            int r1 = GetRValue(cWaveLow), g1 = GetGValue(cWaveLow), b1 = GetBValue(cWaveLow);
            int r2 = GetRValue(cWaveHigh), g2 = GetGValue(cWaveHigh), b2 = GetBValue(cWaveHigh);
            COLORREF bc = RGB(r1 + (int)(t*(r2-r1)), g1 + (int)(t*(g2-g1)), b1 + (int)(t*(b2-b1)));
            HBRUSH bb = CreateSolidBrush(bc);
            RECT br2r = {bx, by, bx + barW, y + waveH - 3};
            FillRect(mem, &br2r, bb);
            DeleteObject(bb);
        }
        y += waveH + 6;
    }

    // ── Seek bar ──
    {
        rcSeek = {margin, y, W - margin, y + 8};
        // Background
        HBRUSH bg = CreateSolidBrush(cSeekBg);
        FillRoundRect(mem, rcSeek, 4, bg);
        DeleteObject(bg);

        // Progress fill
        float pct = (posTotalMs > 0) ? (float)posCurMs / posTotalMs : 0.0f;
        if (pct > 1.0f) pct = 1.0f;
        int fillW = (int)(pct * (rcSeek.right - rcSeek.left));
        if (fillW > 0) {
            RECT rf = {rcSeek.left, rcSeek.top, rcSeek.left + fillW, rcSeek.bottom};
            HBRUSH fb = CreateSolidBrush(cSeekFill);
            FillRoundRect(mem, rf, 4, fb);
            DeleteObject(fb);
        }
        // Thumb
        int tx = rcSeek.left + fillW;
        if (isPlaying || isPaused) {
            HBRUSH tb = CreateSolidBrush(RGB(255,255,255));
            RECT tr = {tx - 5, rcSeek.top - 3, tx + 5, rcSeek.bottom + 3};
            FillRoundRect(mem, tr, 5, tb);
            DeleteObject(tb);
        }

        y += 14;
        // Time labels
        SelectObject(mem, fMono);
        SetTextColor(mem, cTextDim);
        std::string tCur  = FormatTime(posCurMs);
        std::string tTot  = FormatTime(posTotalMs);
        RECT rl = {margin, y, margin + 60, y + 14};
        DrawTextA(mem, tCur.c_str(), -1, &rl, DT_SINGLELINE | DT_VCENTER);
        RECT rr = {W - margin - 60, y, W - margin, y + 14};
        DrawTextA(mem, tTot.c_str(), -1, &rr, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
        y += 20;
    }

    // ── Transport controls (centred) ──
    {
        int btnSz = 38;
        int playBtnSz = 48;
        int gap = 16;
        int totalW = btnSz + gap + btnSz + gap + playBtnSz + gap + btnSz + gap + btnSz;
        int startX = (W - totalW) / 2;
        int cy = y + playBtnSz / 2;

        // Prev ⏮
        rcPrev = {startX, cy - btnSz/2, startX + btnSz, cy + btnSz/2};
        {
            HBRUSH b = CreateSolidBrush(hoverZone == HZ_PREV ? cListHover : cPanel);
            FillRoundRect(mem, rcPrev, 6, b); DeleteObject(b);
            DrawPrevIcon(mem, (rcPrev.left+rcPrev.right)/2, (rcPrev.top+rcPrev.bottom)/2, 24, hoverZone == HZ_PREV ? cAccent : cText);
        }
        startX += btnSz + gap;

        // Stop ⏹
        rcStop = {startX, cy - btnSz/2, startX + btnSz, cy + btnSz/2};
        {
            HBRUSH b = CreateSolidBrush(hoverZone == HZ_STOP ? cListHover : cPanel);
            FillRoundRect(mem, rcStop, 6, b); DeleteObject(b);
            DrawStopSquare(mem, (rcStop.left+rcStop.right)/2, (rcStop.top+rcStop.bottom)/2, 24, hoverZone == HZ_STOP ? cMuted : cText);
        }
        startX += btnSz + gap;

        // Play/Pause ▶ / ⏸  (bigger)
        rcPlay = {startX, cy - playBtnSz/2, startX + playBtnSz, cy + playBtnSz/2};
        {
            HBRUSH b = CreateSolidBrush(hoverZone == HZ_PLAY ? cAccentHover : cAccent);
            FillRoundRect(mem, rcPlay, playBtnSz/2, b); DeleteObject(b);
            int pcx = (rcPlay.left+rcPlay.right)/2, pcy = (rcPlay.top+rcPlay.bottom)/2;
            if (isPlaying)
                DrawPauseIcon(mem, pcx, pcy, 22, RGB(255,255,255));
            else
                DrawPlayTriangle(mem, pcx + 2, pcy, 22, RGB(255,255,255));
        }
        startX += playBtnSz + gap;

        // Next ⏭
        rcNext = {startX, cy - btnSz/2, startX + btnSz, cy + btnSz/2};
        {
            HBRUSH b = CreateSolidBrush(hoverZone == HZ_NEXT ? cListHover : cPanel);
            FillRoundRect(mem, rcNext, 6, b); DeleteObject(b);
            DrawNextIcon(mem, (rcNext.left+rcNext.right)/2, (rcNext.top+rcNext.bottom)/2, 24, hoverZone == HZ_NEXT ? cAccent : cText);
        }
        startX += btnSz + gap;

        y += playBtnSz + 10;
    }

    // ── Volume bar ──
    {
        int volBarW = 120;
        int vx = (W - volBarW - 40) / 2;
        DrawSpeakerIcon(mem, vx + 10, y + 7, 16, cTextDim);

        rcVol = {vx + 30, y + 3, vx + 30 + volBarW, y + 9};
        HBRUSH vbg = CreateSolidBrush(cSeekBg);
        FillRoundRect(mem, rcVol, 3, vbg); DeleteObject(vbg);
        int vfill = (int)(volume * volBarW);
        RECT vfr = {rcVol.left, rcVol.top, rcVol.left + vfill, rcVol.bottom};
        HBRUSH vfb = CreateSolidBrush(cAccent);
        FillRoundRect(mem, vfr, 3, vfb); DeleteObject(vfb);

        // volume pct
        char vpct[16]; sprintf_s(vpct, "%d%%", (int)(volume * 100));
        RECT vpr = {rcVol.right + 6, y, rcVol.right + 50, y + 14};
        SetTextColor(mem, cTextDim);
        DrawTextA(mem, vpct, -1, &vpr, DT_SINGLELINE | DT_VCENTER);
    }

    BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hWnd, &ps);
}

// ═══════════════════════════════════════════════════════════════════════════
//  HIT TESTING
// ═══════════════════════════════════════════════════════════════════════════
static bool PtInR(RECT r, int x, int y) {
    POINT p = {x, y};
    return PtInRect(&r, p);
}

static HitZone HitTest(int x, int y) {
    if (PtInR(rcPlay, x, y))      return HZ_PLAY;
    if (PtInR(rcStop, x, y))      return HZ_STOP;
    if (PtInR(rcPrev, x, y))      return HZ_PREV;
    if (PtInR(rcNext, x, y))      return HZ_NEXT;
    if (PtInR(rcSearchBtn, x, y)) return HZ_SEARCH_BTN;
    if (PtInR(rcFolderBtn, x, y)) return HZ_FOLDER;
    // Seek — widen vertically for easier clicking
    {
        RECT sr = rcSeek; sr.top -= 6; sr.bottom += 6;
        if (PtInR(sr, x, y)) return HZ_SEEK;
    }
    {
        RECT vr = rcVol; vr.top -= 6; vr.bottom += 6;
        if (PtInR(vr, x, y)) return HZ_VOL;
    }
    if (PtInR(rcListArea, x, y)) return HZ_LIST_ITEM;
    return HZ_NONE;
}

// ═══════════════════════════════════════════════════════════════════════════
//  WINDOW PROCEDURE
// ═══════════════════════════════════════════════════════════════════════════
static LRESULT CALLBACK PlayerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE: {
        InitThemeColors();
        InitFonts();

        // Hidden edit control for search input
        hSearchEdit = CreateWindowExA(0, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 100, 24, hWnd, (HMENU)IDC_EDIT_SEARCH, GetModuleHandle(nullptr), nullptr);
        SendMessage(hSearchEdit, WM_SETFONT, (WPARAM)fNormal, TRUE);

        // Subclass to capture Enter key
        origEditProc = (WNDPROC)SetWindowLongPtr(hSearchEdit, GWLP_WNDPROC, (LONG_PTR)SearchEditProc);

        // Style the edit control for dark theme
        // (we'll handle WM_CTLCOLOREDIT)

        // Position timer for seek bar
        SetTimer(hWnd, IDC_TIMER_POS, 80, nullptr);

        // Load recordings in background
        std::thread([]() {
            LoadRecordingList();
            if (hPlayerWnd) PostMessage(hPlayerWnd, WM_USER + 10, 0, 0);
        }).detach();

        return 0;
    }

    case WM_TIMER:
        if (wParam == IDC_TIMER_POS) UpdatePosition();
        return 0;

    case WM_PAINT:
        PaintPlayer(hWnd);
        return 0;

    case WM_ERASEBKGND:
        return 1; // handled in WM_PAINT double-buffer

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, cText);
        SetBkColor(hdc, cSearchBg);
        static HBRUSH brEdit = nullptr;
        static COLORREF lastSearchBg = 0;
        if (!brEdit || lastSearchBg != cSearchBg) {
            if (brEdit) DeleteObject(brEdit);
            brEdit = CreateSolidBrush(cSearchBg);
            lastSearchBg = cSearchBg;
        }
        return (LRESULT)brEdit;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);

        if (seekDragging) {
            float pct = (float)(x - rcSeek.left) / (rcSeek.right - rcSeek.left);
            pct = max(0.0f, min(1.0f, pct));
            posCurMs = (DWORD)(pct * posTotalMs);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (volDragging) {
            float pct = (float)(x - rcVol.left) / (rcVol.right - rcVol.left);
            volume = max(0.0f, min(1.0f, pct));
            MCI_SetVolume((int)(volume * 1000));
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        HitZone hz = HitTest(x, y);
        if (hz != hoverZone) { hoverZone = hz; InvalidateRect(hWnd, nullptr, FALSE); }

        // List hover
        if (hz == HZ_LIST_ITEM) {
            int itemH = 28;
            int headerH = 24; // offset for header inside list
            int idx = listScrollY + (y - rcListArea.top - headerH) / itemH;
            std::lock_guard<std::mutex> lk(listMtx);
            if (idx >= 0 && idx < (int)recList.size() && idx != listHoverIdx) {
                listHoverIdx = idx;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        } else if (listHoverIdx >= 0) {
            listHoverIdx = -1;
            InvalidateRect(hWnd, nullptr, FALSE);
        }

        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hWnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        hoverZone = HZ_NONE;
        listHoverIdx = -1;
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
        HitZone hz = HitTest(x, y);

        if (hz == HZ_SEEK && posTotalMs > 0) {
            seekDragging = true;
            SetCapture(hWnd);
            float pct = (float)(x - rcSeek.left) / (rcSeek.right - rcSeek.left);
            pct = max(0.0f, min(1.0f, pct));
            posCurMs = (DWORD)(pct * posTotalMs);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (hz == HZ_VOL) {
            volDragging = true;
            SetCapture(hWnd);
            float pct = (float)(x - rcVol.left) / (rcVol.right - rcVol.left);
            volume = max(0.0f, min(1.0f, pct));
            MCI_SetVolume((int)(volume * 1000));
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        if (hz == HZ_PLAY) {
            if (isPlaying) MCI_Pause();
            else if (isPaused) MCI_Resume();
            else if (!currentAudioPath.empty()) MCI_Play(currentAudioPath);
            else {
                // Try playing first in list
                std::string p;
                {
                    std::lock_guard<std::mutex> lk(listMtx);
                    if (!recList.empty()) {
                        listSelIdx = 0;
                        p = recList[0].path;
                    }
                }
                if (!p.empty()) PlayFile(p);
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (hz == HZ_STOP)  { MCI_Stop(); InvalidateRect(hWnd, nullptr, FALSE); return 0; }
        if (hz == HZ_PREV)  { PlayPrev(); return 0; }
        if (hz == HZ_NEXT)  { PlayNext(); return 0; }

        if (hz == HZ_SEARCH_BTN) {
            SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_EDIT_SEARCH + 999, 0), 0);
            return 0;
        }
        if (hz == HZ_FOLDER) {
            if (!recordingFolder.empty())
                ShellExecuteA(nullptr, "open", recordingFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }

        if (hz == HZ_LIST_ITEM) {
            int itemH = 28;
            int headerH = 24;
            int idx = listScrollY + (y - rcListArea.top - headerH) / itemH;
            std::string p;
            {
                std::lock_guard<std::mutex> lk(listMtx);
                if (idx >= 0 && idx < (int)recList.size()) {
                    listSelIdx = idx;
                    p = recList[idx].path;
                }
            }
            if (!p.empty()) PlayFile(p);
            return 0;
        }
        return 0;
    }

    case WM_LBUTTONUP:
        if (seekDragging) {
            seekDragging = false;
            ReleaseCapture();
            MCI_SetPos(posCurMs);
            if (isPlaying || isPaused) {
                mciSendStringA("play myAudio notify", nullptr, 0, hPlayerWnd);
                if (isPaused) {
                    // Stay paused at new position
                    mciSendStringA("pause myAudio", nullptr, 0, nullptr);
                } else {
                    isPlaying = true;
                }
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (volDragging) {
            volDragging = false;
            ReleaseCapture();
            return 0;
        }
        return 0;

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        // Check if cursor is over list
        POINT pt; GetCursorPos(&pt); ScreenToClient(hWnd, &pt);
        if (PtInR(rcListArea, pt.x, pt.y)) {
            listScrollY -= delta / 40;
            if (listScrollY < 0) listScrollY = 0;
            {
                std::lock_guard<std::mutex> lk(listMtx);
                int itemH = 28;
                int headerH = 24;
                int visible = (rcListArea.bottom - rcListArea.top - headerH) / itemH;
                int maxScroll = max(0, (int)recList.size() - visible);
                if (listScrollY > maxScroll) listScrollY = maxScroll;
            }
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_EDIT_SEARCH + 999) {
            // Trigger search
            char buf[256] = {};
            GetWindowTextA(hSearchEdit, buf, 256);
            std::string query = buf;
            if (query.empty()) return 0;
            {
                std::lock_guard<std::mutex> lk(searchMtx);
                searchQuery = query;
            }
            searchStatus = "Searching...";
            InvalidateRect(hWnd, nullptr, FALSE);
            std::thread([hWnd, query]() {
                std::string result = FindRecordingByMetadata(query);
                PostMessage(hWnd, WM_USER + 1, 0, (LPARAM)new std::string(result));
            }).detach();
        }
        return 0;
    }

    case WM_USER + 1: { // search result
        std::string* p = (std::string*)lParam;
        if (p) {
            if (!p->empty()) {
                searchStatus = "Found!";
                PlayFile(*p);
            } else {
                searchStatus = "Recording NOT found.";
            }
            delete p;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_USER + 10: // list loaded
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case MM_MCINOTIFY:
        if (wParam == MCI_NOTIFY_SUCCESSFUL) {
            isPlaying = false; isPaused = false;
            // Auto-next
            PlayNext();
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_SPACE) {
            if (isPlaying) MCI_Pause();
            else if (isPaused) MCI_Resume();
            else if (!currentAudioPath.empty()) MCI_Play(currentAudioPath);
            else {
                // Play first in list (same as Play button)
                std::string p;
                {
                    std::lock_guard<std::mutex> lk(listMtx);
                    if (!recList.empty()) {
                        listSelIdx = 0;
                        p = recList[0].path;
                    }
                }
                if (!p.empty()) PlayFile(p);
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (wParam == VK_LEFT  && (GetKeyState(VK_CONTROL) & 0x8000)) { PlayPrev(); return 0; }
        if (wParam == VK_RIGHT && (GetKeyState(VK_CONTROL) & 0x8000)) { PlayNext(); return 0; }
        break;

    case WM_CLOSE:
        MCI_Stop();
        KillTimer(hWnd, IDC_TIMER_POS);
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_DESTROY: {
        KillTimer(hWnd, IDC_TIMER_POS);
        MCI_Stop();
        if (fTitle)  { DeleteObject(fTitle);  fTitle  = nullptr; }
        if (fNormal) { DeleteObject(fNormal); fNormal = nullptr; }
        if (fSmall)  { DeleteObject(fSmall);  fSmall  = nullptr; }
        if (fMono)   { DeleteObject(fMono);   fMono   = nullptr; }
        hSearchEdit = nullptr;
        hPlayerWnd  = nullptr;
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)lParam;
        mm->ptMinTrackSize = {440, 480};
        return 0;
    }

    } // switch
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ═══════════════════════════════════════════════════════════════════════════
//  PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════
void CreatePlayerWindow(HINSTANCE hInstance) {
    if (hPlayerWnd) return;

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = PlayerWndProc;
    wc.hInstance      = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // we paint everything
    wc.lpszClassName = "MicMuteS_Player";
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));

    RegisterClassEx(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - PW_WIDTH) / 2;
    int y = (screenH - PW_HEIGHT) / 2;

    hPlayerWnd = CreateWindowExA(
        WS_EX_APPWINDOW,
        "MicMuteS_Player", "MicMute-S  |  Recording Player",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        x, y, PW_WIDTH, PW_HEIGHT,
        nullptr, nullptr, hInstance, nullptr);

    if (hPlayerWnd) {
        // Dark title bar (Windows 10 1809+)
        BOOL useDark = TRUE;
        DwmSetWindowAttribute(hPlayerWnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/,
                              &useDark, sizeof(useDark));
    }
}

void ShowPlayerWindow() {
    if (!hPlayerWnd) {
        CreatePlayerWindow(GetModuleHandle(nullptr));
    }
    if (hPlayerWnd) {
        ShowWindow(hPlayerWnd, SW_SHOW);
        SetTimer(hPlayerWnd, IDC_TIMER_POS, 80, nullptr);
        SetForegroundWindow(hPlayerWnd);
        // Refresh list
        std::thread([]() {
            LoadRecordingList();
            if (hPlayerWnd) PostMessage(hPlayerWnd, WM_USER + 10, 0, 0);
        }).detach();
    }
}

void ClosePlayerWindow() {
    if (hPlayerWnd) {
        MCI_Stop();
        ShowWindow(hPlayerWnd, SW_HIDE);
    }
}

bool IsPlayerWindowVisible() {
    return hPlayerWnd && IsWindowVisible(hPlayerWnd);
}
