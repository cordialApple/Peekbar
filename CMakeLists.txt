cmake_minimum_required(VERSION 3.20)
project(browser_shell_os LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(browser_shell_os WIN32 src/main.cpp)
target_compile_definitions(browser_shell_os PRIVATE UNICODE _UNICODE)
target_link_libraries(browser_shell_os PRIVATE shell32 user32 gdi32)
