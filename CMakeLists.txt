cmake_minimum_required(VERSION 3.21)
project(ersteinrichtung_cpp VERSION 1.0.0 LANGUAGES CXX RC)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_executable(Ersteinrichtung
    src/main.cpp
    src/app/actions.cpp
    src/app/menus.cpp
    src/core/console.cpp
    src/core/download.cpp
    src/core/logger.cpp
    src/core/process_runner.cpp
    src/core/system.cpp
    src/bitlocker/bitlocker_menu.cpp
    resources/app.rc
)
target_compile_definitions(Ersteinrichtung PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(Ersteinrichtung PRIVATE advapi32 shell32 winhttp wintrust crypt32)
if(MINGW)
    target_link_options(Ersteinrichtung PRIVATE -municode)
endif()
if(MSVC)
    target_compile_options(Ersteinrichtung PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(Ersteinrichtung PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_custom_command(TARGET Ersteinrichtung POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Ersteinrichtung>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/unattend.xml" "$<TARGET_FILE_DIR:Ersteinrichtung>/assets/unattend.xml")
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/TeamViewer_Host_Setup.exe")
    add_custom_command(TARGET Ersteinrichtung POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/TeamViewer_Host_Setup.exe" "$<TARGET_FILE_DIR:Ersteinrichtung>/assets/TeamViewer_Host_Setup.exe")
endif()
