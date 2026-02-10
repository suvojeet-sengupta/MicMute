#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

void SaveOverlayPosition();
void LoadOverlayPosition(int* x, int* y);
void SaveMeterPosition();
void LoadMeterPosition(int* x, int* y, int* w, int* h);
void SaveSettings();
void LoadSettings();
void ManageStartup(bool enable);
bool IsStartupEnabled();
