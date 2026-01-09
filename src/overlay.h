#pragma once
#include <windows.h>

void CreateOverlayWindow(HINSTANCE hInstance);
void CreateMeterWindow(HINSTANCE hInstance);
void UpdateOverlay();
void UpdateMeter();

LRESULT CALLBACK OverlayWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK MeterWndProc(HWND, UINT, WPARAM, LPARAM);
