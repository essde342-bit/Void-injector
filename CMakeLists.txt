cmake_minimum_required(VERSION 3.15)
project(CS2Injector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Форсируем UTF-8, чтобы русский текст не превращался в кашу на MSVC
add_compile_options(/utf-8)

# WIN32 убирает консольное окно
add_executable(Injector WIN32 src/main.cpp)

# Линкуем wininet для HTTP-запросов
target_link_libraries(Injector PRIVATE wininet)
