#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>

// Global Recorder Window Handle
extern HWND hRecorderWnd;

// Functions
void CreateRecorderWindow(HINSTANCE hInstance);
void ShowRecorderWindow(bool show);
void InitRecorder();
void InitRecorder();
void CleanupRecorder();
bool EnsureRecordingFolderSelected(HWND parent);

// Window Procedure
LRESULT CALLBACK RecorderWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Auto-record notification
void NotifyAutoRecordSaved(const std::string& filename);

// Error Notification
#define WM_APP_RECORDING_ERROR (WM_APP + 5)

// State Saving
void SaveRecorderPosition();

