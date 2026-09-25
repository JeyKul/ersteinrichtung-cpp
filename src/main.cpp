#include <windows.h>

#include <iostream>
#include <string>
#include <vector>

#include "bitlocker/bitlocker_menu.hpp"
#include "extra/extra_menu.hpp"
#include "tools/tools_menu.hpp"

namespace {

constexpr WORD kDefaultColor =
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD kAccentColor =
    FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD kWarningColor =
    FOREGROUND_RED | FOREGROUND_INTENSITY;

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

// Confirm a yes/no action, returning true only if the user typed 'y'.
bool confirm(const std::wstring& question) {
    std::wcout << question << L" (y/N): ";
    std::wstring answer;
    std::getline(std::wcin, answer);
    return answer == L"y" || answer == L"Y";
}

// --- Batch helpers (mimic PowerShell Handle-Step) ----------------------------

void stepRenamePC() {
    tools::renameComputer();
}

void stepChocolatey() {
    tools::installChocolatey();
}

void stepSupremo() {
    tools::installSupremoPK();
}

void stepTeamViewerGK() {
    tools::installTeamViewerGK();
}

void stepDefaultPrograms() {
    tools::installDefaultPrograms();
}

void stepWindowsUpdates() {
    tools::checkWindowsUpdates();
}

void stepDisableTelemetry() {
    tools::disableTelemetry();
}

void stepFastBoot() {
    tools::disableFastBoot();
}

void stepRevertFastBoot() {
    tools::revertFastBoot();
}

void runBatch(const std::wstring& label, const std::vector<std::wstring>& steps) {
    std::wcout << L"\n";
    for (const auto& step : steps) {
        if (step == L"1") stepRenamePC();
        else if (step == L"2") stepChocolatey();
        else if (step == L"3") stepSupremo();
        else if (step == L"4") stepTeamViewerGK();
        else if (step == L"5") stepDefaultPrograms();
        else if (step == L"6") stepWindowsUpdates();
        else if (step == L"7") stepDisableTelemetry();
        else if (step == L"8") stepFastBoot();
        else if (step == L"9") stepRevertFastBoot();
    }
}

void printMainMenu() {
    std::wcout
        << L"  MAIN MENU\n\n"
        << L"  --- Batches (automated setup) ---\n"
        << L"  1.  [BATCH] Privatkunde (Rename, Choco, Supremo, Default Programs, Updates)\n"
        << L"  2.  [BATCH] Geschaeftskunde (Rename, Choco, TV+Supremo, Default Programs, Updates, Telemetry)\n"
        << L"  3.  [BATCH] Privatkunde (no rename: Choco, Supremo, Default Programs, Updates)\n"
        << L"  4.  [BATCH] Geschaeftskunde (no rename: Choco, TV+Supremo, Default Programs, Updates, Telemetry)\n"
        << L"  --- Individual tools ---\n"
        << L"  5.  Tools (interactive menu)\n"
        << L"  6.  Extras\n"
        << L"  7.  BitLocker\n"
        << L"  0.  Exit\n\n"
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
            // Privatkunde batch (with rename): steps 5,6,7,9,10 in PS
            if (!confirm(L"Run full Privatkunde setup?")) { waitForEnter(); continue; }
            runBatch(L"Privatkunde", {L"1", L"2", L"3", L"5", L"6"});
        } else if (choice == L"2") {
            // Geschaeftskunde batch (with rename): steps 5,6,8,9,10,22 in PS
            if (!confirm(L"Run full Geschaeftskunde setup?")) { waitForEnter(); continue; }
            runBatch(L"Geschaeftskunde", {L"1", L"2", L"4", L"5", L"6", L"7"});
        } else if (choice == L"3") {
            // Privatkunde batch (no rename): steps 6,7,9,10 in PS
            if (!confirm(L"Run Privatkunde setup (no rename)?")) { waitForEnter(); continue; }
            runBatch(L"Privatkunde", {L"2", L"3", L"5", L"6"});
        } else if (choice == L"4") {
            // Geschaeftskunde batch (no rename): steps 6,8,9,10,22 in PS
            if (!confirm(L"Run Geschaeftskunde setup (no rename)?")) { waitForEnter(); continue; }
            runBatch(L"Geschaeftskunde", {L"2", L"4", L"5", L"6", L"7"});
        } else if (choice == L"5") {
            tools::showMenu();
        } else if (choice == L"6") {
            extra::showMenu();
        } else if (choice == L"7") {
            bitlocker::showMenu();
        } else {
            std::wcout << L"Invalid selection. Try again.\n";
            waitForEnter();
        }
    }
}
