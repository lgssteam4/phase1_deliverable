#include "CallStatus.h"

#include "eWID.h"

static HWND hWndCStat;

LRESULT CreateCallStatusWindow(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, RECT rt)
{
	const LONG offset = 20;

	CreateWindow(_T("STATIC"),
		_T("Call Status"),
		WS_VISIBLE | WS_CHILD,
		rt.left, rt.top, rt.right, offset,
		hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

	hWndCStat = CreateWindow(_T("edit"), NULL,
		WS_CHILD | WS_BORDER | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_READONLY,
		rt.left, rt.top + offset, rt.right, rt.bottom - offset,
		hWnd, (HMENU)IDC_CALL_HISTORY, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

	//WriteToCallStatusEditBox(_T("[ Waiting... ]"));

	return 0;
}

int WriteToCallStatusEditBox(const TCHAR* fmt, ...)
{
	va_list argptr;
	TCHAR buffer[2048];
	int cnt;

	HWND hWnd = hWndCStat;

	if (NULL == hWnd) return 0;

	va_start(argptr, fmt);
	cnt = wvsprintf(buffer, fmt, argptr);
	va_end(argptr);

	SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)buffer);
	return(cnt);
}