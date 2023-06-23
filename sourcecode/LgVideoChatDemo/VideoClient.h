#pragma once
bool ConnectToSever(const char* remotehostname, unsigned short remoteport);
bool StartVideoClient(void);
bool StopVideoClient(void);
bool IsVideoClientRunning(void);
//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------