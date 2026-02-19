#include "AuthService.h"


const string AuthService::SECRET_KEY = []() {
    const char* key = std::getenv("JWT_SECRET_KEY");
    if (!key || strlen(key) < 32) {
        throw std::runtime_error("Invalid or missing JWT_SECRET_KEY");
    }
    return std::string(key);
}();

const int AuthService::TOKEN_EXPIRY_HOURS = 24;