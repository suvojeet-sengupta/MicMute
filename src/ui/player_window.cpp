#include "ui/player_window.h"
#include "core/globals.h"
#include "core/resource.h"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <mmsystem.h> // For PlaySound/mci
#include <iomanip>
#include <sstream>

#pragma comment(lib, "winmm.lib")

// Window Dimensions
#define PLAYER_WIDTH  450
#define PLAYER_HEIGHT 320

// Control IDs
#define IDC_EDIT_SEARCH  101
#define IDC_BTN_SEARCH   102
#define IDC_BTN_PLAY     103
#define IDC_BTN_PAUSE    104
#define IDC_BTN_STOP     105
#define IDC_LBL_STATUS   106
#define IDC_LBL_FILE     107
#define IDC_LBL_TIME     108

// Global handle
static HWND hPlayerWnd = nullptr;
static WNDPROC defaultEditProc = nullptr;

// State
static std::string currentAudioPath;
static bool isPlaying = false;
static bool isPaused = false;

// Helpers
static void UpdatePlayerUI();

// Search Logic
static std::string FindRecordingByMetadata(const std::string& query) {
    if (query.empty() || recordingFolder.empty()) return "";

    try {
        // Iterate over all date folders
        for (const auto& entry : std::filesystem::recursive_directory_iterator(recordingFolder)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                // Read file content
                std::ifstream file(entry.path());
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    std::string content = buffer.str();
                    
                    // Simple case-insensitive check? Or exact? 
                    // User said "VoxID/UCid enter krega". Let's do simple substring search.
                    if (content.find(query) != std::string::npos) {
                        // Found! Check for corresponding .wav
                        std::filesystem::path wavPath = entry.path();
                        wavPath.replace_extension(".wav");
                        
                        if (std::filesystem::exists(wavPath)) {
                            return wavPath.string();
                        }
                    }
                }
            }
        }
    } catch (...) {
        // Handle perm errors etc
    }
    return "";
}

// MCI Playback Wrapper
static void MCI_Play(const std::string& path) {
    if (path.empty()) return;

    // Close any existing
    mciSendString("close myAudio", nullptr, 0, nullptr);

    std::string cmdOpen = "open \"" + path + "\" type waveaudio alias myAudio";
    mciSendString(cmdOpen.c_str(), nullptr, 0, nullptr);

    mciSendString("play myAudio notify", nullptr, 0, hPlayerWnd);
    isPlaying = true;
    isPaused = false;
    UpdatePlayerUI();
}

static void MCI_Pause() {
    mciSendString("pause myAudio", nullptr, 0, nullptr);
    isPaused = true;
    isPlaying = false; // "Active" but not playing
    UpdatePlayerUI();
}

static void MCI_Resume() {
    mciSendString("resume myAudio", nullptr, 0, nullptr);
    isPaused = false;
    isPlaying = true;
    UpdatePlayerUI();
}

static void MCI_Stop() {
    mciSendString("stop myAudio", nullptr, 0, nullptr);
    mciSendString("close myAudio", nullptr, 0, nullptr);
    isPlaying = false;
    isPaused = false;
    UpdatePlayerUI();
}

static void UpdatePlayerUI() {
    if (!hPlayerWnd) return;

    HWND hPlay = GetDlgItem(hPlayerWnd, IDC_BTN_PLAY);
    HWND hPause = GetDlgItem(hPlayerWnd, IDC_BTN_PAUSE);
    HWND hStop = GetDlgItem(hPlayerWnd, IDC_BTN_STOP);
    HWND hFile = GetDlgItem(hPlayerWnd, IDC_LBL_FILE);

    bool hasFile = !currentAudioPath.empty();

    EnableWindow(hPlay, hasFile && (!isPlaying || isPaused));
    EnableWindow(hPause, hasFile && isPlaying);
    EnableWindow(hStop, hasFile && (isPlaying || isPaused));

    if (hasFile) {
        std::filesystem::path p(currentAudioPath);
        SetWindowText(hFile, p.filename().string().c_str());
    } else {
        SetWindowText(hFile, "No file loaded");
    }
}

static LRESULT CALLBACK PlayerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Background
            
            // 1. Search Label
            CreateWindowExA(0, "STATIC", "Enter VoxID / UCid:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 
                20, 20, 150, 24, hWnd, nullptr, nullptr, nullptr);

            // 2. Search Edit
            CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_VISIBLE | WS_CHILD | 
                ES_AUTOHSCROLL, 20, 50, 280, 28, hWnd, (HMENU)IDC_EDIT_SEARCH, nullptr, nullptr);

            // 3. Search Button
            CreateWindowExA(0, "BUTTON", "Search", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                310, 50, 100, 28, hWnd, (HMENU)IDC_BTN_SEARCH, nullptr, nullptr);

            // Separator line logic would be in Paint, keep simple for now

            // 4. File Info
            CreateWindowExA(0, "STATIC", "File Status", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 
                 20, 100, 400, 24, hWnd, (HMENU)IDC_LBL_STATUS, nullptr, nullptr);
            
            CreateWindowExA(0, "STATIC", "No file loaded", WS_VISIBLE | WS_CHILD | SS_CENTER | SS_ENDELLIPSIS, 
                 20, 130, 410, 40, hWnd, (HMENU)IDC_LBL_FILE, nullptr, nullptr);

            // 5. Controls
            int btnW = 100;
            int btnH = 40;
            int startX = (PLAYER_WIDTH - (btnW * 3 + 20)) / 2;
            int y = 200;

            CreateWindowExA(0, "BUTTON", "Play", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
                startX, y, btnW, btnH, hWnd, (HMENU)IDC_BTN_PLAY, nullptr, nullptr);

            CreateWindowExA(0, "BUTTON", "Pause", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
                startX + btnW + 10, y, btnW, btnH, hWnd, (HMENU)IDC_BTN_PAUSE, nullptr, nullptr);
            
            CreateWindowExA(0, "BUTTON", "Stop", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
                startX + (btnW + 10) * 2, y, btnW, btnH, hWnd, (HMENU)IDC_BTN_STOP, nullptr, nullptr);

            // Font setting
            EnumChildWindows(hWnd, [](HWND hChild, LPARAM) -> BOOL {
                SendMessage(hChild, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
                return TRUE;
            }, 0);

            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_BTN_SEARCH) {
                char buf[256];
                GetDlgItemText(hWnd, IDC_EDIT_SEARCH, buf, 256);
                std::string query = buf;
                
                SetDlgItemText(hWnd, IDC_LBL_STATUS, "Searching...");
                currentAudioPath = "";
                MCI_Stop();

                std::thread([hWnd, query]() {
                    std::string result = FindRecordingByMetadata(query);
                    PostMessage(hWnd, WM_USER + 1, 0, (LPARAM)new std::string(result));
                }).detach();
            }
            else if (id == IDC_BTN_PLAY) {
                if (isPaused) MCI_Resume();
                else MCI_Play(currentAudioPath);
            }
            else if (id == IDC_BTN_PAUSE) {
                MCI_Pause();
            }
            else if (id == IDC_BTN_STOP) {
                MCI_Stop();
            }
            break;
        }

        case WM_USER + 1: { // Search Result
            std::string* pRes = (std::string*)lParam;
            if (pRes) {
                if (!pRes->empty()) {
                    currentAudioPath = *pRes;
                    SetDlgItemText(hWnd, IDC_LBL_STATUS, "Result: Found!");
                } else {
                    currentAudioPath = "";
                    SetDlgItemText(hWnd, IDC_LBL_STATUS, "Result: Recording NOT found.");
                }
                UpdatePlayerUI();
                delete pRes;
            }
            break;
        }

        case MM_MCINOTIFY: {
            if (wParam == MCI_NOTIFY_SUCCESSFUL) {
                // Playback finished
                isPlaying = false;
                isPaused = false;
                UpdatePlayerUI();
            }
            break;
        }

        case WM_CLOSE:
            MCI_Stop();
            ShowWindow(hWnd, SW_HIDE);
            return 0; // Don't destroy, just hide
            
        case WM_CTLCOLORSTATIC: {
             HDC hdcStatic = (HDC)wParam;
             SetBkMode(hdcStatic, TRANSPARENT);
             return (LRESULT)GetStockObject(NULL_BRUSH); // Or white brush
        }
        
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void CreatePlayerWindow(HINSTANCE hInstance) {
    if (hPlayerWnd) return;

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = PlayerWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "MicMuteS_Player";
    
    if (!RegisterClassEx(&wc)) {
        OutputDebugString("MicMuteS: Player Class Registration Failed!\n");
    } else {
        OutputDebugString("MicMuteS: Player Class Registered.\n");
    }

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - PLAYER_WIDTH) / 2;
    int y = (screenH - PLAYER_HEIGHT) / 2;

    hPlayerWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, // Keep on top
        "MicMuteS_Player", "Recording Player",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x, y, PLAYER_WIDTH, PLAYER_HEIGHT,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hPlayerWnd) {
        OutputDebugString("MicMuteS: Player Window Creation Failed!\n");
        // GetLastError logic could be added here
        char buf[64];
        sprintf_s(buf, "Error: %d\n", GetLastError());
        OutputDebugString(buf);
    } else {
        OutputDebugString("MicMuteS: Player Window Created.\n");
    }
}

void ShowPlayerWindow() {
    if (hPlayerWnd) {
        ShowWindow(hPlayerWnd, SW_SHOW);
        SetForegroundWindow(hPlayerWnd);
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
