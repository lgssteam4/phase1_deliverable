#include "BoostLog.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include < cstdlib >
#include <opencv2\highgui\highgui.hpp>
#include <opencv2\opencv.hpp>
#include "VideoClient.h"
#include "VoipVoice.h"
#include "LgVideoChatDemo.h"
#include "Camera.h"
#include "TcpSendRecv.h"
#include "DisplayImage.h"
#include "ApplyOpenSSL.h"

enum InputMode { ImageSize, Image };
static  std::vector<uchar> sendbuff;//buffer for coding
static HANDLE hClientEvent = INVALID_HANDLE_VALUE;
static HANDLE hEndVideoClientEvent = INVALID_HANDLE_VALUE;
static HANDLE hTimer = INVALID_HANDLE_VALUE;
static SOCKET Client = INVALID_SOCKET;
static SOCKET SSLClient = INVALID_SOCKET;
SSL_CTX* ctxForClient = NULL;
SSL* SSLSocketForClient = NULL;
static cv::Mat ImageIn;
static DWORD ThreadVideoClientID;
static HANDLE hThreadVideoClient = INVALID_HANDLE_VALUE;

static DWORD WINAPI ThreadVideoClient(LPVOID ivalue);
static void VideoClientSetExitEvent(void);
static void VideoClientCleanup(void);

static void VideoClientSetExitEvent(void)
{
	if (hEndVideoClientEvent != INVALID_HANDLE_VALUE)
		SetEvent(hEndVideoClientEvent);
}

static void VideoClientCleanup(void)
{
	BOOST_LOG_TRIVIAL(info) << "VideoClientCleanup";

	if (hClientEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hClientEvent);
		hClientEvent = INVALID_HANDLE_VALUE;
	}
	if (hEndVideoClientEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hEndVideoClientEvent);
		hEndVideoClientEvent = INVALID_HANDLE_VALUE;
	}
	if (hTimer != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hTimer);
		hTimer = INVALID_HANDLE_VALUE;
	}
	if (Client != INVALID_SOCKET)
	{
		closesocket(Client);
		Client = INVALID_SOCKET;
		if (SSLClient != INVALID_SOCKET)
		{
			SSLClient = INVALID_SOCKET;
			SSL_free(SSLSocketForClient);
			SSL_CTX_free(ctxForClient);
		}
	}
}

bool ConnectToSever(const char* remotehostname, unsigned short remoteport)
{
	BOOST_LOG_TRIVIAL(debug) << "Client: Start ConnectToserver";

	int iResult;
	struct addrinfo   hints;
	struct addrinfo* result = NULL;
	char remoteportno[128];

	sprintf_s(remoteportno, sizeof(remoteportno), "%d", remoteport);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Get server address information
	iResult = getaddrinfo(remotehostname, remoteportno, &hints, &result);
	if (iResult != 0)
	{
		BOOST_LOG_TRIVIAL(error) << "getaddrinfo: Failed";
		return false;
	}

	if (result == NULL)
	{
		BOOST_LOG_TRIVIAL(error) << "getaddrinfo: Failed";
		return false;
	}

	// Create client socket
	if ((Client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		freeaddrinfo(result);
		BOOST_LOG_TRIVIAL(error) << "video client socket() failed with error " << WSAGetLastError();
		return false;
	}
	BOOST_LOG_TRIVIAL(debug) << "Client: Success socket";

	// Connect to server.
	iResult = connect(Client, result->ai_addr, (int)result->ai_addrlen);
	freeaddrinfo(result);
	if (iResult == SOCKET_ERROR) {
		BOOST_LOG_TRIVIAL(error) << "connect function failed with error : " << WSAGetLastError();
		iResult = closesocket(Client);
		Client = INVALID_SOCKET;
		if (iResult == SOCKET_ERROR)
			BOOST_LOG_TRIVIAL(error) << "closesocket function failed with error :" << WSAGetLastError();
		return false;
	}
	BOOST_LOG_TRIVIAL(debug) << "Client: Success connect";

	// Initialize SSL
	initializeSSL();
	BOOST_LOG_TRIVIAL(debug) << "Client: Success initializeSSL";

	// Create and initialize SSL context
	ctxForClient = createSSLContextForClient();
	if (ctxForClient == NULL)
	{
		BOOST_LOG_TRIVIAL(error) << "Client: Error createSSLContextForClient";
		iResult = closesocket(Client);
		Client = INVALID_SOCKET;
		if (iResult == SOCKET_ERROR)
			BOOST_LOG_TRIVIAL(error) << "closesocket function failed with error :" << WSAGetLastError();
		return false;
	}
	BOOST_LOG_TRIVIAL(debug) << "Client: Success createSSLContextForClient";

	// Create SSL socket
	SSLSocketForClient = SSL_new(ctxForClient);
	if (SSLSocketForClient == NULL)
	{
		BOOST_LOG_TRIVIAL(error) << "Client: Error SSL_new";
		iResult = closesocket(Client);
		Client = INVALID_SOCKET;
		SSL_CTX_free(ctxForClient);
		if (iResult == SOCKET_ERROR)
			BOOST_LOG_TRIVIAL(error) << "closesocket function failed with error :" << WSAGetLastError();
		return false;
	}
	BOOST_LOG_TRIVIAL(debug) << "Client: Success SSL_new";

	// Connect a client socket to the SSL socket.
	int fd = SSL_set_fd(SSLSocketForClient, Client);
	if (fd != 1)
	{
		BOOST_LOG_TRIVIAL(error) << "Client: Error SSL_set_fd";
		closesocket(Client);
		Client = INVALID_SOCKET;
		SSL_free(SSLSocketForClient);
		SSL_CTX_free(ctxForClient);
		return false;
	}
	BOOST_LOG_TRIVIAL(debug) << "Client: Success SSL_set_fd";
	SSLClient = SSL_get_fd(SSLSocketForClient);

	// Verify certification
	//SSL_CTX_set_verify(ctxForClient, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	//BOOST_LOG_TRIVIAL(debug) << "Client: Success SSL_CTX_set_verify";

	// Reconnect with the server using SSL socket
	if (SSL_connect(SSLSocketForClient) != 1)
	{
		BOOST_LOG_TRIVIAL(error) << "Client: Error SSL_connect";
		closesocket(Client);
		Client = INVALID_SOCKET;
		SSL_free(SSLSocketForClient);
		SSL_CTX_free(ctxForClient);
		SSLClient = INVALID_SOCKET;
		return false;
	}
	BOOST_LOG_TRIVIAL(debug) << "Client: Success SSL_connect";
	BOOST_LOG_TRIVIAL(debug) << "Client: End ConnectToserver";

	return true;
}

bool StartVideoClient(void)
{
	hThreadVideoClient = CreateThread(NULL, 0, ThreadVideoClient, NULL, 0, &ThreadVideoClientID);
	return true;
}

bool StopVideoClient(void)
{
	VideoClientSetExitEvent();
	if (hThreadVideoClient != INVALID_HANDLE_VALUE)
	{
		WaitForSingleObject(hThreadVideoClient, INFINITE);
		CloseHandle(hThreadVideoClient);
		hThreadVideoClient = INVALID_HANDLE_VALUE;
	}
	return true;
}

bool IsVideoClientRunning(void)
{
	if (hThreadVideoClient == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	else return true;
}

static DWORD WINAPI ThreadVideoClient(LPVOID ivalue)
{
	BOOST_LOG_TRIVIAL(debug) << "Client: Start ThreadVideoClient";

	HANDLE ghEvents[3]{};
	int NumEvents;
	int iResult;
	DWORD dwEvent;
	LARGE_INTEGER liDueTime{};
	InputMode Mode = ImageSize;
	unsigned int InputBytesNeeded = sizeof(unsigned int);
	unsigned int SizeofImage;
	char* InputBuffer = NULL;
	char* InputBufferWithOffset = NULL;
	unsigned int CurrentInputBufferSize = 1024 * 10;

	InputBuffer = (char*)std::realloc(InputBuffer, CurrentInputBufferSize);
	InputBufferWithOffset = InputBuffer;

	if (InputBuffer == NULL)
	{
		BOOST_LOG_TRIVIAL(error) << "InputBuffer Realloc failed";
		return 1;
	}

	liDueTime.QuadPart = 0LL;

	// Create timer
	hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

	if (NULL == hTimer)
	{
		BOOST_LOG_TRIVIAL(error) << "CreateWaitableTimer failed " << GetLastError();
		return 2;
	}

	// Set timer
	if (!SetWaitableTimer(hTimer, &liDueTime, VIDEO_FRAME_DELAY, NULL, NULL, 0))
	{
		BOOST_LOG_TRIVIAL(error) << "SetWaitableTimer failed  " << GetLastError();
		return 3;
	}

	// Create a client event handle
	hClientEvent = WSACreateEvent();

	// Create a client end event handle
	hEndVideoClientEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// Connect socket and event handle (specify to watch for FD_READ and FD_CLOSE events)
	if (WSAEventSelect(SSLClient, hClientEvent, FD_READ | FD_CLOSE) == SOCKET_ERROR)
	{
		BOOST_LOG_TRIVIAL(error) << "WSAEventSelect() failed with error " << WSAGetLastError();
		iResult = closesocket(Client);
		Client = INVALID_SOCKET;
		SSLClient = INVALID_SOCKET;
		SSL_free(SSLSocketForClient);
		SSL_CTX_free(ctxForClient);
		if (iResult == SOCKET_ERROR)
			BOOST_LOG_TRIVIAL(error) << "closesocket function failed with error : " << WSAGetLastError();
		return 4;
	}

	ghEvents[0] = hEndVideoClientEvent;
	ghEvents[1] = hClientEvent;
	ghEvents[2] = hTimer;
	NumEvents = 3;

	while (1) {
		dwEvent = WaitForMultipleObjects(
			NumEvents,		// number of objects in array
			ghEvents,		// array of objects
			FALSE,			// wait for any object
			INFINITE);		// INFINITE) wait

		// Client end event
		if (dwEvent == WAIT_OBJECT_0) break;
		// Handling network events on client socket
		else if (dwEvent == WAIT_OBJECT_0 + 1)
		{
			WSANETWORKEVENTS NetworkEvents;
			// Get network events on client socket
			if (SOCKET_ERROR == WSAEnumNetworkEvents(SSLClient, hClientEvent, &NetworkEvents))
			{
				BOOST_LOG_TRIVIAL(error) << "WSAEnumNetworkEvent: " << WSAGetLastError() << "dwEvent " << dwEvent << " lNetworkEvent " << std::hex << NetworkEvents.lNetworkEvents;
				NetworkEvents.lNetworkEvents = 0;
			}
			else
			{
				if (NetworkEvents.lNetworkEvents & FD_READ)
				{
					if (NetworkEvents.iErrorCode[FD_READ_BIT] != 0)
					{
						BOOST_LOG_TRIVIAL(error) << "FD_READ failed with error " << NetworkEvents.iErrorCode[FD_READ_BIT];
					}
					else
					{
						int iResult;
						iResult = SSLReadDataTcpNoBlock(SSLSocketForClient, (unsigned char*)InputBufferWithOffset, InputBytesNeeded);
						if (iResult != SOCKET_ERROR)
						{
							if (iResult == 0)
							{
								Mode = ImageSize;
								InputBytesNeeded = sizeof(unsigned int);
								InputBufferWithOffset = InputBuffer;
								PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
								BOOST_LOG_TRIVIAL(error) << "Connection closed on Recv";
								break;
							}
							else
							{
								InputBytesNeeded -= iResult;
								InputBufferWithOffset += iResult;
								if (InputBytesNeeded == 0)
								{
									if (Mode == ImageSize)
									{
										Mode = Image;
										InputBufferWithOffset = InputBuffer;;
										memcpy(&SizeofImage, InputBuffer, sizeof(SizeofImage));
										SizeofImage = ntohl(SizeofImage);
										InputBytesNeeded = SizeofImage;
										if (InputBytesNeeded > CurrentInputBufferSize)
										{
											CurrentInputBufferSize = InputBytesNeeded + (10 * 1024);
											InputBuffer = (char*)std::realloc(InputBuffer, CurrentInputBufferSize);
											if (InputBuffer == NULL)
											{
												BOOST_LOG_TRIVIAL(error) << "std::realloc failed ";
											}
										}
										InputBufferWithOffset = InputBuffer;;
									}
									else if (Mode == Image)
									{
										Mode = ImageSize;
										InputBytesNeeded = sizeof(unsigned int);
										InputBufferWithOffset = InputBuffer;
										cv::imdecode(cv::Mat(SizeofImage, 1, CV_8UC1, InputBuffer), cv::IMREAD_COLOR, &ImageIn);
										DispayImage(ImageIn);
									}
								}
							}
						}
						//else BOOST_LOG_TRIVIAL(debug) << "Client: SSLReadDataTcpNoBlock buff failed " << WSAGetLastError();
					}
				}
				if (NetworkEvents.lNetworkEvents & FD_WRITE)
				{
					if (NetworkEvents.iErrorCode[FD_WRITE_BIT] != 0)
					{
						BOOST_LOG_TRIVIAL(error) << "FD_WRITE failed with error " << NetworkEvents.iErrorCode[FD_WRITE_BIT];
					}
					else
					{
						BOOST_LOG_TRIVIAL(info) << "Client: FD_WRITE";
					}
				}
				if (NetworkEvents.lNetworkEvents & FD_CLOSE)
				{
					if (NetworkEvents.iErrorCode[FD_CLOSE_BIT] != 0)
					{
						BOOST_LOG_TRIVIAL(error) << "FD_CLOSE failed with error " << NetworkEvents.iErrorCode[FD_CLOSE_BIT];
					}
					else
					{
						BOOST_LOG_TRIVIAL(info) << "Client: FD_CLOSE";
						PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
						break;
					}
				}
			}
		}
		// Get the camera frame and send it to the server when the hTimer event occurs
		else if (dwEvent == WAIT_OBJECT_0 + 2)
		{
			unsigned int numbytes;

			if (!GetCameraFrame(sendbuff))
			{
				BOOST_LOG_TRIVIAL(info) << "Camera Frame Empty";
			}
			numbytes = htonl((unsigned long)sendbuff.size());
			if (SSLWriteDataTcp(SSLSocketForClient, (unsigned char*)&numbytes, sizeof(numbytes)) == sizeof(numbytes))
			{
				if (SSLWriteDataTcp(SSLSocketForClient, (unsigned char*)sendbuff.data(), (int)sendbuff.size()) != sendbuff.size())
				{
					BOOST_LOG_TRIVIAL(error) << "SSLWriteDataTcp sendbuff.data() Failed " << WSAGetLastError();
					PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
					break;
				}
			}
			else
			{
				BOOST_LOG_TRIVIAL(error) << "SSLWriteDataTcp sendbuff.size() Failed " << WSAGetLastError();
				PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
				break;
			}
		}
	}

	if (InputBuffer)
	{
		std::free(InputBuffer);
		InputBuffer = nullptr;
	}

	VideoClientCleanup();
	BOOST_LOG_TRIVIAL(info) << "Video Client Exiting";
	return 0;
}
//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------