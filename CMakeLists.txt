cmake_minimum_required(VERSION 3.10)
project(todo_backend)

set(CMAKE_CXX_STANDARD 17)

# 1. Просто используем заголовки jwt-cpp
set(JWT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/jwt-cpp/include")

# Проверяем существование
if(NOT EXISTS "${JWT_INCLUDE_DIR}/jwt-cpp/jwt.h")
    message(FATAL_ERROR "❌ jwt-cpp headers not found at ${JWT_INCLUDE_DIR}")
endif()

message(STATUS "✅ Using jwt-cpp headers from: ${JWT_INCLUDE_DIR}")

# 2. Исполняемый файл - добавляем ВСЕ .cpp файлы
add_executable(todo_backend 
    src/main.cpp
    src/AuthService.cpp
)

# 3. Подключаем заголовки - ДОБАВЛЯЕМ picojson
target_include_directories(todo_backend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp-httplib
    ${JWT_INCLUDE_DIR}
    ${JWT_INCLUDE_DIR}/jwt-cpp  # ДОБАВИТЬ эту строку для picojson
)

# 4. Подключаем библиотеки
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)  # ДОБАВИТЬ для pthread

target_link_libraries(todo_backend
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads  # ИСПРАВИТЬ с pthread на Threads::Threads
    sqlite3
)

# 5. Флаги компиляции
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(todo_backend PRIVATE
        -Wall
        -Wextra
        -O2
    )
endif()

# 6. Копируем frontend.html (ДОБАВИТЬ)
configure_file(src/frontend.html ${CMAKE_CURRENT_BINARY_DIR}/frontend.html COPYONLY)

message(STATUS "🎉 Project configured with CMake ${CMAKE_VERSION}")