#include "bitlocker_menu.hpp"

#include "../core/process_runner.hpp"

#include <windows.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace bitlocker {
namespace {

std::wstring selectedDrive = L"C:";

std::wstring getManageBdePath() {
    wchar_t systemDirectory[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(systemDirectory, MAX_PATH);

    if (length == 0 || length >= MAX_PATH) {
        return {};
    }

    return std::wstring(systemDirectory) + L"\\manage-bde.exe";
}

void waitForEnter() {
    std::wcout << L"\nPress Enter to continue...";
    std::wstring ignored;
    std::getline(std::wcin, ignored);
}

std::wstring prompt(const std::wstring& label) {
    std::wcout << label;

    std::wstring value;
    std::getline(std::wcin, value);
    return value;
}

bool confirmAction(const std::wstring& action) {
    const std::wstring expected = action + L" " + selectedDrive;
    const std::wstring actual = prompt(
        L"\nType exactly [" + expected + L"] to confirm: " );

    return actual == expected;
}

core::ProcessResult runManageBde(
    const std::vector<std::wstring>& arguments,
    bool hideOutput = false) {
    const std::wstring executable = getManageBdePath();

    if (executable.empty()) {
        return {1, {}, L"Could not locate manage-bde.exe."};
    }

    const core::ProcessResult result = core::runProcess(executable, arguments);

    if (!result.error.empty()) {
        std::wcout << L"\nError: " << result.error << L"\n";
    }

    if (hideOutput) {
        std::wcout << L"\nCommand completed. Sensitive output is hidden.\n";
    } else {
        std::wcout << L"\n" << result.output;
    }

    std::wcout << L"\nExit code: " << result.exitCode << L"\n";
    return result;
}

void showOverview() {
    runManageBde({L"-status"});
    waitForEnter();
}

void selectDrive() {
    std::wcout << L"\nAvailable fixed drives:\n";

    const DWORD driveMask = GetLogicalDrives();

    for (wchar_t letter = L'A'; letter <= L'Z'; ++letter) {
        const DWORD bit = 1UL << (letter - L'A');

        if ((driveMask & bit) == 0) {
            continue;
        }

        const std::wstring root{letter, L':', L'\\'};

        if (GetDriveTypeW(root.c_str()) == DRIVE_FIXED) {
            std::wcout << L"  " << letter << L":\n";
        }
    }

    const std::wstring input = prompt(L"Select a fixed drive letter: " );

    if (input.empty()) {
        return;
    }

    const wchar_t letter = static_cast<wchar_t>(std::towupper(input.front()));

    if (letter < L'A' || letter > L'Z') {
        return;
    }

    const std::wstring root{letter, L':', L'\\'};

    if (GetDriveTypeW(root.c_str()) != DRIVE_FIXED) {
        std::wcout << L"That is not a fixed drive.\n";
        waitForEnter();
        return;
    }

    selectedDrive = std::wstring(1, letter) + L":";
}

void encryptOrDecrypt() {
    runManageBde({L"-status", selectedDrive});

    const std::wstring choice = prompt(
        L"\n[e] Encrypt  [d] Decrypt  [Enter] Cancel: " );

    if (choice == L"e") {
        if (confirmAction(L"ENCRYPT")) {
            runManageBde(
                {L"-on", selectedDrive, L"-recoverypassword", L"-usedspaceonly"},
                true);
            waitForEnter();
        }
    } else if (choice == L"d" && confirmAction(L"DECRYPT")) {
        runManageBde({L"-off", selectedDrive});
        waitForEnter();
    }
}

void pauseOrResume() {
    const std::wstring choice = prompt(
        L"[p] Pause encryption  [r] Resume encryption  [Enter] Cancel: " );

    if (choice == L"p" && confirmAction(L"PAUSE")) {
        runManageBde({L"-pause", selectedDrive});
        waitForEnter();
    } else if (choice == L"r" && confirmAction(L"RESUME")) {
        runManageBde({L"-resume", selectedDrive});
        waitForEnter();
    }
}

void configureAutoUnlock() {
    wchar_t windowsDirectory[MAX_PATH]{};
    GetWindowsDirectoryW(windowsDirectory, MAX_PATH);

    if (std::towupper(windowsDirectory[0]) == selectedDrive[0]) {
        std::wcout << L"\nAuto-unlock is available only for data drives.\n";
        waitForEnter();
        return;
    }

    const std::wstring choice = prompt(
        L"[e] Enable auto-unlock  [d] Disable  [Enter] Cancel: " );

    if (choice == L"e" && confirmAction(L"ENABLE-AUTOUNLOCK")) {
        runManageBde({L"-autounlock", L"-enable", selectedDrive});
        waitForEnter();
    } else if (choice == L"d" && confirmAction(L"DISABLE-AUTOUNLOCK")) {
        runManageBde({L"-autounlock", L"-disable", selectedDrive});
        waitForEnter();
    }
}

void exportRecoveryProtectors() {
    std::wcout << L"\nRecovery passwords are sensitive. Export only to a protected location.\n";

    if (!confirmAction(L"EXPORT-RECOVERY")) {
        return;
    }

    const std::filesystem::path destination = prompt(
        L"Absolute destination file path: " );

    if (!destination.is_absolute()) {
        std::wcout << L"An absolute file path is required.\n";
        waitForEnter();
        return;
    }

    const core::ProcessResult result = runManageBde(
        {L"-protectors", L"-get", selectedDrive},
        true);

    if (result.exitCode == 0) {
        std::ofstream output(destination, std::ios::binary | std::ios::trunc);

        if (output) {
            const int byteCount = WideCharToMultiByte(
                CP_UTF8, 0, result.output.data(),
                static_cast<int>(result.output.size()), nullptr, 0, nullptr, nullptr);
            std::string utf8(static_cast<std::size_t>(byteCount), '\0');
            WideCharToMultiByte(
                CP_UTF8, 0, result.output.data(),
                static_cast<int>(result.output.size()), utf8.data(), byteCount, nullptr, nullptr);
            output.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
            std::wcout << L"Recovery protectors exported.\n";
        } else {
            std::wcout << L"Could not open the destination file.\n";
        }
    }

    waitForEnter();
}

void addRecoveryPassword() {
    if (confirmAction(L"ADD-RECOVERY")) {
        runManageBde(
            {L"-protectors", L"-add", selectedDrive, L"-recoverypassword"},
            true);
        waitForEnter();
    }
}

void upgradeMetadata() {
    if (confirmAction(L"UPGRADE")) {
        runManageBde({L"-upgrade", selectedDrive});
        waitForEnter();
    }
}

void wipeFreeSpace() {
    if (confirmAction(L"WIPE-FREE-SPACE")) {
        runManageBde({L"-wipefreespace", selectedDrive});
        waitForEnter();
    }
}

void printMenu() {
    std::wcout
        << L"\n=============================================\n"
        << L"  BITLOCKER  |  Selected drive: " << selectedDrive << L"\n"
        << L"=============================================\n"
        << L"  1. Overview\n"
        << L"  2. Select fixed drive\n"
        << L"  3. Encrypt or decrypt\n"
        << L"  4. Pause or resume encryption\n"
        << L"  5. Configure data-drive auto-unlock\n"
        << L"  6. Export recovery protectors\n"
        << L"  7. Add recovery password\n"
        << L"  8. Upgrade BitLocker metadata\n"
        << L"  9. Wipe free space\n"
        << L"  0. Back\n\n";
}

}  // namespace

void showMenu() {
    while (true) {
        printMenu();
        const std::wstring choice = prompt(L"Select an option: " );

        if (choice == L"0") {
            return;
        }

        if (choice == L"1") {
            showOverview();
        } else if (choice == L"2") {
            selectDrive();
        } else if (choice == L"3") {
            encryptOrDecrypt();
        } else if (choice == L"4") {
            pauseOrResume();
        } else if (choice == L"5") {
            configureAutoUnlock();
        } else if (choice == L"6") {
            exportRecoveryProtectors();
        } else if (choice == L"7") {
            addRecoveryPassword();
        } else if (choice == L"8") {
            upgradeMetadata();
        } else if (choice == L"9") {
            wipeFreeSpace();
        }
    }
}

}  // namespace bitlocker
