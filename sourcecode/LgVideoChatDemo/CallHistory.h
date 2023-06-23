#pragma once
#include "framework.h"

LRESULT CreateCallHistoryWindow(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, RECT rt);
int WriteToCallHistoryEditBox(const TCHAR* fmt, ...);