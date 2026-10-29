cmake_minimum_required(VERSION 3.21)
project(ersteinrichtung_cpp VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(Ersteinrichtung
    src/main.cpp

    src/app/actions/action_support.cpp
    src/app/actions/chocolatey.cpp
    src/app/actions/chris_titus.cpp
    src/app/actions/default_programs.cpp
    src/app/actions/extras_menu.cpp
    src/app/actions/legacy_privacy.cpp
    src/app/actions/ltsc_upgrade.cpp
    src/app/actions/remote_support.cpp
    src/app/actions/rename_computer.cpp
    src/app/actions/security_status.cpp
    src/app/actions/sysprep.cpp
    src/app/actions/system_information.cpp
    src/app/actions/tools_menu.cpp
    src/app/actions/why_not_win11.cpp
    src/app/actions/windows_updates.cpp

    src/app/diagnostics.cpp
    src/app/driver_tools.cpp
    src/app/privacy.cpp
    src/app/power.cpp
    src/app/menus.cpp

    src/core/console.cpp
    src/core/download.cpp
    src/core/i18n.cpp
    src/core/logger.cpp
    src/core/process_runner.cpp
    src/core/system.cpp
    src/bitlocker/bitlocker_menu.cpp
    resources/app.rc
)

target_compile_definitions(Ersteinrichtung PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(Ersteinrichtung PRIVATE
    advapi32
    shell32
    winhttp
    wininet
    wintrust
    crypt32
    user32
)

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
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:Ersteinrichtung>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/lang" "$<TARGET_FILE_DIR:Ersteinrichtung>/lang"
)

if(EXISTS "${CMAKE_SOURCE_DIR}/assets/TeamViewer_Host_Setup.exe")
    add_custom_command(TARGET Ersteinrichtung POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/assets/TeamViewer_Host_Setup.exe"
            "$<TARGET_FILE_DIR:Ersteinrichtung>/assets/TeamViewer_Host_Setup.exe"
    )
endif()
