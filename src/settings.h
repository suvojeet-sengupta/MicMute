#pragma once
#include <windows.h>

void SaveOverlayPosition();
void LoadOverlayPosition(int* x, int* y);
void SaveMeterPosition();
void LoadMeterPosition(int* x, int* y);
void SaveSettings();
void LoadSettings();
void ManageStartup(bool enable);
bool IsStartupEnabled();
