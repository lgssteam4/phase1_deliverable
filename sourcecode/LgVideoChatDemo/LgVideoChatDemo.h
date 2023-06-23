#pragma once

#include "resource.h"

#define WM_CLIENT_LOST         WM_USER+1
#define WM_REMOTE_CONNECT      WM_USER+2
#define WM_REMOTE_LOST         WM_USER+3
#define WM_VAD_STATE           WM_USER+4
#define WM_MISSEDCALL          WM_USER+5

#define VIDEO_PORT       10000
#define VOIP_LOCAL_PORT  10001
#define VOIP_REMOTE_PORT 10001
#define VIDEO_FRAME_DELAY 100

extern HWND hWndMain;
extern TVoipAttr VoipAttr;
extern char LocalIpAddress[512];

//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------