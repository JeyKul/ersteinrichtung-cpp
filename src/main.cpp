#include <windows.h>

#include <iostream>
#include <string>

#include "bitlocker/bitlocker_menu.hpp"
#include "extra/extra_menu.hpp"

namespace {

constexpr WORD kDefaultColor =
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD kAccentColor =
    FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;

void setConsoleColor(WORD color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void clearConsole() {
    const HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(console, &info)) {
        return;
    }

    const DWORD cells =
        static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
    const COORD origin{0, 0};
    DWORD written = 0;

    FillConsoleOutputCharacterW(console, L' ', cells, origin, &written);
    FillConsoleOutputAttribute(console, info.wAttributes, cells, origin, &written);
    SetConsoleCursorPosition(console, origin);
}

bool isRunningAsAdministrator() {
    HANDLE token = nullptr;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD returnedSize = 0;
    const BOOL succeeded = GetTokenInformation(
        token,
        TokenElevation,
        &elevation,
        sizeof(elevation),
        &returnedSize);

    CloseHandle(token);
    return succeeded && elevation.TokenIsElevated != 0;
}

void printHeader() {
    setConsoleColor(kAccentColor);
    std::wcout
        << L"\n  ERSTEINRICHTUNG\n"
        << L"  Windows setup helper\n"
        << L"========================================================================\n";

    setConsoleColor(kDefaultColor);
    std::wcout
        << L"  Administrator: "
        << (isRunningAsAdministrator() ? L"yes" : L"no")
        << L"\n\n";
}

void waitForEnter() {
    std::wcout << L"\nPress Enter to return...";
    std::wstring ignored;
    std::getline(std::wcin, ignored);
}

void showPlaceholder(const std::wstring& title) {
    clearConsole();
    printHeader();
    std::wcout << L"  " << title << L" is not implemented yet.\n";
    waitForEnter();
}

void printMainMenu() {
    std::wcout
        << L"  MAIN MENU\n\n"
        << L"  1. Run initial setup\n"
        << L"  2. Windows Update\n"
        << L"  3. Tools\n"
        << L"  4. Extras\n"
        << L"  5. BitLocker\n"
        << L"  0. Exit\n\n"
        << L"Select an option: ";
}

}  // namespace

int wmain() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"Ersteinrichtung");

    if (!isRunningAsAdministrator()) {
        std::wcerr << L"Please run this application as administrator.\n";
        return 1;
    }

    while (true) {
        clearConsole();
        printHeader();
        printMainMenu();

        std::wstring choice;
        if (!std::getline(std::wcin, choice)) {
            return 0;
        }

        if (choice == L"0" || choice == L"q" || choice == L"Q") {
            return 0;
        }

        if (choice == L"1") {
            showPlaceholder(L"Initial setup");
        } else if (choice == L"2") {
            showPlaceholder(L"Windows Update");
        } else if (choice == L"3") {
            showPlaceholder(L"Tools");
        } else if (choice == L"4") {
            extra::showMenu();
        } else if (choice == L"5") {
            bitlocker::showMenu();
        }
    }
}
