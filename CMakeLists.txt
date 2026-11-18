cmake_minimum_required(VERSION 3.15)
project(CS2Injector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# WIN32 убирает консольное окно
add_executable(Injector WIN32 src/main.cpp)
