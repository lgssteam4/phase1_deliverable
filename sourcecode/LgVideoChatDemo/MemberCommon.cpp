#include "MemberCommon.h"

// Function to check if the given email is valid
bool isValidEmail(const std::string& email)
{
    // Regular expression pattern for email validation
    const std::regex pattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    return std::regex_match(email, pattern);
}

// Function to check if the given email is a duplicate
bool isDuplicateEmail(const std::string& email)
{
    unsigned int rc = 0;

    std::string api = "/api/user/check-email/";
    std::string data = "email=" + email;
    rc = sendPostRequest(api, data, "");
    if (rc == 403 || rc == 400) {
        return true;
    }
    return false;
}

// Function to perform email validation and duplicate check
bool checkEmail(HWND hDlg, unsigned int textBox, std::string &email) {
    // Get new email text

    if (!getControlText(hDlg, textBox, email))
    {
        BOOST_LOG_TRIVIAL(error) << "Email cannot be empty";
        MessageBox(hDlg, TEXT("Email cannot be empty"), TEXT("Email Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    if (!isValidEmail(email)) {
        BOOST_LOG_TRIVIAL(error) << "Invalid email format";
        MessageBox(hDlg, TEXT("Invalid email format"), TEXT("Email Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    if (isDuplicateEmail(email)) {
        BOOST_LOG_TRIVIAL(error) << "This email is unavailable (Duplicate)";
        MessageBox(hDlg, TEXT("This email is unavailable (Duplicate)"), TEXT("Email Error"), MB_OK | MB_ICONERROR);
        return false;
    }
    else {
        MessageBox(hDlg, TEXT("This email is available"), TEXT("Success"), MB_OK | MB_ICONINFORMATION);
    }

    return true;
}

// Function to validate password
bool validatePassword(const std::string& password)
{
    // Check if password length is at least 10 characters
    if (password.length() < 10) {
        return false;
    }

    // Check if password contains at least one digit
    if (!std::regex_search(password, std::regex("\\d"))) {
        return false;
    }

    // Check if password contains at least one uppercase letter
    if (!std::regex_search(password, std::regex("[A-Z]"))) {
        return false;
    }

    // Check if password contains at least one lowercase letter
    if (!std::regex_search(password, std::regex("[a-z]"))) {
        return false;
    }

    // Check if password contains at least one special character
    if (!std::regex_search(password, std::regex("[!@#$%^&*(),.?\":{}|<>]"))) {
        return false;
    }

    // Password passed all validation checks
    return true;
}

// The GetEditText function retrieves the text from a specified window control
std::string GetEditText(HWND hEdit)
{
    int length = GetWindowTextLength(hEdit);
    if (length > 0)
    {
        std::vector<char> buffer(length + 1);
        GetWindowTextA(hEdit, buffer.data(), length + 1);
        return buffer.data();
    }
    return "";
}

// The getControlText function retrieves the text from a specified window control.
bool getControlText(HWND hDlg, int controlID, std::string& text)
{
    HWND hControl = GetDlgItem(hDlg, controlID);
    text = GetEditText(hControl);

    if (text.empty())
    {
        return false;
    }

    return true;
}

// Function to display the countdown
void ShowCountdown(HWND hDlg, unsigned int textBox)
{
    // Seconds representing 1 minute
    int countdownSeconds = 1 * 60;

    while (countdownSeconds >= 0)
    {
        // Calculating minutes and seconds
        int minutes = countdownSeconds / 60;
        int seconds = countdownSeconds % 60;

        // Displaying the countdown in the text box
        std::wstring countdownText = std::to_wstring(minutes / 10) + std::to_wstring(minutes % 10) + L":" +
            std::to_wstring(seconds / 10) + std::to_wstring(seconds % 10);
        SetDlgItemTextW(hDlg, textBox, countdownText.c_str());

        // Waiting for 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Decreasing the countdown
        countdownSeconds--;
    }
}

// Function to get email text
bool getEmailText(HWND hDlg, unsigned int textBox, std::string& email)
{
    if (!getControlText(hDlg, textBox, email))
    {
        BOOST_LOG_TRIVIAL(error) << "Email is empty";
        MessageBox(hDlg, TEXT("Email is empty"), TEXT("Email Error"), MB_OK | MB_ICONERROR);
        return false;
    }
    if (!isValidEmail(email)) {
        BOOST_LOG_TRIVIAL(error) << "Invalid email format";
        MessageBox(hDlg, TEXT("Invalid email format"), TEXT("Email Error"), MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

// Function to get password text
bool getPasswordText(HWND hDlg, unsigned int textBox, std::string& password)
{
    if (!getControlText(hDlg, textBox, password))
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
    return true;
}

// Function to activate the textbox
void inActivateTextBox(HWND hDlg, unsigned int textBox, bool active)
{
    HWND hEditOTP = GetDlgItem(hDlg, textBox);
    EnableWindow(hEditOTP, active);
}
