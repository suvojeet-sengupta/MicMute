#pragma once
#include <windows.h>
#include <string>

// UI Constants
#define SIDEBAR_WIDTH 120
#define SIDEBAR_ITEM_HEIGHT 50
#define TOGGLE_WIDTH 40
#define TOGGLE_HEIGHT 20

// Drawing Helpers
void DrawSidebar(HDC hdc, RECT clientRect, int selectedTab);
void DrawSidebarItem(HDC hdc, int index, const char* text, int selectedTab, int hoverTab);
void DrawToggle(LPDRAWITEMSTRUCT lpDrawItem);
void DrawRoundedRect(HDC hdc, RECT rc, int radius, COLORREF color);
