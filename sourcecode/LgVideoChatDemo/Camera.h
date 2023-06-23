#pragma once
#include <vector>
#include <uchar.h>
bool OpenCamera(void);
bool GetCameraFrame(std::vector<uchar>& sendbuff);
bool CloseCamera(void);
//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------