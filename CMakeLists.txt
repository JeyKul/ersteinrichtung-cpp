cmake_minimum_required(VERSION 3.21)

project(ersteinrichtung_cpp VERSION 0.2.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(Ersteinrichtung
    src/main.cpp
    src/core/process_runner.cpp
    src/bitlocker/bitlocker_menu.cpp
    resources/app.rc
)

target_compile_definitions(Ersteinrichtung PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(Ersteinrichtung PRIVATE advapi32)

if(MSVC)
    target_compile_options(Ersteinrichtung PRIVATE /W4 /permissive-)
else()
    target_compile_options(Ersteinrichtung PRIVATE -Wall -Wextra -Wpedantic)
endif()
