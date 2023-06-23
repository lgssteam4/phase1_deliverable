#include <opencv2\highgui\highgui.hpp>
#include <opencv2\opencv.hpp>
#include <windows.h>
#include "DisplayImage.h"

static HDC hdc;
static HWND hWindowMain;
static BITMAPINFO BitmapInfo;
static bool SetupBitMapInfoSet = false;
static RECT rt;

static void SetupBitMapInfo(BITMAPINFO* BitmapInfo, cv::Mat* frame);

static void SetupBitMapInfo(BITMAPINFO* BitmapInfo, cv::Mat* frame)
{
    int depth = frame->depth();
    int channels = frame->channels();
    int width = frame->cols;
    int height = frame->rows;

    unsigned int pixelSize = (8 << (depth / 2)) * channels; // pixelSize >= 8
    unsigned long bmplineSize = ((width * pixelSize + 31) >> 5) << 2;   // 

    BitmapInfo->bmiHeader.biSize = 40;
    BitmapInfo->bmiHeader.biWidth = width;
    BitmapInfo->bmiHeader.biHeight = height;
    BitmapInfo->bmiHeader.biPlanes = 1;
    BitmapInfo->bmiHeader.biBitCount = pixelSize;
    BitmapInfo->bmiHeader.biCompression = 0;
    BitmapInfo->bmiHeader.biSizeImage = height * bmplineSize;
    BitmapInfo->bmiHeader.biXPelsPerMeter = 0;
    BitmapInfo->bmiHeader.biYPelsPerMeter = 0;
    BitmapInfo->bmiHeader.biClrUsed = 0;
    BitmapInfo->bmiHeader.biClrImportant = 0;
    memset(&BitmapInfo->bmiColors, 0, sizeof(BitmapInfo->bmiColors));
}


bool InitializeImageDisplay(HWND hWndMain)
{
    hWindowMain = hWndMain;
    hdc = GetDC(hWindowMain);
    SetStretchBltMode(hdc, COLORONCOLOR);
    SetupBitMapInfoSet = false;
    return true;
}
bool DispayImage(cv::Mat & ImageIn)
{
    const LONG offset = 10;
 GetClientRect(hWindowMain, &rt);
 if (!ImageIn.empty())
   {
     if (!SetupBitMapInfoSet)
       {
        SetupBitMapInfo(&BitmapInfo, &ImageIn);
        SetupBitMapInfoSet = true;
       }
     StretchDIBits(hdc,
        //rt.left + 5, rt.top + 100, (rt.right - rt.left) /2, (rt.bottom - rt.top) / 2,
         rt.left + offset, rt.top - offset, rt.right - 2*offset, rt.bottom,
        0, 0, BitmapInfo.bmiHeader.biWidth, BitmapInfo.bmiHeader.biHeight,
        ImageIn.data, &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);  //DIB_RGB_COLORS
     return true;
   }
  return false;
}
//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------