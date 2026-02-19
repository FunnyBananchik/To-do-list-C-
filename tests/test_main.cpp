#define BUILD_TESTS
#include "Database.h"
#include <gtest/gtest.h>
#include <iostream>
#include <sys/stat.h> 
#include <errno.h>   

Database* Database::instance = nullptr;

class TestEnv : public ::testing::Environment {
public:
    void SetUp() override {
        if (mkdir("logs", 0755) == 0) {
            std::cout << "Directory 'logs' created" << std::endl;
        } else {
            if (errno == EEXIST) {
                std::cout << "Directory 'logs' already exists" << std::endl;
            } else {
                std::cerr << "Failed to create directory: " << strerror(errno) << std::endl;
            }
        }
        std::cout << "Test Environment Setup" << std::endl;
    }
    
    void TearDown() override {
        // Очищаем тестовые файлы
         if (unlink("test.db") == 0) {
            std::cout << "File 'test.db' removed successfully" << std::endl;
        } else {
            if (errno == ENOENT) {
                std::cout << "File 'test.db' not found (nothing to remove)" << std::endl;
            } else {
                std::cerr << "Failed to remove file 'test.db': " 
                          << strerror(errno) << std::endl;
            }
        }
        // Удаляем директорию logs
        if (rmdir("logs") == 0) {
            std::cout << "Directory 'logs' removed successfully" << std::endl;
        } else {
            if (errno == ENOENT) {
                std::cout << "Directory 'logs' not found (nothing to remove)" << std::endl;
            } else if (errno == ENOTEMPTY) {
                std::cout << "Directory 'logs' not removed (not empty)" << std::endl;
            } else {
                std::cerr << "Failed to remove directory 'logs': " 
                          << strerror(errno) << std::endl;
            }
        }
        std::cout << "Test Environment Cleanup" << std::endl;
    }
};

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new TestEnv);
    return RUN_ALL_TESTS();
}