#ifndef AUTHMIDDLEWARE_H
#define AUTHMIDDLEWARE_H

#include "AuthService.h"
#include <cctype>  
#include <iostream> 
#include <string>
#include <map>
#include "Logger.h"

using namespace std;

class AuthMiddleware {
public:
     static string extractToken(const string& auth_header) {
        if (auth_header.empty()) {
            return "";
        }
        
        const string bearer_prefix = "Bearer ";
        if (auth_header.find(bearer_prefix) == 0) {
            return auth_header.substr(bearer_prefix.length());
        }
        
        return "";
    }
     static bool requireAuth(const string& auth_header, int& user_id, string& username) {
        string token = extractToken(auth_header);
        
        if (token.empty()) {
            app_logger.error("No token provided in Authorization header");
            return false;
        }
        app_logger.info("Validating token: " + token.substr(0, 20) + "...");
        return AuthService::validateToken(token, user_id, username);
    }

    static string authErrorResponse(const string& message = "Authentication required") {
        return "{\"error\":\"" + message + "\",\"code\":401}";
    }
    
    // Проверка пароля на сложность
    static bool validatePasswordStrength(const string& password) {
        if (password.length() < 6) {
            return false;
        }
        
        bool has_letter = false;
        bool has_digit = false;
        
        for (char c : password) {
            if (isalpha(c)) has_letter = true;
            if (isdigit(c)) has_digit = true;
        }
        
        return has_letter && has_digit;
    }

};

#endif