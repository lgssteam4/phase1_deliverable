#pragma once
#include "framework.h"

void UpdateContactList(void);

LRESULT CreateContactListWindow(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, RECT rt);
LRESULT DoubleClickContactListEventHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
