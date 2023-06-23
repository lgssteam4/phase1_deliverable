#include "BoostLog.h"
#include "ContactList.h"
#include "eWID.h"

#include <string>
#include <regex>
#include "BackendHttpsClient.h"

extern std::string accessToken;

static HWND hContactListWnd;
static void AddContactListInfo(void);
static std::wstring ExtractIPAddress(const std::wstring& input);
static void ConvertToWideCharArray(const std::wstring& input, wchar_t* output, std::size_t size);
static LPSTR ConvertWideCharToLPSTR(const wchar_t* wideCharString);

LRESULT CreateContactListWindow(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, RECT rt)
{
	const LONG offset = 20;

	// Label
	CreateWindow(_T("STATIC"),
		_T("Contact List"),
		WS_VISIBLE | WS_CHILD,
		rt.left, rt.top, rt.right, offset,
		hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
	// listbox
	hContactListWnd = CreateWindow(_T("LISTBOX"), NULL,
		WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL,
		rt.left, rt.top + offset, rt.right, rt.bottom - offset,
		hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

	return 1;
}

LRESULT DoubleClickContactListEventHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND hEditWnd;
	HWND hListBox = (HWND)lParam;

	int selectedIndex = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
	if (selectedIndex != LB_ERR)
	{
		wchar_t address[256], ipaddr[256];
		SendMessage(hListBox, LB_GETTEXT, selectedIndex, (LPARAM)address);
		ConvertToWideCharArray(ExtractIPAddress(std::wstring(address)), ipaddr, 256);

		// Update Text to EditBox
		hEditWnd = GetDlgItem(hWnd, IDC_EDIT_REMOTE);		
		SetWindowTextA(hEditWnd, ConvertWideCharToLPSTR(ipaddr));
	}

	return 0;
}

// Get user information
bool queryAllUserInfo(std::map<std::string, std::string>& userInfo) {
	unsigned int rc = 0;
	std::string api = "/api/user/all/";
	std::string sessionToken = accessToken;

	rc = sendGetRequest(api, sessionToken, userInfo);
	if (rc == 200) {
		return true;
	}

	return false;
}

void UpdateContactList(void)
{
	BOOST_LOG_TRIVIAL(debug) << "Start UpdateContactList: ";
	std::map<std::string, std::string> response;
	std::vector<const wchar_t*> contactList = {};

	if (queryAllUserInfo(response))
	{
		BOOST_LOG_TRIVIAL(debug) << "Success queryAllUserInfo";
		for (const auto& pair : response) {
			BOOST_LOG_TRIVIAL(debug) << pair.first << ": " << pair.second;
			std::wstring valueWStr = L" " + std::wstring(pair.second.begin(), pair.second.end());
			const wchar_t* value = _wcsdup(valueWStr.c_str());
			contactList.push_back(value);
		}
	}
	else
	{
		BOOST_LOG_TRIVIAL(debug) << "Error queryAllUserInfo";
		std::wstring valueWStr = L"Failed to get contact list from DB.";
		const wchar_t* errorMessage = _wcsdup(valueWStr.c_str());
		contactList.push_back(errorMessage);
	}

	for (const wchar_t* contact : contactList) {
		SendMessage(hContactListWnd, LB_ADDSTRING, 0, (LPARAM)contact);
	}

	// contactList에 할당된 메모리 해제
	for (const wchar_t* contact : contactList) {
		free((void*)contact);
	}

	BOOST_LOG_TRIVIAL(debug) << "End UpdateContactList";
}

static std::wstring ExtractIPAddress(const std::wstring& input)
{
	// IP 주소 추출을 위한 정규식 패턴
	std::wregex pattern(L"\\b(?:\\d{1,3}\\.){3}\\d{1,3}\\b");

	// 정규식 패턴과 매치되는 첫 번째 IP 주소 추출
	std::wsmatch matches;
	if (std::regex_search(input, matches, pattern))
	{
		// 매치된 IP 주소 반환
		return matches[0].str();
	}

	// 매치된 IP 주소가 없을 경우 빈 문자열 반환
	return L"";
}

static void ConvertToWideCharArray(const std::wstring& input, wchar_t* output, std::size_t size)
{
	if (output != nullptr && size > 0)
	{
		wcsncpy_s(output, size, input.c_str(), _TRUNCATE);
	}
}

static LPSTR ConvertWideCharToLPSTR(const wchar_t* wideCharString)
{
	int bufferSize = WideCharToMultiByte(CP_ACP, 0, wideCharString, -1, nullptr, 0, nullptr, nullptr);
	if (bufferSize == 0)
	{
		// Failed to determine the required buffer size
		return nullptr;
	}

	LPSTR lpstrString = new CHAR[bufferSize];
	if (WideCharToMultiByte(CP_ACP, 0, wideCharString, -1, lpstrString, bufferSize, nullptr, nullptr) == 0)
	{
		// Failed to convert wide character string to LPSTR
		delete[] lpstrString;
		return nullptr;
	}

	return lpstrString;
}