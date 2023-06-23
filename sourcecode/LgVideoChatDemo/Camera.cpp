#include <opencv2\highgui\highgui.hpp>
#include <opencv2\opencv.hpp>
#include "Camera.h"

static  cv::VideoCapture *Camera=NULL;
static  int init_values[2] = { cv::IMWRITE_JPEG_QUALITY,80 }; //default(95) 0-100
static  std::vector<int> param(&init_values[0], &init_values[0] + 2);
static  cv::Mat frame;

bool OpenCamera(void)
{
	if (Camera) return true;
	else Camera = new cv::VideoCapture();
	if (Camera)
	{
		Camera->open(0);
		if (!Camera->isOpened())
		{
			Camera->release();
			Camera = NULL;
			return false;
		}
		return true;

	}
	else return false;
}
bool CloseCamera(void)
{
	if (Camera)
	{
		Camera->release();
		Camera = NULL;
		return true;
	}
	return false;
}
bool GetCameraFrame(std::vector<uchar> &sendbuff)
{
	if (Camera)
	{
		Camera->read(frame);
		if (!frame.empty())
		{
			cv::rotate(frame, frame, cv::ROTATE_180);
			cv::imencode(".jpg", frame, sendbuff, param);
			return true;
		}
	}
    return false;
}
//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------