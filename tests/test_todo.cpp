#include <gtest/gtest.h>
#include "../src/Todo.h"
#include <vector>

class TodoTest : public ::testing::Test {
protected:
    void SetUp() override {
        todo1 = Todo("Test Task 1", "Description 1", 2, "2024-12-31", 1, 100);
        todo2 = Todo("Test Task 2", "", 1, "", 0, 100);
    }
    
    Todo todo1;
    Todo todo2;
};

// Тест 1: Конструкторы Todo
TEST_F(TodoTest, Constructors) {
    EXPECT_EQ(todo1.title, "Test Task 1");
    EXPECT_EQ(todo1.description, "Description 1");
    EXPECT_EQ(todo1.priority, 2);
    EXPECT_EQ(todo1.due_date, "2024-12-31");
    EXPECT_EQ(todo1.category_id, 1);
    EXPECT_EQ(todo1.user_id, 100);
    EXPECT_FALSE(todo1.completed);
    EXPECT_GT(todo1.created_at.length(), 0);
    
    Todo empty;
    EXPECT_EQ(empty.id, 0);
    EXPECT_TRUE(empty.title.empty());
    EXPECT_TRUE(empty.description.empty());
    EXPECT_FALSE(empty.completed);
    EXPECT_EQ(empty.priority, 1);
}

// Тест 2: Копирование Todo
TEST_F(TodoTest, CopyConstructor) {
    todo1.id = 100;
    Todo copy(todo1);
    EXPECT_EQ(copy.id, todo1.id);
    EXPECT_EQ(copy.title, todo1.title);
    EXPECT_EQ(copy.description, todo1.description);
    EXPECT_EQ(copy.completed, todo1.completed);
    EXPECT_EQ(copy.priority, todo1.priority);
    EXPECT_EQ(copy.due_date, todo1.due_date);
    EXPECT_EQ(copy.category_id, todo1.category_id);
    EXPECT_EQ(copy.user_id, todo1.user_id);
}

// Тест 3: JSON сериализация
TEST_F(TodoTest, ToJson) {
    todo1.id = 100;
    string result = todo1.to_json();

    EXPECT_TRUE(result.find("\"id\":100") != std::string::npos);
    EXPECT_TRUE(result.find("\"title\":\"Test Task 1\"") != std::string::npos);
    EXPECT_TRUE(result.find("\"description\":\"Description 1\"") != std::string::npos);
    EXPECT_TRUE(result.find("\"completed\":false") != std::string::npos);
    EXPECT_TRUE(result.find("\"priority\":2") != std::string::npos);
    EXPECT_TRUE(result.find("\"due_date\":\"2024-12-31\"") != std::string::npos);
    EXPECT_TRUE(result.find("\"category_id\":1") != std::string::npos);
    EXPECT_TRUE(result.find("\"user_id\":100") != std::string::npos);

    Todo todo3("With \"quotes\"", "With \\ backslash");
    result = todo3.to_json();
    EXPECT_TRUE(result.find("With \\\"quotes\\\"") != std::string::npos);
    EXPECT_TRUE(result.find("With \\\\ backslash") != std::string::npos);
}

// Тест 4: JSON десериализация
TEST_F(TodoTest, FromJson) {
    std::string json = R"({
        "id":55,
        "title":"JSON Task",
        "description":"JSON Description",
        "completed":true,
        "priority":3,
        "due_date":"2026-01-25",
        "category_id":2,
        "user_id":101
    })";
    
    Todo todo = Todo::from_json(json);
    
    EXPECT_EQ(todo.id, 55);
    EXPECT_EQ(todo.title, "JSON Task");
    EXPECT_EQ(todo.description, "JSON Description");
    EXPECT_TRUE(todo.completed);
    EXPECT_EQ(todo.priority, 3);
    EXPECT_EQ(todo.due_date, "2026-01-25");
    EXPECT_EQ(todo.category_id, 2);
    EXPECT_EQ(todo.user_id, 101);
}