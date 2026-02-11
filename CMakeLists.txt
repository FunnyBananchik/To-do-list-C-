cmake_minimum_required(VERSION 3.10)
project(todo_backend)

set(CMAKE_CXX_STANDARD 17)


set(JWT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/jwt-cpp/include")

# Проверяем существование
if(NOT EXISTS "${JWT_INCLUDE_DIR}/jwt-cpp/jwt.h")
    message(FATAL_ERROR "jwt-cpp headers not found at ${JWT_INCLUDE_DIR}")
endif()

message(STATUS "Using jwt-cpp headers from: ${JWT_INCLUDE_DIR}")


add_executable(todo_backend 
    src/main.cpp
    src/AuthService.cpp
)

target_include_directories(todo_backend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp-httplib
    ${JWT_INCLUDE_DIR}
    ${JWT_INCLUDE_DIR}/jwt-cpp
)

# Подключаем библиотеки
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(todo_backend
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    sqlite3
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(todo_backend PRIVATE
        -Wall
        -Wextra
        -O2
    )
    
    # Добавляем определение для FTS5
    target_compile_definitions(todo_backend PRIVATE SQLITE_ENABLE_FTS5)
elseif(MSVC)
    target_compile_options(todo_backend PRIVATE
        /W3
        /O2
    )
    target_compile_definitions(todo_backend PRIVATE SQLITE_ENABLE_FTS5)
endif()

configure_file(src/frontend.html ${CMAKE_CURRENT_BINARY_DIR}/frontend.html COPYONLY)

message(STATUS "Project configured with CMake ${CMAKE_VERSION}")
message(STATUS "FTS5 search support: ENABLED")