#include "MemberUpdate.h"

extern std::string accessToken;

std::string newEmail = "";
bool checkDuplicateEmail = false;

bool updatePasswordEmail(const std::string& password, const std::string& newPassword,
    const std::string& confirmPassword, const std::string& email, const std::string& otp) {

    unsigned int rc = 0;
    std::string data;
    std::string api = "/api/user/update/";
    std::string sessionToken = accessToken;

    // Append current password
    if (!password.empty()) {
        data += "current_password=" + password + "&";
    }

    // Append new password and confirmation password
    if (!newPassword.empty() && !confirmPassword.empty()) {
        data += "new_password=" + newPassword + "&";
        data += "confirm_new_password=" + confirmPassword + "&";
    }

    // Append new email
    if (!email.empty()) {
        data += "new_email=" + email + "&";
    }

    // Append otp
    data += "otp=" + otp;

    rc = sendPostRequest(api, data, sessionToken);
    if (rc == 200) {
        return true;
    }

    return false;
}

bool getOTP(HWND hDlg)
{
    unsigned int rc = 0;

    std::string api = "/api/user/generate-otp";
    std::string sessionToken = accessToken;
   
    rc = sendGetRequest(api, sessionToken);
    if (rc == 200) {

        // Display a notification window if the update is successful
        MessageBox(hDlg, TEXT("Please enter the OTP code that has been sent to your email"), TEXT("Get OTP"), MB_OK | MB_ICONINFORMATION);

        EnableWindow(GetDlgItem(hDlg, IDC_UPDATE_B_GEN_OTP), FALSE);
        inActivateTextBox(hDlg, IDC_UPDATE_E_OTP, true);
        SendMessage(GetDlgItem(hDlg, IDC_UPDATE_E_OTP), EM_LIMITTEXT, 6, 0);

        // Start the countdown.
        std::thread countdownThread(ShowCountdown, hDlg, IDC_UPDATE_T_OTP_TIME);
        countdownThread.detach();

        EnableWindow(GetDlgItem(hDlg, IDC_UPDATE_B_GEN_OTP), TRUE);
    }
    else {
        return false;
    }

    return true;
}

// Function to perform update logic
bool performUpdate(HWND hDlg)
{
    bool changePassword = false;
    bool changePEmail = false;

    // Get password text
    std::string password;
    if (!getControlText(hDlg, IDC_UPDATE_E_CURRENT_PW, password))
    {
        BOOST_LOG_TRIVIAL(error) << "Password is empty";
        MessageBox(hDlg, TEXT("Please enter your current password"), TEXT("Password Error"), MB_OK | MB_ICONERROR);
        return false;
    }
    else if (!validatePassword(password)) {
        BOOST_LOG_TRIVIAL(error) << "password is invalid";
        MessageBox(hDlg, TEXT("Password is invalid"), TEXT("Password Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    // Get new password text
    std::string newPassword;
    bool isPasswordEntered = getControlText(hDlg, IDC_UPDATE_E_NEW_PW, newPassword);

    // Get confirmation password text
    std::string confirmPassword;
    bool isConfirmPasswordEntered = getControlText(hDlg, IDC_UPDATE_E_CONFIRM_NEW_PW, confirmPassword);

    if (isPasswordEntered ^ isConfirmPasswordEntered)
    {
        BOOST_LOG_TRIVIAL(error) << "Please enter both new password and confirmation password";
        MessageBox(hDlg, TEXT("Please enter both new password and confirmation password"), TEXT("Password Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    if (isPasswordEntered && isConfirmPasswordEntered)
    {
        if (!validatePassword(newPassword)) {
            BOOST_LOG_TRIVIAL(error) << "New password is invalid";
            MessageBox(hDlg, TEXT("New password is invalid"), TEXT("Password Error"), MB_OK | MB_ICONERROR);
            return false;
        }
        if (!validatePassword(confirmPassword)) {
            BOOST_LOG_TRIVIAL(error) << "Confirmation password is invalid";
            MessageBox(hDlg, TEXT("Confirmation password is invalid"), TEXT("Password Error"), MB_OK | MB_ICONERROR);
            return false;
        }
        if (newPassword != confirmPassword)
        {
            BOOST_LOG_TRIVIAL(error) << "Password and confirmation password do not match";
            MessageBox(hDlg, TEXT("New password and confirmation password do not match"), TEXT("Password Error"), MB_OK | MB_ICONERROR);
            return false;
        }
        changePassword = true;
    }

    // Get new email text
    std::string email;

    if (!getControlText(hDlg, IDC_UPDATE_E_NEW_EMAIL, email))
    {
        if (!changePassword)
        {
            BOOST_LOG_TRIVIAL(error) << "Please enter a new password or email";
            MessageBox(hDlg, TEXT("Please enter a new password or email"), TEXT("Error"), MB_OK | MB_ICONERROR);
            return false;
        }
    }
    else
    {
        // Check if the email is a duplicate
        // Check if the email matches the newEmail
        if (!checkDuplicateEmail || (email != newEmail))
        {
            checkDuplicateEmail = false;
            newEmail = "";
            BOOST_LOG_TRIVIAL(error) << "Please click the [Duplicate Check] button";
            MessageBox(hDlg, TEXT("Please click the [Duplicate Check] button"), TEXT("Error"), MB_OK | MB_ICONERROR);
            return false;
        }
    }

    // Get OTP text
    std::string otp;
    if (!getControlText(hDlg, IDC_UPDATE_E_OTP, otp))
    {
        BOOST_LOG_TRIVIAL(error) << "Please enter OTP";
        MessageBox(hDlg, TEXT("Please enter OTP"), TEXT("OTP Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    if (!updatePasswordEmail(password, newPassword, confirmPassword, email, otp))
    {
        BOOST_LOG_TRIVIAL(error) << "You cannot update your password or email";
        MessageBox(hDlg, TEXT("You cannot update your password or email"), TEXT("Update Error"), MB_OK | MB_ICONERROR);
        return false;
    }


    return true;
}

// Message handler for Login box.
INT_PTR CALLBACK Update(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            // Perform update
            if (performUpdate(hDlg))
            {
                // Display a notification window if the update is successful
                BOOST_LOG_TRIVIAL(info) << "Update successful!";
                MessageBox(hDlg, TEXT("Update successful!"), TEXT("Success"), MB_OK | MB_ICONINFORMATION);
                accessToken = "";
                // Close the dialog if the update is successful
                EndDialog(hDlg, IDOK);
            }
            else
            {
                // Handling for invalid input or failed update
                return FALSE;
            }

            return TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            // Close the dialog if the cancel button is pressed
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDC_UPDATE_B_CHECK_NEW_EMAIL)
        {
            // Call the OnButtonOTPClick function when the IDC_UPDATE_BUTTON_DUPLICATE button is clicked
            std::string email;
            checkDuplicateEmail = checkEmail(hDlg, IDC_UPDATE_E_NEW_EMAIL, email);
            if (checkDuplicateEmail)
            {
                newEmail = email;
            }
            
            return TRUE;
        }
        else if (LOWORD(wParam) == IDC_UPDATE_B_GEN_OTP)
        {
            // Call the getOTP function when the IDC_UPDATE_BUTTON_OTP button is clicked
            getOTP(hDlg);

            return TRUE;
        }
        break;
    }

    return FALSE;
}
