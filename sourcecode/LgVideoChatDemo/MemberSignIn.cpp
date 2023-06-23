#include "MemberSignIn.h"
#include "ContactList.h"

std::string accessToken;

bool GenerateOTP(HWND hDlg)
{
    unsigned int rc = 0;
    std::map<std::string, std::string> response;
    std::string api = "/api/user/generate-otp";

    // Get email text
    std::string email;
    if (!getEmailText(hDlg, IDC_SIGNIN_E_EMAIL, email)) return FALSE;

    // Get password text
    std::string password;
    if (!getPasswordText(hDlg, IDC_SIGNIN_E_PW, password)) return FALSE;

    std::string data = "email=" + email + "&" + "password=" + password;

    rc = sendPostRequest(api, data, "", response);
    if (rc == 200)
    {
        inActivateTextBox(hDlg, IDC_SIGNIN_E_OTP, true);

        EnableWindow(GetDlgItem(hDlg, IDC_SIGNIN_B_GEN_OTP), FALSE);

        // Start the countdown.
        std::thread countdownThread(ShowCountdown, hDlg, IDC_SIGNIN_T_OTP_TIME);
        countdownThread.detach();

        BOOST_LOG_TRIVIAL(error) << "Please enter the OTP code that has been sent to your email";
        MessageBox(hDlg, TEXT("Please enter the OTP code that has been sent to your email"), TEXT("Generate OTP"), MB_OK | MB_ICONINFORMATION);

        EnableWindow(GetDlgItem(hDlg, IDC_SIGNIN_B_GEN_OTP), TRUE);
        SendMessage(GetDlgItem(hDlg, IDC_SIGNIN_E_OTP), EM_LIMITTEXT, 6, 0);
    }
    else if (rc == 400)
    {
        BOOST_LOG_TRIVIAL(error) << "User doest not exist";
        MessageBox(hDlg, TEXT("User doest not exist"), TEXT("Generate OTP"), MB_OK | MB_ICONERROR);
        return false;
    }
    else if (rc == 403)
    {
        BOOST_LOG_TRIVIAL(error) << "Please proceed with email account activation";
        MessageBox(hDlg, TEXT("Please proceed with email account activation"), TEXT("Generate OTP"), MB_OK | MB_ICONERROR);
        return false;
    }
    else
    {
        //If the user enters the incorrect password more than three times, then their account will be locked for one hour
        std::string message = response["message"];
        std::string errorMsg;
        if (message == "Invalid credentials")
        {
            BOOST_LOG_TRIVIAL(info) << "Invalid credentials";
            //If the user enters the incorrect password more than three times, then their account will be locked for one hour
            std::string remainingAttempts = response["remaining_attempts"];
            if (std::stoi(remainingAttempts) > 0)
            {
                errorMsg = "You entered the wrong password\nIf you are wrong more than " + remainingAttempts + " times, your account will be locked for 1 hour";
            }
            else
            {
                errorMsg = "Your account has been locked for 1 hour\nPlease try again in 1 hour.";
            }
        }
        else
        {
            errorMsg = response["message"];
        }

        BOOST_LOG_TRIVIAL(error) << errorMsg;

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wstr = converter.from_bytes(errorMsg);

        MessageBox(hDlg, wstr.c_str(), TEXT("Generate OTP"), MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

// Function to perform Sign-In logic
bool PerformSignIn(HWND hDlg)
{
    unsigned int rc = 0;
    std::map<std::string, std::string> response;

    // Get email text
    std::string email;
    if (!getEmailText(hDlg, IDC_SIGNIN_E_EMAIL, email)) return false;

    // Get password text
    std::string password;
    if (!getPasswordText(hDlg, IDC_SIGNIN_E_PW, password)) return false;

    // Get OTP text
    std::string input_otp;
    if (!getControlText(hDlg, IDC_SIGNIN_E_OTP, input_otp))
    {
        MessageBox(hDlg, TEXT("Please enter OTP"), TEXT("OTP Error"), MB_OK | MB_ICONERROR);
        return false;
    }
 
    // Send request to verify OTP
    // Check if the entered email, password, and OTP match the predefined values and generated OTP

    std::string api = "/api/auth/login";
    std::string data = "email=" + email + "&" + "password=" + password + "&" + "otp=" + input_otp;
    rc = sendPostRequest(api, data, "", response);
    if (rc == 200)
    {
        accessToken = response["access_token"];
        BOOST_LOG_TRIVIAL(info) << "Sign-In access_token : " << accessToken;
        BOOST_LOG_TRIVIAL(info) << "Sign-In successful!";
        return true;
    }
    else if (rc == 400)
    {
        BOOST_LOG_TRIVIAL(error) << "User doest not exist";
        MessageBox(hDlg, TEXT("User doest not exist"), TEXT("Generate OTP"), MB_OK | MB_ICONERROR);
        return false;
    }
    else
    {
        //If the user enters the incorrect password more than three times, then their account will be locked for one hour
        std::string message = response["message"];
        std::string errorMsg;
        if(message == "Invalid credentials")
        {
            BOOST_LOG_TRIVIAL(info) << "Invalid credentials";
            //If the user enters the incorrect password more than three times, then their account will be locked for one hour
            std::string remainingAttempts = response["remaining_attempts"];
            if (std::stoi(remainingAttempts) > 0)
            {
                errorMsg = "You entered the wrong password\nIf you are wrong more than " + response["remaining_attempts"] + " times, your account will be locked for 1 hour";
            }
            else
            {
                errorMsg = "Your account has been locked for 1 hour\nPlease try again in 1 hour.";
            }
        }
        else
        {
            errorMsg = response["message"];
        }

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wstr = converter.from_bytes(errorMsg);

        BOOST_LOG_TRIVIAL(error) << errorMsg;
        MessageBox(hDlg, wstr.c_str(), TEXT("Generate OTP"), MB_OK | MB_ICONERROR);
        return false;
    }
  
    return false;
}

// Message handler for SignIn box.
INT_PTR CALLBACK SignIn(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            // Perform SignIn
            if (PerformSignIn(hDlg))
            {
                BOOST_LOG_TRIVIAL(info) << "Sign-In successful!";
                MessageBox(hDlg, TEXT("Sign-In successful!"), TEXT("Sign-In Success"), MB_OK | MB_ICONINFORMATION);
                EndDialog(hDlg, IDOK);
                UpdateContactList();
            }
            else
            {
                return FALSE;
            }

            return TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDC_SIGNIN_B_GEN_OTP)
        {
            if (GenerateOTP(hDlg)) return TRUE;

            return FALSE;
        }
        break;
    }

    return FALSE;
}