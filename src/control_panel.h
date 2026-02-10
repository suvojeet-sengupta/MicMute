#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>

// Control Panel Window
void CreateControlPanel(HINSTANCE hInstance);
void UpdateControlPanel();
void SetControlPanelSavedStatus(const std::string& filename);
void LayoutControlPanel(HWND hWnd);
LRESULT CALLBACK ControlPanelWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Position persistence
void SaveControlPanelPosition();
void LoadControlPanelPosition(int* x, int* y, int* w, int* h);

// Panel size calculation
void GetPanelDimensions(int mode, float scale, int* outW, int* outH);
