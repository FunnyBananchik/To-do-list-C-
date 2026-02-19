#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include <string>
#include <cstdlib>
#include "../jwt-cpp/include/jwt-cpp/jwt.h"
#include <openssl/sha.h>
#include <iostream>
#include "Logger.h"

using namespace std;

class AuthService {
private:
    static const string SECRET_KEY;
    static const int TOKEN_EXPIRY_HOURS;

public:
    // Генерация JWT токена
    static string generateToken(int user_id, const string& username) {
        try {
            auto token = jwt::create()
                .set_issuer("todo-app-server")
                .set_type("JWT")
                .set_subject(to_string(user_id))
                .set_payload_claim("username", jwt::claim(username))
                .set_issued_at(chrono::system_clock::now())
                .set_expires_at(chrono::system_clock::now() + 
                               chrono::hours(TOKEN_EXPIRY_HOURS))
                .sign(jwt::algorithm::hs256{SECRET_KEY});
            
            return token;
        } catch (const exception& e) {
            stringstream ss;
            ss << "Error generating JWT: " << e.what();
            app_logger.error(ss.str());
            return "";
        }
    }


     static bool validateToken(const string& token, int& user_id, string& username) {
        try {
            app_logger.debug("Validating JWT token...");
            auto decoded = jwt::decode(token);
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{SECRET_KEY})
                .with_issuer("todo-app-server");
            
            verifier.verify(decoded);
            string subject = decoded.get_subject();
            user_id = stoi(subject);
            username = decoded.get_payload_claim("username").as_string();
            app_logger.info("Token valid for user: " + username + " (ID: " + to_string(user_id) + ")");
            return true;
            
        } catch (const std::exception& e) {
            stringstream ss;
            ss << "Token validation error: " << e.what();
            app_logger.error(ss.str());
            return false;
        }
    }

     // Хэширование пароля с солью
    static string hashPassword(const string& password, const string& salt = "") {
        string salted_password = password + salt + SECRET_KEY;
        
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, salted_password.c_str(), salted_password.length());
        SHA256_Final(hash, &sha256);
        
        // Конвертируем в hex строку
        char hex_hash[65];
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            sprintf(hex_hash + (i * 2), "%02x", hash[i]);
        }
        hex_hash[64] = '\0';
        
        return string(hex_hash);
    }

    static string generateSalt() {
        const char charset[] = 
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        
        string salt;
        srand(time(nullptr));
        for(int i = 0; i < 16; i++) {
            salt += charset[rand() % (sizeof(charset) - 1)];
        }
        return salt;
    }  
};

#endif AUTHSERVICE_H