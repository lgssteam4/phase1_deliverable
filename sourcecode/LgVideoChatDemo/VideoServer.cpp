#include "BoostLog.h"

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include < iostream >
#include <new>
#include "VideoServer.h"
#include <opencv2\highgui\highgui.hpp>
#include <opencv2\opencv.hpp>
#include "VoipVoice.h"
#include "LgVideoChatDemo.h"
#include "TcpSendRecv.h"
#include "DisplayImage.h"
#include "Camera.h"
#include "ApplyOpenSSL.h"
#include "VideoClient.h"

static  std::vector<uchar> sendbuff;//buffer for coding
enum InputMode { ImageSize, Image };
static HANDLE hAcceptEvent = INVALID_HANDLE_VALUE;
static HANDLE hListenEvent = INVALID_HANDLE_VALUE;
static HANDLE hEndVideoServerEvent = INVALID_HANDLE_VALUE;
static HANDLE hTimer = INVALID_HANDLE_VALUE;
static SOCKET Listen = INVALID_SOCKET;
static SOCKET Accept = INVALID_SOCKET;
static SOCKET SSLAccept = INVALID_SOCKET;
SSL_CTX* ctxForServer = NULL;
SSL* SSLSocketForServer = NULL;
static cv::Mat ImageIn;
static DWORD ThreadVideoServerID;
static HANDLE hThreadVideoServer = INVALID_HANDLE_VALUE;
static int NumEvents;
static unsigned int InputBytesNeeded;
static char* InputBuffer = NULL;
static char* InputBufferWithOffset = NULL;
static InputMode Mode = ImageSize;
static bool RealConnectionActive = false;

static void VideoServerSetExitEvent(void);
static void VideoServerCleanup(void);
static DWORD WINAPI ThreadVideoServer(LPVOID ivalue);
static void CleanUpClosedConnection(void);

bool StartVideoServer(bool& Loopback)
{
	if (hThreadVideoServer == INVALID_HANDLE_VALUE)
	{
		hThreadVideoServer = CreateThread(NULL, 0, ThreadVideoServer, &Loopback, 0, &ThreadVideoServerID);
	}
	return true;
}

bool StopVideoServer(void)
{
	VideoServerSetExitEvent();
	if (hThreadVideoServer != INVALID_HANDLE_VALUE)
	{
		WaitForSingleObject(hThreadVideoServer, INFINITE);
		hThreadVideoServer = INVALID_HANDLE_VALUE;
	}
	VideoServerCleanup();
	return true;
}

bool IsVideoServerRunning(void)
{
	if (hThreadVideoServer == INVALID_HANDLE_VALUE) return false;
	else return true;
}

void ClosedConnection(void)
{
	CleanUpClosedConnection();
}

static void VideoServerSetExitEvent(void)
{
	if (hEndVideoServerEvent != INVALID_HANDLE_VALUE)
		SetEvent(hEndVideoServerEvent);
}

static void VideoServerCleanup(void)
{
	if (hListenEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hListenEvent);
		hListenEvent = INVALID_HANDLE_VALUE;
	}
	if (hAcceptEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hAcceptEvent);
		hAcceptEvent = INVALID_HANDLE_VALUE;
	}
	if (hEndVideoServerEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hEndVideoServerEvent);
		hEndVideoServerEvent = INVALID_HANDLE_VALUE;
	}
	if (hTimer != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hTimer);
		hTimer = INVALID_HANDLE_VALUE;
	}
	if (Listen != INVALID_SOCKET)
	{
		closesocket(Listen);
		Listen = INVALID_SOCKET;
	}
	if (Accept != INVALID_SOCKET)
	{
		closesocket(Accept);
		Accept = INVALID_SOCKET;
		if (SSLAccept != INVALID_SOCKET)
		{
			SSLAccept = INVALID_SOCKET;
			SSL_free(SSLSocketForServer);
			SSL_CTX_free(ctxForServer);
		}
	}
}

static DWORD WINAPI ThreadVideoServer(LPVOID ivalue)
{
	BOOST_LOG_TRIVIAL(debug) << "Server: Start ThreadVideoServer";

	SOCKADDR_IN InternetAddr;
	HANDLE ghEvents[4];
	DWORD dwEvent;
	bool  Loopback = *((bool*)ivalue);
	bool  LoopbackOverRide = false;
	unsigned int SizeofImage;
	unsigned int CurrentInputBufferSize = 1024 * 10;

	BOOST_LOG_TRIVIAL(info) << "Video Server Started Loopback " << (Loopback ? "True" : "False");

	RealConnectionActive = false;
	Mode = ImageSize;
	InputBytesNeeded = sizeof(unsigned int);
	InputBuffer = (char*)std::realloc(NULL, CurrentInputBufferSize);
	InputBufferWithOffset = InputBuffer;

	if (InputBuffer == NULL)
	{
		BOOST_LOG_TRIVIAL(error) << "InputBuffer Realloc failed";
		return 1;
	}

	// Creates a socket that the server will use to accept connections from clients
	if ((Listen = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		BOOST_LOG_TRIVIAL(error) << "listen socket() failed with error " << WSAGetLastError();
		return 1;
	}

	// Create a handle to handle client connection requests and socket events
	hListenEvent = WSACreateEvent();

	// Create a video server end event handle
	hEndVideoServerEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// // Connect socket and event handle (specify to watch for FD_READ and FD_CLOSE events)
	if (WSAEventSelect(Listen, hListenEvent, FD_ACCEPT | FD_CLOSE) == SOCKET_ERROR)
	{
		BOOST_LOG_TRIVIAL(error) << "WSAEventSelect() failed with error " << WSAGetLastError();
		return 1;
	}
	InternetAddr.sin_family = AF_INET;
	InternetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	InternetAddr.sin_port = htons(VIDEO_PORT);

	// Bind an IP address and port number to a socket
	if (bind(Listen, (PSOCKADDR)&InternetAddr, sizeof(InternetAddr)) == SOCKET_ERROR)
	{
		BOOST_LOG_TRIVIAL(error) << "bind() failed with error " << WSAGetLastError();
		return 1;
	}

	// Set a socket to wait to listen for connections from clients
	if (listen(Listen, 5))
	{
		BOOST_LOG_TRIVIAL(error) << "listen() failed with error " << WSAGetLastError();
		return 1;
	}

	ghEvents[0] = hEndVideoServerEvent;
	ghEvents[1] = hListenEvent;
	NumEvents = 2;

	while (1) {
		dwEvent = WaitForMultipleObjects(
			NumEvents,	// number of objects in array
			ghEvents,	// array of objects
			FALSE,		// wait for any object
			INFINITE);	// INFINITE) wait

		// Video server end event
		if (dwEvent == WAIT_OBJECT_0) break;
		// Client's connection request event
		else if (dwEvent == WAIT_OBJECT_0 + 1)
		{
			WSANETWORKEVENTS NetworkEvents;
			// Get network events on listen socket
			if (SOCKET_ERROR == WSAEnumNetworkEvents(Listen, hListenEvent, &NetworkEvents))
			{
				BOOST_LOG_TRIVIAL(info) << "WSAEnumNetworkEvent: " << WSAGetLastError() << " dwEvent  " << dwEvent << " lNetworkEvent " << std::hex << NetworkEvents.lNetworkEvents;
				NetworkEvents.lNetworkEvents = 0;
			}
			else
			{
				// Accept new client connection when FD_ACCEPT event occurs
				if (NetworkEvents.lNetworkEvents & FD_ACCEPT)
				{
					if (NetworkEvents.iErrorCode[FD_ACCEPT_BIT] != 0)
					{
						BOOST_LOG_TRIVIAL(error) << "FD_ACCEPT failed with error " << NetworkEvents.iErrorCode[FD_ACCEPT_BIT];
					}
					else
					{
						struct sockaddr_storage sa{};
						socklen_t sa_len = sizeof(sa);
						char RemoteIP[INET6_ADDRSTRLEN];
						char MissedIP[INET6_ADDRSTRLEN];
						int err = -1;

						if (Accept == INVALID_SOCKET)
						{
							LARGE_INTEGER liDueTime{};

							// Accept a new connection, and add it to the socket and event lists
							Accept = accept(Listen, (struct sockaddr*)&sa, &sa_len);
							BOOST_LOG_TRIVIAL(debug) << "Server: Success accept";

							// Initialize SSL
							initializeSSL();
							BOOST_LOG_TRIVIAL(debug) << "Server: Success initializeSSL";

							// Create and initialize SSL context
							ctxForServer = createSSLContextForServer();
							if (ctxForServer == NULL)
							{
								BOOST_LOG_TRIVIAL(error) << "Server: Error createSSLContextForServer";
								closesocket(Accept);
								Accept = INVALID_SOCKET;
								continue;
							}
							BOOST_LOG_TRIVIAL(debug) << "Server: Success createSSLContextForServer";

							// Create SSL socket
							SSLSocketForServer = createSSLSocket(ctxForServer, Accept);
							if (SSLSocketForServer == NULL)
							{
								BOOST_LOG_TRIVIAL(error) << "Server: Error createSSLSocket";
								closesocket(Accept);
								Accept = INVALID_SOCKET;
								SSL_CTX_free(ctxForServer);
								continue;
							}
							SSLAccept = SSL_get_fd(SSLSocketForServer);
							if (SSLAccept == INVALID_SOCKET)
							{
								BOOST_LOG_TRIVIAL(error) << "Server: Error SSL_get_fd";
								closesocket(Accept);
								Accept = INVALID_SOCKET;
								SSL_free(SSLSocketForServer);
								SSL_CTX_free(ctxForServer);
								continue;
							}

							// Accept or decline client connections
							int checkOK = MessageBox(hWndMain, L"Would you like to accept the video call?", L"Incoming video call", MB_ICONQUESTION | MB_OKCANCEL);
							if (checkOK != IDOK)
							{
								BOOST_LOG_TRIVIAL(info) << "Server: Rejected video call";
								closesocket(Accept);
								Accept = INVALID_SOCKET;
								SSL_free(SSLSocketForServer);
								SSL_CTX_free(ctxForServer);
								SSLAccept = INVALID_SOCKET;
								continue;
							}

							err = getnameinfo((struct sockaddr*)&sa, sa_len, RemoteIP, sizeof(RemoteIP), 0, 0, NI_NUMERICHOST);
							if (err != 0) {
								snprintf(RemoteIP, sizeof(RemoteIP), "invalid address");
							}
							else
							{
								BOOST_LOG_TRIVIAL(info) << "Accepted Connection " << RemoteIP;
							}

							if (!Loopback)
							{
								if ((strcmp(RemoteIP, LocalIpAddress) == 0) ||
									(strcmp(RemoteIP, "127.0.0.1") == 0))
								{
									LoopbackOverRide = true;
									BOOST_LOG_TRIVIAL(info) << "Loopback Over Ride";
								}
								else LoopbackOverRide = false;
							}

							// Send message indicating connection status
							PostMessage(hWndMain, WM_REMOTE_CONNECT, 0, reinterpret_cast<LPARAM>(RemoteIP));
							hAcceptEvent = WSACreateEvent();
							WSAEventSelect(SSLAccept, hAcceptEvent, FD_READ | FD_WRITE | FD_CLOSE);
							ghEvents[2] = hAcceptEvent;
							if ((Loopback) || (LoopbackOverRide))  NumEvents = 3;
							else
							{
								liDueTime.QuadPart = 0LL;
								if (!OpenCamera())
								{
									BOOST_LOG_TRIVIAL(error) << "OpenCamera() Failed";
									break;
								}
								VoipVoiceStart(RemoteIP, VOIP_LOCAL_PORT, VOIP_REMOTE_PORT, VoipAttr);
								BOOST_LOG_TRIVIAL(info) << "Voip Voice Started..";
								RealConnectionActive = true;
								hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

								if (NULL == hTimer)
								{
									BOOST_LOG_TRIVIAL(error) << "CreateWaitableTimer failed " << GetLastError();
									break;
								}

								if (!SetWaitableTimer(hTimer, &liDueTime, VIDEO_FRAME_DELAY, NULL, NULL, 0))
								{
									BOOST_LOG_TRIVIAL(error) << "SetWaitableTimer failed  " << GetLastError();
									break;
								}
								ghEvents[3] = hTimer;
								NumEvents = 4;
							}
						}
						else
						{
							SOCKET Temp = accept(Listen, (struct sockaddr*)&sa, &sa_len);
							if (Temp != INVALID_SOCKET)
							{
								err = getnameinfo((struct sockaddr*)&sa, sa_len, MissedIP, sizeof(MissedIP), 0, 0, NI_NUMERICHOST);
								if (err != 0) {
									snprintf(MissedIP, sizeof(MissedIP), "invalid address");
								}
								closesocket(Temp);
								BOOST_LOG_TRIVIAL(info) << "Refused-Already Connected";

								PostMessage(hWndMain, WM_MISSEDCALL, 0, reinterpret_cast<LPARAM>(MissedIP));
							}
						}
					}
				}
				if (NetworkEvents.lNetworkEvents & FD_CLOSE)
				{
					if (NetworkEvents.iErrorCode[FD_CLOSE_BIT] != 0)
					{
						BOOST_LOG_TRIVIAL(error) << "FD_CLOSE failed with error on Listen Socket" << NetworkEvents.iErrorCode[FD_CLOSE_BIT];
					}

					closesocket(Listen);
					Listen = INVALID_SOCKET;
					break;
				}
			}
		}
		// Data transmission and reception are handled when an event of a connected client socket occurs
		else if (dwEvent == WAIT_OBJECT_0 + 2)
		{
			WSANETWORKEVENTS NetworkEvents;
			if (SOCKET_ERROR == WSAEnumNetworkEvents(SSLAccept, hAcceptEvent, &NetworkEvents))
			{
				BOOST_LOG_TRIVIAL(error) << "WSAEnumNetworkEvent: " << WSAGetLastError() << " dwEvent  " << dwEvent << " lNetworkEvent " << std::hex << NetworkEvents.lNetworkEvents;
				NetworkEvents.lNetworkEvents = 0;
			}
			else
			{
				// Read data when FD_READ event occurs
				if (NetworkEvents.lNetworkEvents & FD_READ)
				{
					if (NetworkEvents.iErrorCode[FD_READ_BIT] != 0)
					{
						BOOST_LOG_TRIVIAL(error) << "FD_READ failed with error " << NetworkEvents.iErrorCode[FD_READ_BIT];
					}
					// Receive the data and send it back when loopback connection
					else if ((Loopback) || (LoopbackOverRide))
					{
						unsigned char* buffer;
						u_long bytesAvailable;
						int bytestosend;
						int iResult;

						ioctlsocket(SSLAccept, FIONREAD, &bytesAvailable);
						if (bytesAvailable >= 0)
						{
							buffer = new (std::nothrow) unsigned char[bytesAvailable];
							//BOOST_LOG_TRIVIAL(debug) << "FD_READ "<< bytesAvailable;
							iResult = SSLReadDataTcpNoBlock(SSLSocketForServer, buffer, bytesAvailable);
							if (iResult > 0)
							{
								bytestosend = iResult;
								iResult = SSLWriteDataTcp(SSLSocketForServer, buffer, bytestosend);
								delete[] buffer;
								if (iResult == SOCKET_ERROR)
								{
									BOOST_LOG_TRIVIAL(error) << "SSLWriteDataTcp failed: " << WSAGetLastError();
								}
							}
							else if (iResult == 0)
							{
								BOOST_LOG_TRIVIAL(error) << "Connection closed on Recv";
								CleanUpClosedConnection();
							}
							else
							{
								//BOOST_LOG_TRIVIAL(debug) << "Server: SSLReadDataTcpNoBlock failed:" << WSAGetLastError();
							}
						}
					}
					else // No Loopback
					{
						int iResult;

						iResult = SSLReadDataTcpNoBlock(SSLSocketForServer, (unsigned char*)InputBufferWithOffset, InputBytesNeeded);
						if (iResult != SOCKET_ERROR)
						{
							if (iResult == 0)
							{
								CleanUpClosedConnection();
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
						//else BOOST_LOG_TRIVIAL(debug) << "Server: SSLReadDataTcpNoBlock buff failed " << WSAGetLastError();
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
						BOOST_LOG_TRIVIAL(info) << "Server: FD_WRITE";
					}
				}
				if (NetworkEvents.lNetworkEvents & FD_CLOSE)
				{
					if (NetworkEvents.iErrorCode[FD_CLOSE_BIT] != 0)
					{
						BOOST_LOG_TRIVIAL(error) << "FD_CLOSE failed with error Connection " << NetworkEvents.iErrorCode[FD_CLOSE_BIT];
					}
					else
					{
						BOOST_LOG_TRIVIAL(info) << "Server: FD_CLOSE";
					}
					CleanUpClosedConnection();
				}
			}
		}
		// When a timer event occurs, frames are taken from the camera and sent to the client
		else if (dwEvent == WAIT_OBJECT_0 + 3)
		{
			unsigned int numbytes;

			if (!GetCameraFrame(sendbuff))
			{
				BOOST_LOG_TRIVIAL(info) << "Camera Frame Empty";
			}
			numbytes = htonl((unsigned long)sendbuff.size());
			if (SSLWriteDataTcp(SSLSocketForServer, (unsigned char*)&numbytes, sizeof(numbytes)) == sizeof(numbytes))
			{
				if (SSLWriteDataTcp(SSLSocketForServer, (unsigned char*)sendbuff.data(), (int)sendbuff.size()) != sendbuff.size())
				{
					BOOST_LOG_TRIVIAL(error) << "SSLWriteDataTcp sendbuff.data() Failed " << WSAGetLastError();
					CleanUpClosedConnection();
				}
			}
			else {
				BOOST_LOG_TRIVIAL(error) << "SSLWriteDataTcp sendbuff.size() Failed " << WSAGetLastError();
				CleanUpClosedConnection();
			}
		}
	}

	CleanUpClosedConnection();
	VideoServerCleanup();
	BOOST_LOG_TRIVIAL(info) << "Video Server Stopped";
	return 0;
}

static void CleanUpClosedConnection(void)
{
	if (Accept != INVALID_SOCKET)
	{
		closesocket(Accept);
		Accept = INVALID_SOCKET;
		if (SSLAccept != INVALID_SOCKET)
		{
			SSLAccept = INVALID_SOCKET;
			SSL_free(SSLSocketForServer);
			SSL_CTX_free(ctxForServer);
		}
		PostMessage(hWndMain, WM_REMOTE_LOST, 0, 0);
	}
	if (hAcceptEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hAcceptEvent);
		hAcceptEvent = INVALID_HANDLE_VALUE;
	}
	if (hTimer != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hTimer);
		hTimer = INVALID_HANDLE_VALUE;
	}
	if (RealConnectionActive)
	{
		CloseCamera();
		VoipVoiceStop();
	}
	Mode = ImageSize;
	InputBytesNeeded = sizeof(unsigned int);
	InputBufferWithOffset = InputBuffer;
	NumEvents = 2;
	RealConnectionActive = false;
}
//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------