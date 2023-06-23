#pragma once

#include "BoostLog.h"
#include "framework.h"
#include <Windows.h>
#include <string>
#include <regex>
#include "BackendHttpsClient.h"

// Function to check if the given email is a duplicate
bool isDuplicateEmail(const std::string& email);

// Function to check if the given email is valid
bool isValidEmail(const std::string& email);

// Function to perform email validation and duplicate check
bool checkEmail(HWND hDlg, unsigned int textBox, std::string& email);

// Function to validate password
bool validatePassword(const std::string& password);

// The GetEditText function retrieves the text from a specified window control
std::string GetEditText(HWND hEdit);

// The getControlText function retrieves the text from a specified window control.
bool getControlText(HWND hDlg, int controlID, std::string& text);

// Function to display the countdown
void ShowCountdown(HWND hDlg, unsigned int textBox);

// Function to get email text
bool getEmailText(HWND hDlg, unsigned int textBox, std::string& email);

// Function to get password text
bool getPasswordText(HWND hDlg, unsigned int textBox, std::string& password);

// Function to activate the textbox
void inActivateTextBox(HWND hDlg, unsigned int textBox, bool active);


//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------
