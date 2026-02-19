#include <gtest/gtest.h>
#include "../src/Database.h"
#include <sys/stat.h>  
#include <unistd.h> 
#include <clocale>  


class SearchTest : public ::testing::Test {
protected:
    void SetUp() override {
        setlocale(LC_ALL, "");
        unlink("search_test.db");
        
        db = Database::getInstance();
        ASSERT_TRUE(db->open("search_test.db"));
        
        test_user = db->registerUser("searchuser", "testpass123");
        ASSERT_NE(test_user.id, 0);
        
        // Создаем тестовые задачи
        db->createTodo("Купить молоко", "Купить молоко в магазине", 1, "", 0, test_user.id);
        db->createTodo("Купить хлеб", "Купить свежий хлеб", 1, "", 0, test_user.id);
        db->createTodo("Работа над проектом", "Закончить документацию", 2, "", 0, test_user.id);
        db->createTodo("Сходить в спортзал", "Тренировка в 18:00", 2, "", 0, test_user.id);
        db->createTodo("Позвонить маме", "Позвонить в выходные", 3, "", 0, test_user.id);
    }
    
    void TearDown() override {
        db->close();
        //Database::cleanup();
        unlink("search_test.db");
    }
    
    Database* db;
    User test_user;
};

// Тест 1: Простой поиск
TEST_F(SearchTest, SimpleSearch) {
    auto results = db->searchTodos(test_user.id, "купить", false);
    EXPECT_GE(results.size(), 2);
    
    bool found_milk = false;
    bool found_bread = false;
    
    for (const auto& todo : results) {
        if (todo.title.find("Купить молоко") != std::string::npos) found_milk = true;
        if (todo.title.find("Купить хлеб") != std::string::npos) found_bread = true;
    }
    
    EXPECT_TRUE(found_milk);
    EXPECT_TRUE(found_bread);
}

// Тест 2: Поиск по описанию
TEST_F(SearchTest, SearchInDescription) {
    auto results = db->searchTodos(test_user.id, "магазин", false);
    EXPECT_GE(results.size(), 1);
    
    if (!results.empty()) {
        EXPECT_TRUE(results[0].title.find("Купить молоко") != std::string::npos);
    }
}

// Тест 3: Поиск с оператором AND
TEST_F(SearchTest, SearchWithAnd) {
    auto results = db->searchTodos(test_user.id, "купить AND молоко", false);
    EXPECT_GE(results.size(), 1);
    
    if (!results.empty()) {
        EXPECT_TRUE(results[0].title.find("Купить молоко") != std::string::npos);
    }
}

// Тест 4: Поиск с оператором OR
TEST_F(SearchTest, SearchWithOr) {
    auto results = db->searchTodos(test_user.id, "спортзал OR тренировка", false);
    EXPECT_GE(results.size(), 1);
}

// Тест 5: Поиск фразы
TEST_F(SearchTest, SearchPhrase) {
    auto results = db->searchTodos(test_user.id, "Купить хлеб", false);
    EXPECT_GE(results.size(), 1);
    
    if (!results.empty()) {
        EXPECT_TRUE(results[0].title.find("Купить хлеб") != std::string::npos);
    }
}

// Тест 6: Префиксный поиск
TEST_F(SearchTest, PrefixSearch) {
    auto results = db->searchTodos(test_user.id, "купи*", false);
    
    std::cout << "Префиксный поиск 'купи*' нашел " << results.size() << " результатов" << std::endl;
     
    EXPECT_GE(results.size(), 2);
}


// Тест 7: Для исправленной функции fallback поиска
TEST_F(SearchTest, FixedFallbackTest) {
    auto todos = db->getUserTodos(test_user.id);
    for (const auto& todo : todos) {
        db->deleteTodo(todo.id);
    }
    
    db->createTodo("Тестовая задача A", "Описание A", 1, "", 0, test_user.id);
    db->createTodo("Тестовая задача B", "Описание B", 2, "", 0, test_user.id);
    db->createTodo("Другая задача", "Другое описание", 3, "", 0, test_user.id);
    
    auto results = db->searchTodosFallback(test_user.id, "Тестовая");
    
    EXPECT_GE(results.size(), 2);
}

// Тест 8: Набор для проверки подсветки с английским языком
TEST_F(SearchTest, EnglishCaseInsensitiveTest) {

    auto todos = db->getUserTodos(test_user.id);
    for (const auto& todo : todos) {
        db->deleteTodo(todo.id);
    }
    
    db->createTodo("London is a capital of Great Britain", 
                   "London Bridge is falling down", 
                   2, "", 0, test_user.id);
    
    db->createTodo("My Favorite Book is War and Peace", 
                   "I love Reading books", 
                   1, "", 0, test_user.id);
    
    db->populateFtsTable();
    
    // Тест 1: Ищем "london" с маленькой, в тексте "London" с большой
    auto results = db->searchTodos(test_user.id, "london", true);
    
    ASSERT_GE(results.size(), 1);

    EXPECT_TRUE(results[0].title_snippet.find("<mark>London</mark>") != string::npos);
    
    // Тест 2: Ищем "reading" с маленькой, в тексте "Reading" с большой
    cout << "\n=== Test 4: Search 'reading' (lowercase) in text with 'Reading' (uppercase) ===" << endl;
    results = db->searchTodos(test_user.id, "reading", true);
    
    ASSERT_GE(results.size(), 1);
    cout << "Found " << results.size() << " results" << endl;
    
    for (const auto& todo : results) {
        if (todo.description.find("Reading") != string::npos) {
            cout << "Description: " << todo.description << endl;
            cout << "Desc snippet: " << todo.desc_snippet << endl;
            EXPECT_TRUE(todo.desc_snippet.find("<mark>Reading</mark>") != string::npos);
            break;
        }
    }
}

// Тест 9: Для проверки подсветки на русском языке
TEST_F(SearchTest, RussianExactCaseTest) {
    auto todos = db->getUserTodos(test_user.id);
    for (const auto& todo : todos) {
        db->deleteTodo(todo.id);
    }
    
    db->createTodo("Москва - столица России", 
                   "Московский Кремль находится в Москве", 
                   2, "", 0, test_user.id);
    db->createTodo("Любимая книга Война и мир", 
                   "Я люблю читать русскую классику", 
                   1, "", 0, test_user.id);
    
    db->createTodo("Купить молоко и хлеб", 
                   "Нужно купить свежее молоко", 
                   3, "", 0, test_user.id);
    
    db->populateFtsTable();
    
    // Тест 1: Поиск "Москва" (с большой)
    cout << "\n=== Тест 1: Поиск 'Москва' ===" << endl;
    auto results = db->searchTodos(test_user.id, "Москва", true);
    
    ASSERT_GE(results.size(), 1);
    cout << "Заголовок: " << results[0].title << endl;
    cout << "Сниппет: " << results[0].title_snippet << endl;
    EXPECT_TRUE(results[0].title_snippet.find("<mark>Москва</mark>") != string::npos);
    
    // Тест 2: Поиск "купить"
    results = db->searchTodos(test_user.id, "купить", true);
    
    ASSERT_GE(results.size(), 1);
    bool found = false;
    for (const auto& todo : results) {
        if (todo.title.find("Купить") != string::npos) {
            cout << "Заголовок: " << todo.title << endl;
            cout << "Сниппет: " << todo.title_snippet << endl;
            EXPECT_TRUE(todo.title_snippet.find("<mark>Купить</mark>") != string::npos);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}
