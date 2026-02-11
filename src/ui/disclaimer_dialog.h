#pragma once
#include <windows.h>

// Shows a custom dark-themed legal disclaimer dialog.
// Returns true if the user agreed, false if declined or closed.
bool ShowDisclaimerDialog(HWND hParent, const char* customText = nullptr);
