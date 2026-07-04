cmake_minimum_required(VERSION 3.20)

# Buildable two ways with no shell coupling:
#   as a subdirectory of the top-level project (one repo, two targets), or
#   standalone:  cmake -B build-profiler profiler
if(NOT DEFINED PROJECT_NAME)
    project(shell_profiler LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Console executable. Shares ZERO source with the shell (hard rule 8); the only
# coupling is the ETW provider/event names in Contract.h, declared here itself.
add_executable(shell_profiler
    main.cpp
    EtwSession.cpp
    MetricsView.cpp
)
target_compile_options(shell_profiler PRIVATE /utf-8)
target_compile_definitions(shell_profiler PRIVATE
    UNICODE _UNICODE
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    NTDDI_VERSION=0x0A000004
)
target_link_libraries(shell_profiler PRIVATE advapi32 tdh bcrypt psapi)
