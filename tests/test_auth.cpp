#include <gtest/gtest.h>
#include "../src/AuthService.h"
#include "../src/AuthMiddleware.h"
#include "../src/AuthUtils.h"
#include "../src/Logger.h"

class AuthTest : public ::testing::Test {
protected:
    static void SetUpTestCase() {
        const char* existing_key = std::getenv("JWT_SECRET_KEY");
        if (existing_key) {
            saved_key = existing_key;
        }
        
        setenv("JWT_SECRET_KEY", "test_secret_key_32_bytes_long_12345678", 1);
    }
    
    static void TearDownTestCase() {
        if (!saved_key.empty()) {
            setenv("JWT_SECRET_KEY", saved_key.c_str(), 1);
        }
    }
     static std::string saved_key;
};

std::string AuthTest::saved_key = "";

// Тест 1: Проверка извлечения токена
TEST_F(AuthTest, ExtractToken) {
    // Пустой заголовок
    EXPECT_EQ(AuthMiddleware::extractToken(""), "");
    
    // Корректный Bearer токен
    std::string token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9";
    std::string auth_header = "Bearer " + token;
    EXPECT_EQ(AuthMiddleware::extractToken(auth_header), token);
    
    // Неправильный префикс
    EXPECT_EQ(AuthMiddleware::extractToken("Basic " + token), "");
    
    // Токен без пробела
    EXPECT_EQ(AuthMiddleware::extractToken("Bearer" + token), "");
}

TEST_F(AuthTest, RequireAuth) {
    int orig_id = 100;
    std::string orig_username = "authuser";
    std::string token = AuthService::generateToken(orig_id, orig_username);
    std::string auth_header = "Bearer " + token;
    
    // Тест с валидным токеном
    int user_id;
    std::string username;
    bool result = AuthMiddleware::requireAuth(auth_header, user_id, username);
    EXPECT_TRUE(result);
    EXPECT_EQ(user_id, orig_id);
    EXPECT_EQ(username, orig_username);
    result = AuthMiddleware::requireAuth("", user_id, username);
    EXPECT_FALSE(result);
    // Тест с невалидным токеном
    result = AuthMiddleware::requireAuth("Bearer 545466jkllkjh1", user_id, username);
    EXPECT_FALSE(result);
}

TEST_F(AuthTest, ValidatePasswordStrength) {
    string passw = "qw12";
    bool result = AuthMiddleware::validatePasswordStrength(passw);
    EXPECT_FALSE(result);

    passw = "45544352315";
    result = AuthMiddleware::validatePasswordStrength(passw);
    EXPECT_FALSE(result);

    passw = "gsyrdggdg";
    result = AuthMiddleware::validatePasswordStrength(passw);
    EXPECT_FALSE(result);

    passw = "dfdgj142";
    result = AuthMiddleware::validatePasswordStrength(passw);
    EXPECT_TRUE(result);
}

TEST_F(AuthTest, GenerateAndValidateToken) {
    std::string token = AuthService::generateToken(42, "adryt");
    EXPECT_FALSE(token.empty());

    int extracted_id;
    std::string extracted_username;
    
    bool valid = AuthService::validateToken(token, extracted_id, extracted_username);
    EXPECT_TRUE(valid);
    EXPECT_EQ(extracted_id, 42);
    EXPECT_EQ(extracted_username, "adryt");
}

TEST_F(AuthTest, HashPassword) {
    std::string password = "testpassword123";
    std::string salt = AuthService::generateSalt();
    std::string hash1 = AuthService::hashPassword(password, salt);
    std::string hash2 = AuthService::hashPassword(password, salt);
    EXPECT_EQ(hash1, hash2); 
    sleep(1);
    std::string salt2 = AuthService::generateSalt();
    std::string hash3 = AuthService::hashPassword(password, salt2);
    EXPECT_NE(hash1, hash3);



    EXPECT_EQ(hash1.length(), 64);
    EXPECT_EQ(hash3.length(), 64);
}

// Тест 6: Генерация соли
TEST_F(AuthTest, SaltGeneration) {
    std::string salt1 = AuthService::generateSalt();
    sleep(1);
    std::string salt2 = AuthService::generateSalt();
    
    EXPECT_FALSE(salt1.empty());
    EXPECT_FALSE(salt2.empty());
    EXPECT_NE(salt1, salt2);
    EXPECT_EQ(salt1.length(), 16);
    EXPECT_EQ(salt2.length(), 16);
}

// Тест 7: AuthUtils функции
TEST_F(AuthTest, AuthUtils) {
    // Генерируем токен
    int original_id = 101;
    std::string original_username = "utiluser";
    std::string token = AuthService::generateToken(original_id, original_username);
    std::string auth_header = "Bearer " + token;
    
    // Тест authenticate
    auto result = AuthUtils::authenticate(auth_header);
    EXPECT_TRUE(result.is_authenticated);
    EXPECT_EQ(result.user_id, original_id);
    EXPECT_EQ(result.username, original_username);
    EXPECT_TRUE(result.error_message.empty());
        
    // Тест checkAuth
    int user_id;
    std::string username;
    std::string error;
    bool check = AuthUtils::checkAuth(auth_header, user_id, username, error);
    EXPECT_TRUE(check);
    EXPECT_EQ(user_id, original_id);
    EXPECT_EQ(username, original_username);
    EXPECT_TRUE(error.empty());
    
    // Тест с пустым заголовком
    check = AuthUtils::checkAuth("", user_id, username, error);
    EXPECT_FALSE(check);
    EXPECT_FALSE(error.empty());
}

