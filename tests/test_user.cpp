#include <gtest/gtest.h>
#include "../src/User.h"

class UserTest : public ::testing::Test {
protected:
    void SetUp() override {
        user1 = User("testuser", "hash123", "salt123");
        user1.id = 42;
    }
    
    User user1;
};

// Тест 1: Конструкторы пользователей
TEST_F(UserTest, Constructors) {
    // Проверка конструктора по умолчанию
    User empty;
    EXPECT_EQ(empty.id, 0);
    EXPECT_TRUE(empty.username.empty());
    EXPECT_TRUE(empty.password_hash.empty());
    EXPECT_TRUE(empty.salt.empty());
    
    // Проверка параметризованного конструктора
    EXPECT_EQ(user1.id, 42);
    EXPECT_EQ(user1.username, "testuser");
    EXPECT_EQ(user1.password_hash, "hash123");
    EXPECT_EQ(user1.salt, "salt123");
    EXPECT_GT(user1.created_at.length(), 0);
}

// Тест 2: JSON сериализация
TEST_F(UserTest, ToJSON) {
    std::string json = user1.to_json();
    
    EXPECT_TRUE(json.find("\"id\":42") != std::string::npos);
    EXPECT_TRUE(json.find("\"username\":\"testuser\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"created_at\":\"") != std::string::npos);
    
    EXPECT_TRUE(json.find("password") == std::string::npos);
    EXPECT_TRUE(json.find("salt") == std::string::npos);
}

// Тест 3: JSON с токеном
TEST_F(UserTest, ToJSONWithToken) {
    std::string token = "test.jwt.token";
    std::string json = user1.to_json_with_token(token);
    
    EXPECT_TRUE(json.find("\"id\":42") != std::string::npos);
    EXPECT_TRUE(json.find("\"username\":\"testuser\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"token\":\"test.jwt.token\"") != std::string::npos);
}