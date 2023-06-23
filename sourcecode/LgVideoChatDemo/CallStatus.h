#pragma once
#include "framework.h"

LRESULT CreateCallStatusWindow(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, RECT rt);
int WriteToCallStatusEditBox(const TCHAR* fmt, ...);