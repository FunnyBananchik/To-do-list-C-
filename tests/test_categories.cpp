#include <gtest/gtest.h>
#include "../src/Categories.h"
#include <vector>

class CategoriesTest : public ::testing::Test {
protected:
    void SetUp() override {
        cat1 = categories("Work", "#FF0000", 100);
        cat2 = categories("Personal", "#00FF00", 100);
    }
    
    categories cat1;
    categories cat2;
};

// Тест 1: Конструкторы категорий
TEST_F(CategoriesTest, Constructors) {
    // Проверка конструктора с параметрами
    EXPECT_EQ(cat1.name, "Work");
    EXPECT_EQ(cat1.color, "#FF0000");
    EXPECT_EQ(cat1.user_id, 100);
    EXPECT_EQ(cat1.id, -1);  // ID не задан
    
    // Проверка конструктора по умолчанию
    categories empty;
    EXPECT_EQ(empty.id, 0);
    EXPECT_TRUE(empty.name.empty());
    EXPECT_TRUE(empty.color.empty());
    EXPECT_EQ(empty.user_id, 0);
    
    // Проверка конструктора копирования
    categories copy = cat1;
    EXPECT_EQ(copy.name, cat1.name);
    EXPECT_EQ(copy.color, cat1.color);
    EXPECT_EQ(copy.user_id, cat1.user_id);
}

// Тест 2: JSON сериализация категорий
TEST_F(CategoriesTest, ToJson) {
    cat1.id = 5;
    std::string json = cat1.to_json();
    
    EXPECT_TRUE(json.find("\"id\":5") != std::string::npos);
    EXPECT_TRUE(json.find("\"name\":\"Work\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"color\":\"#FF0000\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"user_id\":100") != std::string::npos);
}

// Тест 3: JSON десериализация категорий
TEST_F(CategoriesTest, FromJson) {
    std::string json = R"({
        "id":10,
        "name":"Shopping",
        "color":"#0000FF",
        "user_id":102
    })";
    
    categories cat = categories::cat_from_json(json);
    
    EXPECT_EQ(cat.id, 10);
    EXPECT_EQ(cat.name, "Shopping");
    EXPECT_EQ(cat.color, "#0000FF");
    EXPECT_EQ(cat.user_id, 102);
}
