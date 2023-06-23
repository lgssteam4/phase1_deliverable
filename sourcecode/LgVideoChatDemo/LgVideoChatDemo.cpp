// LgVideoChatDemo.cpp : Defines the entry point for the application.

#include "BoostLog.h"
#include "framework.h"
#include <Commctrl.h>
#include <atlstr.h>
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <fcntl.h>
#include <opencv2\highgui\highgui.hpp>
#include <opencv2\opencv.hpp>
#include "VoipVoice.h"
#include "LgVideoChatDemo.h"
#include "VideoServer.h"
#include "Camera.h"
#include "DisplayImage.h"
#include "VideoClient.h"
#include "litevad.h"
#include "MemberSignIn.h"
#include "MemberSignUp.h"
#include "MemberUpdate.h"
#include "ContactList.h"
#include "CallHistory.h"
#include "CallStatus.h"
#include "BackendHttpsClient.h"
#include "eWID.h"

extern std::string accessToken;

#pragma comment(lib,"comctl32.lib")
#ifdef _DEBUG
#pragma comment(lib,"..\\..\\opencv\\build\\x64\\vc16\\lib\\opencv_world470d.lib")
#else
#pragma comment(lib,"..\\..\\opencv\\build\\x64\\vc16\\lib\\opencv_world470.lib")
#endif
#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "ws2_32.lib")

#define MAX_LOADSTRING 100

// Global Variables:

HWND hWndMain, hCamWnd;

GUID InstanceGuid;
char guidBuf[1024];
char LocalIpAddress[512] = "127.0.0.1";
char* ConnectedIP = nullptr;
char* MissedIP = nullptr;
std::time_t startCall, endCall;
std::tm startCall_time{}, endCall_time{};
TVoipAttr VoipAttr = { true,false };

static HINSTANCE hInst;                                // current instance
static WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
static WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

static char RemoteAddress[512] = "127.0.0.1";
static bool Loopback = false;

static FILE* pCout = NULL;
static HWND hWndMainToolbar;
static HWND hWndEdit;

// Forward declarations of functions included in this code module:
static ATOM                MyRegisterClass(HINSTANCE hInstance);
static BOOL                InitInstance(HINSTANCE, int);
static LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

static LRESULT OnCreate(HWND, UINT, WPARAM, LPARAM);
static LRESULT OnSize(HWND, UINT, WPARAM, LPARAM);
static int OnConnect(HWND, UINT, WPARAM, LPARAM);
static int OnDisconnect(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static int OnStartServer(HWND, UINT, WPARAM, LPARAM);
static int OnStopServer(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static int PRINT(const TCHAR* fmt, ...);
static void SetHostAddr(void);
static void SetStdOutToNewConsole(void);
static void DisplayMessageOkBox(const char* Msg);
static bool OnlyOneInstance(void);

// Get user information
bool queryUserInfo(const std::string& remoteIp, std::map<std::string, std::string>& userInfo) {
	unsigned int rc = 0;
	std::string data = "ip_address=" + remoteIp;
	std::string api = "/api/user/get-info-from-ip/";
	std::string sessionToken = accessToken;

	rc = sendPostRequest(api, data, sessionToken, userInfo);
	if (rc == 200) {
		return true;
	}

	return false;
}

// Update call status
void updateCallStatus(const char* remoteIP)
{
	BOOST_LOG_TRIVIAL(debug) << "Start updateCallStatus: " << remoteIP;
	std::map<std::string, std::string> response;
	std::string email, first_name, last_name, ip_address;
	CStringW cstr_email, cstr_first_name, cstr_last_name, cstr_ip_address;

	// Check call start time
	startCall = std::time(nullptr);

	if (queryUserInfo(remoteIP, response))
	{
		//for (const auto& pair : response) {
		//	BOOST_LOG_TRIVIAL(debug) << pair.first << ": " << pair.second;
		//}
		email = response["email"];
		first_name = response["first_name"];
		last_name = response["last_name"];
		ip_address = response["ip_address"];
		cstr_email = email.c_str();
		cstr_first_name = first_name.c_str();
		cstr_last_name = last_name.c_str();
		cstr_ip_address = ip_address.c_str();

		WriteToCallStatusEditBox(_T("[ Calling... ]\r\n- Email: %s\r\n- First Name: %s\r\n- Last Name: %s\r\n- IP Address: %s"),
			cstr_email, cstr_first_name, cstr_last_name, cstr_ip_address);
	}
	else
	{
		WriteToCallStatusEditBox(_T("[ Calling... ]\r\nNo user information"));
	}
	BOOST_LOG_TRIVIAL(debug) << "End updateCallStatus";
}

// Update call history
void updateCallHistory(const char* remoteIP, bool calling)
{
	BOOST_LOG_TRIVIAL(debug) << "Start updateCallHistory: " << remoteIP << " calling: " << calling;
	std::map<std::string, std::string> response;
	std::string email, first_name, last_name, ip_address;
	CStringW cstr_email, cstr_first_name, cstr_last_name, cstr_ip_address, cstr_time, cstr_ip;
	std::time_t duration;
	char time_str[12];

	// Check call end time
	endCall = std::time(nullptr);

	if (calling)
	{
		duration = std::difftime(endCall, startCall);
		localtime_s(&startCall_time, &startCall);
		std::strftime(time_str, sizeof(time_str), "%I:%M:%S %p", &startCall_time);
	}
	else
	{
		localtime_s(&endCall_time, &endCall);
		std::strftime(time_str, sizeof(time_str), "%I:%M:%S %p", &endCall_time);
	}

	BOOST_LOG_TRIVIAL(debug) << "Current time: " << time_str;
	cstr_time = time_str;

	if (queryUserInfo(remoteIP, response))
	{
		//for (const auto& pair : response) {
		//	BOOST_LOG_TRIVIAL(debug) << pair.first << ": " << pair.second;
		//}
		email = response["email"];
		first_name = response["first_name"];
		last_name = response["last_name"];
		ip_address = response["ip_address"];
		cstr_email = email.c_str();
		cstr_first_name = first_name.c_str();
		cstr_last_name = last_name.c_str();
		cstr_ip_address = ip_address.c_str();

		if (calling)
		{
			WriteToCallHistoryEditBox(_T("[ Called ]\r\n- Time: %s (duration: %ds)\r\n- Email: %s\r\n- First Name: %s\r\n- Last Name: %s\r\n- IP Address: %s\r\n\r\n"),
				cstr_time, static_cast<int>(duration), cstr_email, cstr_first_name, cstr_last_name, cstr_ip_address);
		}
		else
		{
			WriteToCallHistoryEditBox(_T(" [Missed Call ]\r\n- Time: %s\r\n- Email: %s\r\n- First Name: %s\r\n- Last Name: %s\r\n- IP Address: %s\r\n\r\n"),
				cstr_time, cstr_email, cstr_first_name, cstr_last_name, cstr_ip_address);
		}
	}
	else
	{
		cstr_ip = remoteIP;
		if (calling)
		{
			WriteToCallHistoryEditBox(_T("[ Called ]\r\n- Time: %s (duration: %ds)\r\n- IP Address: %s\r\nNo user information\r\n\r\n"),
				cstr_time, static_cast<int>(duration), cstr_ip);
		}
		else
		{
			WriteToCallHistoryEditBox(_T("[ Missed Call ]\r\n- Time: %s\r\n- IP Address: %s\r\nNo user information\r\n\r\n"),
				cstr_time, cstr_ip);
		}
	}
	BOOST_LOG_TRIVIAL(debug) << "End updateCallHistory";
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	WSADATA wsaData;
	HRESULT hr;

	//SetStdOutToNewConsole();
	InitLogging();

	int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (res != NO_ERROR) {
		BOOST_LOG_TRIVIAL(error) << "WSAStartup failed with error ";
		return 1;
	}
	SetHostAddr();
	hr = CoCreateGuid(&InstanceGuid);
	if (hr != S_OK)
	{
		BOOST_LOG_TRIVIAL(error) << "GUID Create Failure ";
		return 1;
	}

	snprintf(guidBuf, sizeof(guidBuf), "Guid = {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
		InstanceGuid.Data1, InstanceGuid.Data2, InstanceGuid.Data3,
		InstanceGuid.Data4[0], InstanceGuid.Data4[1], InstanceGuid.Data4[2], InstanceGuid.Data4[3],
		InstanceGuid.Data4[4], InstanceGuid.Data4[5], InstanceGuid.Data4[6], InstanceGuid.Data4[7]);
	BOOST_LOG_TRIVIAL(info) << guidBuf;

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_LGVIDEOCHATDEMO, szWindowClass, MAX_LOADSTRING);

	if (!OnlyOneInstance())
	{
		BOOST_LOG_TRIVIAL(info) << "Another Instance Running ";
		return 1;
	}

	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		WSACleanup();
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LGVIDEOCHATDEMO));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (pCout)
	{
		fclose(pCout);
		FreeConsole();
	}
	WSACleanup();
	return (int)msg.wParam;
}

static void SetStdOutToNewConsole(void)
{
	// Allocate a console for this app
	AllocConsole();
	//AttachConsole(ATTACH_PARENT_PROCESS);
	freopen_s(&pCout, "CONOUT$", "w", stdout);
}

static void SetHostAddr(void)
{
	// Get the local hostname
	struct addrinfo* _addrinfo;
	struct addrinfo* _res;
	char _address[INET6_ADDRSTRLEN];
	char szHostName[255];
	gethostname(szHostName, sizeof(szHostName));
	getaddrinfo(szHostName, NULL, 0, &_addrinfo);

	for (_res = _addrinfo; _res != NULL; _res = _res->ai_next)
	{
		if (_res->ai_family == AF_INET)
		{
			if (NULL == inet_ntop(AF_INET,
				&((struct sockaddr_in*)_res->ai_addr)->sin_addr,
				_address,
				sizeof(_address))
				)
			{
				perror("inet_ntop");
				return;
			}
			strcpy_s(RemoteAddress, sizeof(RemoteAddress), _address);
			strcpy_s(LocalIpAddress, sizeof(LocalIpAddress), _address);
		}
	}
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
static ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LGVIDEOCHATDEMO));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_LGVIDEOCHATDEMO);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
static BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW
		& ~WS_THICKFRAME     /* disable window resize*/
		& ~WS_MAXIMIZEBOX,   /* remove window maximizebox*/
		CW_USEDEFAULT, 0,
		//CW_USEDEFAULT, 0,
		1380, 790,			/* set to fixed size */
		nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	INT_PTR resultSignUp;
	INT_PTR resultUpdate;

	switch (message)
	{
	
	case WM_COMMAND:
	{
		if (HIWORD(wParam) == LBN_SELCHANGE) // ListBox Select
			DoubleClickContactListEventHandler(hWnd, message, wParam, lParam);

		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDC_EDIT_REMOTE:
		{
			HWND hEditWnd;
			hEditWnd = GetDlgItem(hWnd, IDC_EDIT_REMOTE);
			GetWindowTextA(hEditWnd, RemoteAddress, sizeof(RemoteAddress));
		}
		break;
		case IDC_CHECKBOX_LOOPBACK:
		{
			BOOL checked = IsDlgButtonChecked(hWnd, IDC_CHECKBOX_LOOPBACK);
			if (checked) {
				CheckDlgButton(hWnd, IDC_CHECKBOX_LOOPBACK, BST_UNCHECKED);
				Loopback = false;;
			}
			else {
				CheckDlgButton(hWnd, IDC_CHECKBOX_LOOPBACK, BST_CHECKED);
				Loopback = true;
			}
		}
		break;
		case IDC_CHECKBOX_AEC:
		{
			BOOL checked = IsDlgButtonChecked(hWnd, IDC_CHECKBOX_AEC);
			if (checked) {
				CheckDlgButton(hWnd, IDC_CHECKBOX_AEC, BST_UNCHECKED);
				VoipAttr.AecOn = false;;
			}
			else {
				CheckDlgButton(hWnd, IDC_CHECKBOX_AEC, BST_CHECKED);
				VoipAttr.AecOn = true;
			}
		}
		break;
		case IDC_CHECKBOX_NS:
		{
			BOOL checked = IsDlgButtonChecked(hWnd, IDC_CHECKBOX_NS);
			if (checked) {
				CheckDlgButton(hWnd, IDC_CHECKBOX_NS, BST_UNCHECKED);
				VoipAttr.NoiseSuppressionOn = false;;
			}
			else {
				CheckDlgButton(hWnd, IDC_CHECKBOX_NS, BST_CHECKED);
				VoipAttr.NoiseSuppressionOn = true;
			}
		}
		break;
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case IDM_CONNECT:
			if (OnConnect(hWnd, message, wParam, lParam))
			{
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_CONNECT,
					(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_DISCONNECT,
					(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
			}
			break;
		case IDM_DISCONNECT:
			SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_CONNECT,
				(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
			SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_DISCONNECT,
				(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
			OnDisconnect(hWnd, message, wParam, lParam);
			break;
		case IDM_START_SERVER:
			SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_START_SERVER,
				(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
			SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_STOP_SERVER,
				(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
			EnableWindow(GetDlgItem(hWnd, IDC_CHECKBOX_LOOPBACK), false);
			//EnableWindow(GetDlgItem(hWnd, IDC_CHECKBOX_AEC), false);
			//EnableWindow(GetDlgItem(hWnd, IDC_CHECKBOX_NS), false);
			OnStartServer(hWnd, message, wParam, lParam);
			break;
		case IDM_STOP_SERVER:
			SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_START_SERVER,
				(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
			SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_STOP_SERVER,
				(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
			SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_CONNECT,
				(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
			SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_DISCONNECT,
				(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
			EnableWindow(GetDlgItem(hWnd, IDC_CHECKBOX_LOOPBACK), true);
			//EnableWindow(GetDlgItem(hWnd, IDC_CHECKBOX_AEC), true);
			//EnableWindow(GetDlgItem(hWnd, IDC_CHECKBOX_NS), true);
			OnStopServer(hWnd, message, wParam, lParam);
			break;
		case IDM_SIGNUP:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_SIGNUPDIALOG), hWnd, SignUp);
			break;
		case IDM_SIGNIN:
			resultSignUp = DialogBox(hInst, MAKEINTRESOURCE(IDD_SIGNINDIALOG), hWnd, SignIn);
			if (resultSignUp == IDOK) {
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_SIGNIN,
						(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_SIGNUP,
					(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_UPDATE,
					(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_CONNECT,
					(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_START_SERVER,
					(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
			}
			break;
		case IDM_UPDATE:
			resultUpdate = DialogBox(hInst, MAKEINTRESOURCE(IDD_UPDATEDIALOG), hWnd, Update);
			if (resultUpdate == IDOK) {
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_SIGNIN,
					(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_SIGNUP,
					(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_UPDATE,
					(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_CONNECT,
					(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_DISCONNECT,
					(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_START_SERVER,
					(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
				SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_STOP_SERVER,
					(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
			}
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_CREATE:
		OnCreate(hWnd, message, wParam, lParam);
		break;
	case WM_SIZE:
		OnSize(hWnd, message, wParam, lParam);
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_CLOSE:
	{
		int checkOK = MessageBox(hWnd, L"Are you sure you want to exit the program?", L"LGVideoChatDemo Application", MB_ICONQUESTION | MB_OKCANCEL);
		if (checkOK != IDCANCEL) {
			DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CLIENT_LOST:
		BOOST_LOG_TRIVIAL(info) << "WM_CLIENT_LOST";
		SendMessage(hWndMain, WM_COMMAND, IDM_DISCONNECT, 0);
		// Update call history
		updateCallHistory(RemoteAddress, true);
		break;
	case WM_REMOTE_CONNECT:
		BOOST_LOG_TRIVIAL(info) << "WM_REMOTE_CONNECT";
		SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_CONNECT,
			(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
		SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_DISCONNECT,
			(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
		// Update call status
		ConnectedIP = reinterpret_cast<char*>(lParam);
		updateCallStatus(ConnectedIP);

		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		BOOST_LOG_TRIVIAL(info) << "Enable window alway on top.";
		break;
	case WM_REMOTE_LOST:
		BOOST_LOG_TRIVIAL(info) << "WM_REMOTE_LOST";
		SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_CONNECT,
			(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
		SendMessage(hWndMainToolbar, TB_SETSTATE, IDM_DISCONNECT,
			(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
		// Update call history
		updateCallHistory(ConnectedIP, true);
		
		WriteToCallStatusEditBox(_T("[ Waiting... ]"));
		MessageBox(hWnd, L"Video Call ended.", L"Alarm", MB_OK);
		SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		BOOST_LOG_TRIVIAL(info) << "Disable window alway on top.";
		break;
	case WM_MISSEDCALL:
		BOOST_LOG_TRIVIAL(info) << "WM_MISSEDCALL";
		// Update call history
		MissedIP = reinterpret_cast<char*>(lParam);
		updateCallHistory(MissedIP, false);
		break;
	case WM_VAD_STATE:
	{
		HWND hTempWnd;
		litevad_result_t VadState;
		hTempWnd = GetDlgItem(hWnd, IDC_VAD_STATE_STATUS);
		VadState = (litevad_result_t)wParam;
		switch (VadState)
		{
		case LITEVAD_RESULT_SPEECH_BEGIN:
			SetWindowTextA(hTempWnd, "Speech");
			break;
		case LITEVAD_RESULT_SPEECH_END:
			SetWindowTextA(hTempWnd, "Speech End");
			break;
		case LITEVAD_RESULT_SPEECH_BEGIN_AND_END:
			SetWindowTextA(hTempWnd, "Speech Begin & End");
			break;
		case LITEVAD_RESULT_FRAME_SILENCE:
			SetWindowTextA(hTempWnd, "Silence");
			break;
		case LITEVAD_RESULT_FRAME_ACTIVE:
			break;
		case LITEVAD_RESULT_ERROR:
			SetWindowTextA(hTempWnd, "VAD Error");
			break;
		default:
			SetWindowTextA(hTempWnd, "Unknown Error");
			break;
		}
	}
	break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
static INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
HIMAGELIST g_hImageList = NULL;

HWND CreateSimpleToolbar(HWND hWndParent)
{
	// Declare and initialize local constants.
	const int ImageListID = 0;
	const int numButtons = 7;
	const int bitmapSize = 16;

	const DWORD buttonStyles = BTNS_AUTOSIZE;

	// Create the toolbar.
	HWND hWndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
		WS_CHILD | TBSTYLE_WRAPABLE,
		0, 0, 0, 0,
		hWndParent, NULL, hInst, NULL);

	if (hWndToolbar == NULL)
		return NULL;

	// Create the image list.
	g_hImageList = ImageList_Create(bitmapSize, bitmapSize,   // Dimensions of individual bitmaps.
		ILC_COLOR16 | ILC_MASK,   // Ensures transparent background.
		numButtons, 0);

	// Set the image list.
	SendMessage(hWndToolbar, TB_SETIMAGELIST,
		(WPARAM)ImageListID,
		(LPARAM)g_hImageList);

	// Load the button images.
	SendMessage(hWndToolbar, TB_LOADIMAGES,
		(WPARAM)IDB_STD_SMALL_COLOR,
		(LPARAM)HINST_COMMCTRL);

	// Initialize button info.
	// IDM_NEW, IDM_OPEN, and IDM_SAVE are application-defined command constants.

	TBBUTTON tbButtons[numButtons] =
	{
		{ MAKELONG(VIEW_NETCONNECT,    ImageListID), IDM_CONNECT,      TBSTATE_INDETERMINATE, buttonStyles, {0}, 0, (INT_PTR)L"Connect" },
		{ MAKELONG(VIEW_NETDISCONNECT, ImageListID), IDM_DISCONNECT,   TBSTATE_INDETERMINATE, buttonStyles, {0}, 0, (INT_PTR)L"Disconnect"},
		{ MAKELONG(VIEW_NETCONNECT,    ImageListID), IDM_START_SERVER, TBSTATE_INDETERMINATE, buttonStyles, {0}, 0, (INT_PTR)L"Start Server"},
		{ MAKELONG(VIEW_NETDISCONNECT, ImageListID), IDM_STOP_SERVER,  TBSTATE_INDETERMINATE, buttonStyles, {0}, 0, (INT_PTR)L"Stop Server"},
		{ MAKELONG(VIEW_NETCONNECT,    ImageListID), IDM_SIGNUP,       TBSTATE_ENABLED,       buttonStyles, {0}, 0, (INT_PTR)L"Sign-Up" },
		{ MAKELONG(VIEW_NETCONNECT,    ImageListID), IDM_SIGNIN,       TBSTATE_ENABLED,       buttonStyles, {0}, 0, (INT_PTR)L"Sign-In" },
		{ MAKELONG(VIEW_NETCONNECT,    ImageListID), IDM_UPDATE,       TBSTATE_INDETERMINATE, buttonStyles, {0}, 0, (INT_PTR)L"Update" },
	};

	// Add buttons.
	SendMessage(hWndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
	SendMessage(hWndToolbar, TB_ADDBUTTONS, (WPARAM)numButtons, (LPARAM)&tbButtons);

	// Resize the toolbar, and then show it.
	SendMessage(hWndToolbar, TB_AUTOSIZE, 0, 0);
	ShowWindow(hWndToolbar, TRUE);

	return hWndToolbar;
}

static LRESULT OnCreate(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT checked;
	
	InitCommonControls();

	CreateWindow(_T("STATIC"),
		_T("Remote Address:"),
		WS_VISIBLE | WS_CHILD,
		5, 50, 120, 20,
		hWnd,
		(HMENU)IDC_LABEL_REMOTE,
		((LPCREATESTRUCT)lParam)->hInstance, NULL);

	CreateWindow(_T("STATIC"),
		_T("VAD State:"),
		WS_VISIBLE | WS_CHILD,
		260, 50, 70, 20,
		hWnd,
		(HMENU)IDC_LABEL_VAD_STATE,
		((LPCREATESTRUCT)lParam)->hInstance, NULL);

	CreateWindow(_T("STATIC"),
		_T("Unknown"),
		WS_VISIBLE | WS_CHILD,
		335, 50, 120, 20,
		hWnd,
		(HMENU)IDC_VAD_STATE_STATUS,
		((LPCREATESTRUCT)lParam)->hInstance, NULL);

	CreateWindowExA(WS_EX_CLIENTEDGE,
		"EDIT", RemoteAddress,
		WS_CHILD | WS_VISIBLE ,// | WS_DISABLED,
		130, 50, 120, 20,
		hWnd,
		(HMENU)IDC_EDIT_REMOTE,
		((LPCREATESTRUCT)lParam)->hInstance, NULL);

	if (Loopback)  checked = BST_CHECKED;
	else checked = BST_UNCHECKED;

	CreateWindow(_T("button"), _T("Loopback"),
		WS_VISIBLE | WS_CHILD | BS_CHECKBOX,
		5, 75, 85, 20,
		hWnd, (HMENU)IDC_CHECKBOX_LOOPBACK, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
	CheckDlgButton(hWnd, IDC_CHECKBOX_LOOPBACK, checked);

	if (VoipAttr.AecOn)  checked = BST_CHECKED;
	else checked = BST_UNCHECKED;

	CreateWindow(_T("button"), _T("AEC"),
		WS_VISIBLE | WS_CHILD | BS_CHECKBOX,
		95, 75, 50, 20,
		hWnd, (HMENU)IDC_CHECKBOX_AEC, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
	CheckDlgButton(hWnd, IDC_CHECKBOX_AEC, checked);

	if (VoipAttr.NoiseSuppressionOn)  checked = BST_CHECKED;
	else checked = BST_UNCHECKED;

	CreateWindow(_T("button"), _T("Noise Suppression"),
		WS_VISIBLE | WS_CHILD | BS_CHECKBOX,
		150, 75, 145, 20,
		hWnd, (HMENU)IDC_CHECKBOX_NS, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
	CheckDlgButton(hWnd, IDC_CHECKBOX_NS, checked);

	hWndEdit = CreateWindow(_T("edit"), NULL,
		WS_CHILD | WS_BORDER | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_READONLY,
		0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT, hInst, NULL);

	// Create 1 sub window
	CreateWindow(_T("STATIC"),
		_T("Cam"),
		WS_VISIBLE | WS_CHILD,
		5, 100, 900, 20,
		hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
	hCamWnd = CreateWindow(_T("STATIC"), NULL,
		WS_VISIBLE | WS_CHILD,
		5, 120, 900, 350,
		hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
	
	// Create 2 sub window (ListBox)
	CreateContactListWindow(hWnd, message, wParam, lParam, { 910, 100, 450, 300 });
	
	// Create 3 sub window (EditBox Call History)
	CreateCallHistoryWindow(hWnd, message, wParam, lParam, { 910, 400, 450, 190 });
	
	// Create 4 sub window (EditBox Call Status)
	CreateCallStatusWindow(hWnd, message, wParam, lParam, { 5, 475, 900, 115 });

	hWndMainToolbar = CreateSimpleToolbar(hWnd);

	hWndMain = hWnd;
	InitializeImageDisplay(hCamWnd);
	return 1;
}

LRESULT OnSize(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int cxClient, cyClient;

	cxClient = LOWORD(lParam);
	cyClient = HIWORD(lParam);

	MoveWindow(hWndEdit, 5, cyClient - 130, cxClient - 10, 120, TRUE);

	return DefWindowProc(hWnd, message, wParam, lParam);
}

static int OnConnect(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CStringW cstring(RemoteAddress);

	PRINT(_T("Remote Address : %s Loopback %s\r\n"), cstring, Loopback ? _T("True") : _T("False"));

	if (!IsVideoClientRunning())
	{
		if (OpenCamera())
		{
			if (ConnectToSever(RemoteAddress, VIDEO_PORT))
			{
				BOOST_LOG_TRIVIAL(info) << "Connected to Server";
				StartVideoClient();
				BOOST_LOG_TRIVIAL(info) << "Video Client Started..";
				VoipVoiceStart(RemoteAddress, VOIP_LOCAL_PORT, VOIP_REMOTE_PORT, VoipAttr);
				BOOST_LOG_TRIVIAL(info) << "Voip Voice Started..";

				SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				BOOST_LOG_TRIVIAL(info) << "Enable window alway on top.";

				// Update call status
				updateCallStatus(RemoteAddress);
				return 1;
			}
			else
			{
				DisplayMessageOkBox("Connection Failed!");
				return 0;
			}
		}
		else
		{
			BOOST_LOG_TRIVIAL(error) << "Open Camera Failed";
			return 0;
		}
	}
	return 0;
}

static int OnDisconnect(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (IsVideoClientRunning())
	{
		VoipVoiceStop();
		StopVideoClient();
		CloseCamera();
		MessageBox(hWnd, L"Video Call ended.", L"Alarm", MB_OK);
		updateCallHistory(RemoteAddress, true);
        BOOST_LOG_TRIVIAL(info) << "Video Client Stopped";
	}
	else if (IsVideoServerRunning())
	{
		ClosedConnection();
		BOOST_LOG_TRIVIAL(info) << "ClosedConnection";
	}

	WriteToCallStatusEditBox(_T("[ Waiting... ]"));

	SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	BOOST_LOG_TRIVIAL(info) << "Disable window alway on top.";
	return 1;
}

static int PRINT(const TCHAR* fmt, ...)
{
	va_list argptr;
	TCHAR buffer[2048];
	int cnt;

	int iEditTextLength;
	HWND hWnd = hWndEdit;

	if (NULL == hWnd) return 0;

	va_start(argptr, fmt);

	cnt = wvsprintf(buffer, fmt, argptr);

	va_end(argptr);

	iEditTextLength = GetWindowTextLength(hWnd);
	if (iEditTextLength + cnt > 30000)       // edit text max length is 30000
	{
		SendMessage(hWnd, EM_SETSEL, 0, 10000);
		SendMessage(hWnd, WM_CLEAR, 0, 0);
		PostMessage(hWnd, EM_SETSEL, 0, 10000);
		iEditTextLength = iEditTextLength - 10000;
	}
	SendMessage(hWnd, EM_SETSEL, iEditTextLength, iEditTextLength);
	SendMessage(hWnd, EM_REPLACESEL, 0, (LPARAM)buffer);
	return(cnt);
}

static int OnStartServer(HWND, UINT, WPARAM, LPARAM)
{
	StartVideoServer(Loopback);
	return 0;
}

static int OnStopServer(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	StopVideoServer();
	return 0;
}

static void DisplayMessageOkBox(const char* Msg)
{
	int msgboxID = MessageBoxA(
		NULL,
		Msg,
		"Information",
		MB_OK
	);

	switch (msgboxID)
	{
	case IDCANCEL:
		// TODO: add code
		break;
	case IDTRYAGAIN:
		// TODO: add code
		break;
	case IDCONTINUE:
		// TODO: add code
		break;
	}

}

static bool OnlyOneInstance(void)
{
	HANDLE m_singleInstanceMutex = CreateMutex(NULL, TRUE, L"F2CBD5DE-2AEE-4BDA-8C56-D508CFD3F4DE");
	if (m_singleInstanceMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS)
	{
		HWND existingApp = FindWindow(0, szTitle);
		if (existingApp)
		{
			ShowWindow(existingApp, SW_NORMAL);
			SetForegroundWindow(existingApp);
		}
		return false;
	}
	return true;
}
//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------