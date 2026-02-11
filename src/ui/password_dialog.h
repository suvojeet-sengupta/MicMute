#pragma once
#include <windows.h>

// Prompts the user for the password. Returns true if correct, false if cancelled or incorrect.
bool PromptForPassword(HWND hParent);
