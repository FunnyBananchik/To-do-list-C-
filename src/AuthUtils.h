#ifndef AUTHUTILS_H
#define AUTHUTILS_H

#include <string>
#include "Database.h"
#include "AuthMiddleware.h"

using namespace std;

namespace AuthUtils {
     // Структура для результата аутентификации
    struct AuthResult {
        bool is_authenticated;
        int user_id;
        string username;
        string error_message;
        
        AuthResult() : is_authenticated(false), user_id(0) {}
        AuthResult(int id, const string& name) 
            : is_authenticated(true), user_id(id), username(name) {}
        AuthResult(const string& error) 
            : is_authenticated(false), user_id(0), error_message(error) {}
    };
    
    // Функция проверки аутентификации
    inline AuthResult authenticate(const string& auth_header) {
        if (auth_header.empty()) {
            return AuthResult("No authorization header");
        }
        
        int user_id;
        string username;
        
        if (AuthMiddleware::requireAuth(auth_header, user_id, username)) {
            return AuthResult(user_id, username);
        }
        
        return AuthResult("Invalid or expired token");
    }
    
    // Функция для быстрой проверки и получения данных пользователя
    inline bool checkAuth(const string& auth_header, int& user_id, string& username, string& error) {
        auto result = authenticate(auth_header);
        
        if (!result.is_authenticated) {
            error = result.error_message;
            return false;
        }
        
        user_id = result.user_id;
        username = result.username;
        return true;
    }
    
    // Функция для обработки ошибки аутентификации
    inline string authError(const string& message = "Authentication required") {
        return "{\"error\":\"" + message + "\",\"code\":401}";
    }
};

#endif