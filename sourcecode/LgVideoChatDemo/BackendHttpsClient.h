#pragma once
#include <iostream>
#include <map>
#include <string>

int request(std::string request_method, std::string uri, std::string session_token, unsigned int* status_code, std::map<std::string, std::string>& response);
int request(std::string request_method, std::string uri, std::string data, std::string session_token, unsigned int* status_code, std::map<std::string, std::string>& response);
unsigned int sendGetRequest(const std::string& function, const std::string& sessionToken);
unsigned int sendGetRequest(const std::string& function, const std::string& sessionToken, std::map<std::string, std::string>& response);
unsigned int sendPostRequest(const std::string& function, const std::string& data, const std::string& sessionToken);
unsigned int sendPostRequest(const std::string& function, const std::string& data, const std::string& sessionToken, std::map<std::string, std::string>& response);