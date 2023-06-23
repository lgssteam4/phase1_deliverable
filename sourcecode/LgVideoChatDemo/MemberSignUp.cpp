#include "MemberSignUp.h"

static char signUpRemoteAddress[512] = "127.0.0.1";
char signUpLocalIpAddress[512] = "127.0.0.1";
std::string signUpEmail = "";
bool checkDuplicateSignUpEmail = false;

bool startsWith(const std::string& str, const std::string& prefix) {
    if (str.length() < prefix.length()) {
        return false;
    }

    return str.compare(0, prefix.length(), prefix) == 0;
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

            if (startsWith(_address, "192.168.0."))
            {
                strcpy_s(signUpRemoteAddress, sizeof(signUpRemoteAddress), _address);
                strcpy_s(signUpLocalIpAddress, sizeof(signUpLocalIpAddress), _address);
                BOOST_LOG_TRIVIAL(info) << "RemoteAddress : " << signUpRemoteAddress << "LocalIpAddress : " << signUpLocalIpAddress;
                break;
            }
        }
    }
}

void EnableEdit(HWND hDlg, BOOL bEnable)
{
    HWND hEditPassOTP = GetDlgItem(hDlg, IDC_SIGNUP_E_PW);
    HWND hEditCPassOTP = GetDlgItem(hDlg, IDC_SIGNUP_E_CONFIRM_PW);
    HWND hEditFNameOTP = GetDlgItem(hDlg, IDC_SIGNUP_E_FIRST_NAME);
    HWND hEditLNameOTP = GetDlgItem(hDlg, IDC_SIGNUP_E_LAST_NAME);
    HWND hEditAddrOTP = GetDlgItem(hDlg, IDC_SIGNUP_E_IP_ADDRESS);

    // IDC_LOGIN_E_OTP Edit 창을 활성화
    EnableWindow(hEditPassOTP, bEnable);
    EnableWindow(hEditCPassOTP, bEnable);
    EnableWindow(hEditFNameOTP, bEnable);
    EnableWindow(hEditLNameOTP, bEnable);
    EnableWindow(hEditAddrOTP, bEnable);

}

// Function to perform SignUp logic
bool PerformSignUp(HWND hDlg)
{
    // Get email text
    std::string email;
    if (!getControlText(hDlg, IDC_SIGNUP_E_EMAIL, email))
    {
        BOOST_LOG_TRIVIAL(error) << "Email is empty";
        MessageBox(hDlg, TEXT("Email is empty"), TEXT("Email Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    // Check if the email is a duplicate
    // Check if the email matches the newEmail
    if (!checkDuplicateSignUpEmail || (email != signUpEmail))
    {
        checkDuplicateSignUpEmail = false;
        signUpEmail = "";
        BOOST_LOG_TRIVIAL(error) << " click the [Duplicate Check] button";
        MessageBox(hDlg, TEXT("Please click the [Duplicate Check] button"), TEXT("Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    // Get password text
    std::string password;
    if (!getControlText(hDlg, IDC_SIGNUP_E_PW, password))
    {
        BOOST_LOG_TRIVIAL(error) << "Password is empty";
        MessageBox(hDlg, TEXT("Password is empty"), TEXT("Password Error"), MB_OK | MB_ICONERROR);
        return false;
    }
    else if (!validatePassword(password)) {
        BOOST_LOG_TRIVIAL(error) << "Password is invalid";
        MessageBox(hDlg, TEXT("Password is invalid"), TEXT("Password Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    // Get confirm password text
    std::string cPassword;
    if (!getControlText(hDlg, IDC_SIGNUP_E_CONFIRM_PW, cPassword))
    {
        BOOST_LOG_TRIVIAL(error) << "Confirm password is empty";
        MessageBox(hDlg, TEXT("Confirm password is empty"), TEXT("Password Error"), MB_OK | MB_ICONERROR);
        return false;
    }
    else if (!validatePassword(cPassword)) {
        BOOST_LOG_TRIVIAL(error) << "Confirm password is invalid";
        MessageBox(hDlg, TEXT("Confirm password is invalid"), TEXT("Password Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    // Compare the entered password with loginPassword
    if (password != cPassword)
    {
        BOOST_LOG_TRIVIAL(error) << "Password and confirmation password do not match";
        MessageBox(hDlg, TEXT("Password and confirmation password do not match"), TEXT("Password Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    // Get first name text
    std::string fName;
    if (!getControlText(hDlg, IDC_SIGNUP_E_FIRST_NAME, fName))
    {
        BOOST_LOG_TRIVIAL(error) << "First name is empty";
        MessageBox(hDlg, TEXT("First name is empty"), TEXT("First Name Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    // Get last name text
    std::string lName;
    if (!getControlText(hDlg, IDC_SIGNUP_E_LAST_NAME, lName))
    {
        BOOST_LOG_TRIVIAL(error) << "Last name is empty";
        MessageBox(hDlg, TEXT("Last name is empty"), TEXT("Last Name Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    // Get IP address text
    std::string addr;
    if (!getControlText(hDlg, IDC_SIGNUP_E_IP_ADDRESS, addr))
    {
        BOOST_LOG_TRIVIAL(error) << "IP address is empty";
        MessageBox(hDlg, TEXT("IP address is empty"), TEXT("IP Address Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    // password, cpassword, email, fName, lName, addr
    std::string requestString = "email=" + email + "&password=" + password + "&confirm_password=" + cPassword + "&ip_address=" + addr + "&first_name=" + fName + "&last_name=" + lName;

    unsigned int status_code;
    std::map<std::string, std::string> response;
    int rc = request("POST", "/api/user/signup/", requestString, "", &status_code, response);

    if (rc != 0 || status_code != 200)
    {
        BOOST_LOG_TRIVIAL(error) << "rc =" << rc << " status_code = " << status_code;
        BOOST_LOG_TRIVIAL(error) << "Error is occurred, Please check the input value";
        MessageBox(hDlg, TEXT("Error is occurred, Please check the input value"), TEXT("Error from server"), MB_OK | MB_ICONERROR);
        return false;
    }

    BOOST_LOG_TRIVIAL(info) << response["message"];
    std::string successMessage = response["message"];

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring wstr = converter.from_bytes(successMessage);

    BOOST_LOG_TRIVIAL(info) << "User created successfully";
    MessageBox(hDlg, wstr.c_str(), TEXT("Sign-In Success"), MB_OK | MB_ICONINFORMATION);

    return true;
}

void setIPAddr(HWND hDlg)
{
    HWND hAddrEdit = GetDlgItem(hDlg, IDC_SIGNUP_E_IP_ADDRESS);
    SetHostAddr();
    SetWindowTextA(hAddrEdit, signUpRemoteAddress);
}

// Message handler for SignUp box.
INT_PTR CALLBACK SignUp(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        setIPAddr(hDlg);
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            if (PerformSignUp(hDlg))
            {
                EndDialog(hDlg, IDOK);
            }
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            // Close the dialog when the Cancel button is pressed
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        else if (LOWORD(wParam) == IDC_SIGNUP_B_CHECK_EMAIL)
        {
            SetHostAddr();
            /// Call the OnButtonOTPClick function when the IDC_SIGNUP_B_CHECK_EMAIL button is clicked
            std::string email;
            checkDuplicateSignUpEmail = checkEmail(hDlg, IDC_SIGNUP_E_EMAIL, email);
            if (checkDuplicateSignUpEmail)
            {
                signUpEmail = email;
                EnableEdit(hDlg, TRUE);
            }
            else
            {
                EnableEdit(hDlg, FALSE);
            }
            return TRUE;
        }
        break;
    }

    return FALSE;
}
