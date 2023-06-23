#include "CallHistory.h"

#include "eWID.h"

static HWND hWndCHist;

LRESULT CreateCallHistoryWindow(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, RECT rt)
{
	const LONG offset = 20;

	CreateWindow(_T("STATIC"),
		_T("Call History"),
		WS_VISIBLE | WS_CHILD,
		rt.left, rt.top, rt.right, offset,
		hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

	hWndCHist = CreateWindow(_T("edit"), NULL,
		WS_CHILD | WS_BORDER | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_READONLY,
		rt.left, rt.top + offset, rt.right, rt.bottom - offset,
		hWnd, (HMENU)IDC_CALL_HISTORY, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

	return 0;
}

int WriteToCallHistoryEditBox(const TCHAR* fmt, ...)
{
	va_list argptr;
	TCHAR buffer[2048];
	int cnt;

	int iEditTextLength;
	HWND hWnd = hWndCHist;

	if (NULL == hWnd) return 0;

	va_start(argptr, fmt);

	cnt = _vsntprintf_s(buffer, _countof(buffer), _TRUNCATE, fmt, argptr);

	va_end(argptr);

	iEditTextLength = GetWindowTextLength(hWnd);
	if (iEditTextLength + cnt > 30000)       // edit text max length is 30000
	{
		SendMessage(hWnd, EM_SETSEL, iEditTextLength - 10000, iEditTextLength);
		SendMessage(hWnd, EM_REPLACESEL, 0, (LPARAM)_T(""));
		iEditTextLength -= 10000;
	}
	SendMessage(hWnd, EM_SETSEL, 0, 0);
	SendMessage(hWnd, EM_REPLACESEL, 0, (LPARAM)buffer);
	return(cnt);
}