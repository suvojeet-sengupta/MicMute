#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

void RegisterDeveloperOptionsClass(HINSTANCE hInstance);
void CreateDeveloperOptionsWindow(HINSTANCE hInstance, HWND hParent);
