#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Create the player window (called at startup or lazily)
void CreatePlayerWindow(HINSTANCE hInstance);

// Show the player window (called when button is clicked)
void ShowPlayerWindow();

// Close/Hide the player window
void ClosePlayerWindow();

// Check if player window is open
bool IsPlayerWindowVisible();
