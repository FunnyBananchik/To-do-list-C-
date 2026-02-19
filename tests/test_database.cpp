#include <gtest/gtest.h>
#include "../src/Database.h"
#include <sys/stat.h>  
#include <unistd.h>   

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        unlink("test.db");
        
        db = Database::getInstance();
        ASSERT_TRUE(db->open("test.db"));
        
        test_user = db->registerUser("testuser", "testpass123");
        ASSERT_NE(test_user.id, 0);
        auto todos = db->getUserTodos(test_user.id);
        for (const auto& todo : todos) {
            db->deleteTodo(todo.id);
        }
    }
    
    void TearDown() override {
        db->close();
        //Database::cleanup();
        unlink("test.db");
    }
    
    Database* db;
    User test_user;
};

// Тест 1: Создание и получение задач
TEST_F(DatabaseTest, CreateAndGetTodo) {
    Todo created = db->createTodo("Test Todo", "Description", 2, "2024-12-31", 0, test_user.id);
    EXPECT_NE(created.id, 0);
    
    Todo fetched = db->getTodoById(created.id, test_user.id);
    EXPECT_EQ(fetched.id, created.id);
    EXPECT_EQ(fetched.title, "Test Todo");
    EXPECT_EQ(fetched.description, "Description");
    EXPECT_EQ(fetched.priority, 2);
}
// Тест 2: Получение всех задач пользователя
TEST_F(DatabaseTest, GetUserTodos) {
    db->createTodo("Todo 1", "", 1, "", 0, test_user.id);
    db->createTodo("Todo 2", "", 2, "", 0, test_user.id);
    db->createTodo("Todo 3", "", 3, "", 0, test_user.id);
    
    auto todos = db->getUserTodos(test_user.id);
    EXPECT_EQ(todos.size(), 3);
}

// Тест 3: Обновление задачи
TEST_F(DatabaseTest, UpdateTodo) {
    Todo todo = db->createTodo("Original", "Original desc", 1, "", 0, test_user.id);
    
    todo.title = "Updated";
    todo.description = "Updated desc";
    todo.completed = true;
    todo.priority = 3;
    
    bool updated = db->updateTodo(todo);
    EXPECT_TRUE(updated);
    
    Todo fetched = db->getTodoById(todo.id, test_user.id);
    EXPECT_EQ(fetched.title, "Updated");
    EXPECT_EQ(fetched.description, "Updated desc");
    EXPECT_TRUE(fetched.completed);
    EXPECT_EQ(fetched.priority, 3);
}

// Тест 4: Удаление задачи
TEST_F(DatabaseTest, DeleteTodo) {
    Todo todo = db->createTodo("To Delete", "", 1, "", 0, test_user.id);
    
    bool deleted = db->deleteTodo(todo.id);
    EXPECT_TRUE(deleted);
    
    Todo fetched = db->getTodoById(todo.id, test_user.id);
    EXPECT_EQ(fetched.id, -1);  
}

// Тест 5: Статистика
TEST_F(DatabaseTest, GetStats) {
    db->createTodo("Todo 1", "", 1, "", 0, test_user.id);
    db->createTodo("Todo 2", "", 2, "", 0, test_user.id);
    
    Todo todo = db->createTodo("Todo 3", "", 3, "", 0, test_user.id);
    todo.completed = true;
    db->updateTodo(todo);
    
    auto stats = db->getStats(test_user.id);
    EXPECT_EQ(stats.total, 3);
    EXPECT_EQ(stats.completed, 1);
    EXPECT_EQ(stats.pending, 2);
}

// Тест 6: Категории
TEST_F(DatabaseTest, Categories) {
    categories cat = db->createCat("Test Category", "#FF0000", test_user.id);
    EXPECT_NE(cat.id, 0);
    
    auto cats = db->getAllCat(test_user.id);
    EXPECT_EQ(cats.size(), 1);
    EXPECT_EQ(cats[0].name, "Test Category");
    
    bool deleted = db->deleteCat(cat.id);
    EXPECT_TRUE(deleted);
    
    cats = db->getAllCat(test_user.id);
    EXPECT_EQ(cats.size(), 0);
}

// Тест 7: Просроченные задачи
TEST_F(DatabaseTest, OverdueTodos) {
    db->createTodo("Overdue", "", 1, "2020-01-01", 0, test_user.id);
    
    db->createTodo("Future", "", 1, "2026-10-10", 0, test_user.id);
    
    auto overdue = db->getOverdueTrue(test_user.id);
    
    EXPECT_EQ(overdue.size(), 1);
}

// Тест 8: Проверка существования пользователя, регистрация и вход
TEST_F(DatabaseTest, UserAuth) {
    EXPECT_TRUE(db->userExists("testuser"));
    EXPECT_FALSE(db->userExists("nonexistent"));

    auto user1 = db->registerUser("nonexistent", "fdgfdh563hh");
    EXPECT_TRUE(db->userExists("nonexistent"));
    
    User login_user = db->loginUser("testuser", "testpass123");
    EXPECT_NE(login_user.id, 0);
    EXPECT_EQ(login_user.username, "testuser");
    
    login_user = db->loginUser("testuser", "wrongpass");
    EXPECT_EQ(login_user.id, 0);
}

// Тест 9: Задачи по категориям
TEST_F(DatabaseTest, TodosByCategories) {
    categories cat1 = db->createCat("Cat1", "#FF0000", test_user.id);
    categories cat2 = db->createCat("Cat2", "#00FF00", test_user.id);
    
    db->createTodo("Todo in Cat1", "", 1, "", cat1.id, test_user.id);
    db->createTodo("Todo in Cat2", "", 1, "", cat2.id, test_user.id);
    db->createTodo("Todo in Cat1 again", "", 1, "", cat1.id, test_user.id);
    
    std::vector<int> cat_ids = {cat1.id};
    auto todos = db->getTodosByCategories(cat_ids, test_user.id);
    EXPECT_EQ(todos.size(), 2);
    
    cat_ids = {cat1.id, cat2.id};
    todos = db->getTodosByCategories(cat_ids, test_user.id);
    EXPECT_EQ(todos.size(), 3);
}

