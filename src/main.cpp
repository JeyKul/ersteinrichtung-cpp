#include <windows.h>
#include <conio.h>
#include <lmcons.h>

#include <array>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace {

constexpr WORD kNormal = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD kAccent = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD kMuted = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD kWarning = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD kError = FOREGROUND_RED | FOREGROUND_INTENSITY;

void setColor(WORD color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void resetColor() {
    setColor(kNormal);
}

void clearScreen() {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(output, &info)) {
        std::system("cls");
        return;
    }

    const DWORD cells = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
    DWORD written = 0;
    const COORD home{0, 0};
    FillConsoleOutputCharacterW(output, L' ', cells, home, &written);
    FillConsoleOutputAttribute(output, info.wAttributes, cells, home, &written);
    SetConsoleCursorPosition(output, home);
}

bool isElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD bytes = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &bytes) != 0;
    CloseHandle(token);
    return ok && elevation.TokenIsElevated != 0;
}

std::wstring getComputerName() {
    std::array<wchar_t, MAX_COMPUTERNAME_LENGTH + 1> buffer{};
    DWORD length = static_cast<DWORD>(buffer.size());
    return GetComputerNameW(buffer.data(), &length) ? std::wstring(buffer.data(), length) : L"Unknown";
}

std::wstring getUserName() {
    std::array<wchar_t, UNLEN + 1> buffer{};
    DWORD length = static_cast<DWORD>(buffer.size());
    return GetUserNameW(buffer.data(), &length) ? std::wstring(buffer.data(), length - 1) : L"Unknown";
}

std::wstring getWindowsVersion() {
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto rtlGetVersion = ntdll == nullptr
        ? nullptr
        : reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));

    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (rtlGetVersion != nullptr && rtlGetVersion(&version) == 0) {
        return L"Windows " + std::to_wstring(version.dwMajorVersion) + L"."
            + std::to_wstring(version.dwMinorVersion) + L" (build "
            + std::to_wstring(version.dwBuildNumber) + L")";
    }

    return L"Windows (version unavailable)";
}

void printRule(wchar_t character = L'=') {
    setColor(kMuted);
    std::wcout << std::wstring(72, character) << L'\n';
    resetColor();
}

void showHeader() {
    setColor(kAccent);
    std::wcout << L"\n  ERSTEINRICHTUNG  |  Native C++ prototype\n";
    resetColor();
    printRule();

    std::wcout << L"  Device: " << getComputerName() << L"\n";
    std::wcout << L"  User:   " << getUserName() << L"\n";
    std::wcout << L"  OS:     " << getWindowsVersion() << L"\n";
    std::wcout << L"  Admin:  ";
    setColor(isElevated() ? kAccent : kError);
    std::wcout << (isElevated() ? L"Yes" : L"No");
    resetColor();
    std::wcout << L"\n";
    printRule(L'-');
}

void waitForEnter() {
    setColor(kMuted);
    std::wcout << L"\nPress Enter to return to the main menu...";
    resetColor();
    std::wstring ignored;
    std::getline(std::wcin, ignored);
}

void showPlaceholder(const std::wstring& title, const std::wstring& detail) {
    clearScreen();
    showHeader();
    setColor(kWarning);
    std::wcout << L"  " << title << L"\n\n";
    resetColor();
    std::wcout << L"  " << detail << L"\n\n";
    std::wcout << L"  This option is deliberately non-destructive in the first prototype.\n";
    std::wcout << L"  Its PowerShell behavior will be ported in a later step.\n";
    waitForEnter();
}

void showMenu() {
    clearScreen();
    showHeader();

    const std::array<std::pair<wchar_t, std::wstring>, 6> entries{{
        {L'1', L"Run initial setup"},
        {L'2', L"Windows Update"},
        {L'3', L"Tools"},
        {L'4', L"Extras"},
        {L'5', L"BitLocker"},
        {L'0', L"Exit"},
    }};

    setColor(kAccent);
    std::wcout << L"  Main menu\n\n";
    resetColor();

    for (const auto& [key, label] : entries) {
        setColor(kAccent);
        std::wcout << L"  [" << key << L"] ";
        resetColor();
        std::wcout << label << L"\n";
    }

    printRule(L'-');
    std::wcout << L"  Select an option: ";
}

} // namespace

int wmain() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleTitleW(L"Ersteinrichtung");

    if (!isElevated()) {
        clearScreen();
        setColor(kError);
        std::wcout << L"This executable must be launched with administrator privileges.\n";
        resetColor();
        std::wcout << L"The embedded manifest should request UAC elevation.\n";
        waitForEnter();
        return 1;
    }

    for (;;) {
        showMenu();
        const wint_t input = std::towupper(std::wcin.get());
        std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');

        switch (input) {
            case L'1':
                showPlaceholder(L"Initial setup", L"The complete setup workflow will be implemented here.");
                break;
            case L'2':
                showPlaceholder(L"Windows Update", L"The update workflow will be implemented here.");
                break;
            case L'3':
                showPlaceholder(L"Tools", L"The tools submenu will be implemented here.");
                break;
            case L'4':
                showPlaceholder(L"Extras", L"The extras submenu will be implemented here.");
                break;
            case L'5':
                showPlaceholder(L"BitLocker", L"The BitLocker workflow will be implemented here.");
                break;
            case L'0':
            case L'Q':
            case 27:
                clearScreen();
                setColor(kAccent);
                std::wcout << L"Ersteinrichtung closed.\n";
                resetColor();
                return 0;
            default:
                setColor(kError);
                std::wcout << L"\nInvalid selection.";
                resetColor();
                Sleep(900);
                break;
        }
    }
}
