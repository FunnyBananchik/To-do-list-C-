cmake_minimum_required(VERSION 3.10)
project(TodoAppTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

find_path(GTEST_INCLUDE_DIR NAMES gtest/gtest.h
    PATHS /usr/include /usr/local/include /usr/src/gtest/include
)

find_library(GTEST_LIBRARY NAMES gtest
    PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
)

find_library(GTEST_MAIN_LIBRARY NAMES gtest_main
    PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
)

if(NOT GTEST_INCLUDE_DIR OR NOT GTEST_LIBRARY)
    message(FATAL_ERROR "Google Test not found")
endif()

include_directories(${GTEST_INCLUDE_DIR})

find_path(SQLITE3_INCLUDE_DIR NAMES sqlite3.h
    PATHS /usr/include /usr/local/include
)

find_library(SQLITE3_LIBRARY NAMES sqlite3
    PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
)

find_path(OPENSSL_INCLUDE_DIR NAMES openssl/ssl.h
    PATHS /usr/include /usr/local/include
)

find_library(OPENSSL_SSL_LIBRARY NAMES ssl
    PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
)

find_library(OPENSSL_CRYPTO_LIBRARY NAMES crypto
    PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
)

find_path(JWT_CPP_INCLUDE_DIR NAMES jwt-cpp/jwt.h
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../jwt-cpp/include
)

include_directories(
    ${SOURCE_DIR}
    ${JWT_CPP_INCLUDE_DIR}
    ${SQLITE3_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

set(APP_SOURCES
    ${SOURCE_DIR}/AuthService.cpp
)

add_executable(run_tests
    test_main.cpp
    test_helpers.cpp      
    test_auth.cpp
    test_todo.cpp
    test_categories.cpp
    test_user.cpp
    test_database.cpp  
    test_search.cpp     
    ${APP_SOURCES}
)

target_link_libraries(run_tests
    ${GTEST_LIBRARY}
    ${GTEST_MAIN_LIBRARY}
    ${SQLITE3_LIBRARY}
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    pthread
    dl
)

enable_testing()
add_test(NAME TodoAppTests COMMAND run_tests)