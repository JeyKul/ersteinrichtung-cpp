#include "bitlocker_menu.hpp"

#include "../core/process_runner.hpp"
#include "tpm.hpp"

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace bitlocker {
namespace {

std::wstring selectedDrive = L"C:";

wchar_t toUpper(wchar_t character) {
    if (character >= L'a' && character <= L'z') {
        return static_cast<wchar_t>(character - L'a' + L'A');
    }
    return character;
}

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

void printFixedDrives() {
    std::wcout
        << L"\n  Drive  Label             Free    Total\n"
        << L"  -----  ----------------  ------  -------\n";

    const DWORD driveMask = GetLogicalDrives();

    for (wchar_t letter = L'A'; letter <= L'Z'; ++letter) {
        const DWORD bit = 1UL << (letter - L'A');

        if ((driveMask & bit) == 0) {
            continue;
        }

        const std::wstring root{letter, L':', L'\\'};

        if (GetDriveTypeW(root.c_str()) != DRIVE_FIXED) {
            continue;
        }

        wchar_t label[MAX_PATH]{};
        GetVolumeInformationW(
            root.c_str(), label, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0);

        ULARGE_INTEGER freeBytes{}, totalBytes{};
        GetDiskFreeSpaceExW(root.c_str(), &freeBytes, &totalBytes, nullptr);

        const auto gigabytes = [](const ULARGE_INTEGER& bytes) {
            const double value =
                static_cast<double>(bytes.QuadPart) / (1024.0 * 1024.0 * 1024.0);
            wchar_t text[32]{};
            swprintf(text, 32, L"%6.0f GB", value);
            return std::wstring(text);
        };

        std::wstring driveLabel(label);
        if (driveLabel.size() > 16) {
            driveLabel = driveLabel.substr(0, 16);
        }

        std::wcout << L"  " << letter << L":     " << std::left << std::setw(16)
                  << driveLabel << std::right << std::setw(8)
                  << gigabytes(freeBytes) << ' '
                  << std::setw(8) << gigabytes(totalBytes) << L"\n";
    }
}

void selectDrive() {
    printFixedDrives();

    const std::wstring input = prompt(L"Select a fixed drive letter: " );

    if (input.empty()) {
        return;
    }

    const wchar_t letter = toUpper(input.front());

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

void encryptSelectedDrive() {
    if (isTpmAvailable()) {
        std::wcout << L"\nTPM detected: enabling BitLocker with TPM protection...\n";
        runManageBde({L"-on", selectedDrive});

        std::wcout
            << L"\nAdding a recovery password protector (recommended)...\n";
        const core::ProcessResult result = runManageBde(
            {L"-protectors", L"-add", selectedDrive, L"-recoverypassword"});

        if (result.exitCode == 0) {
            std::wcout
                << L"\nIMPORTANT: Write down the recovery password shown above"
                   L" or export it (option 6)!\n";
        }
    } else {
        std::wcout
            << L"\nNo TPM detected: using a recovery password only...\n";
        const core::ProcessResult result = runManageBde(
            {L"-on", selectedDrive, L"-usedspaceonly", L"-recoverypassword"});

        if (result.exitCode == 0) {
            std::wcout
                << L"\nIMPORTANT: Write down the recovery password shown above"
                   L" or export it (option 6)!\n";
        }
    }

    waitForEnter();
}

void encryptOrDecrypt() {
    runManageBde({L"-status", selectedDrive});

    const std::wstring choice = prompt(
        L"\n[e] Encrypt  [d] Decrypt  [Enter] Cancel: " );

    if (choice == L"e") {
        if (confirmAction(L"ENCRYPT")) {
            encryptSelectedDrive();
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

    if (toUpper(windowsDirectory[0]) == toUpper(selectedDrive[0])) {
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

void ensureDirectory(const std::wstring& directory) {
    const DWORD attributes = GetFileAttributesW(directory.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return;
    }

    std::filesystem::path parent(directory);
    const std::wstring leaf = parent.filename().native();

    if (!leaf.empty()) {
        parent.remove_filename();
        ensureDirectory(parent.native());
    }

    std::error_code error;
    std::filesystem::create_directory(directory, error);
}

std::wstring defaultRecoveryFilePath() {
    wchar_t exePath[MAX_PATH]{};
    const DWORD exeLength = GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring executableDirectory;
    if (exeLength > 0 && exeLength < MAX_PATH) {
        std::wstring exe(exePath);
        executableDirectory = exe.substr(0, exe.find_last_of(L"\\/") + 1);
    }

    const std::wstring keyDirectory =
        executableDirectory + L"Logs\\BitLockerKeys\\";
    ensureDirectory(keyDirectory);

    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1]{};
    DWORD nameLength = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameExW(ComputerNameNetBIOS, computerName, &nameLength);

    SYSTEMTIME now{};
    GetLocalTime(&now);

    wchar_t date[32]{};
    swprintf(date, 32, L"%02d.%02d.%04d", now.wDay, now.wMonth, now.wYear);

    return keyDirectory + selectedDrive.substr(0, 1) + L"-BITLOCKER_" +
           computerName + L"_" + date + L".txt";
}

void exportRecoveryProtectorsTo(const std::wstring& destination) {
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
}

void exportRecoveryProtectors() {
    const std::wstring destination = defaultRecoveryFilePath();

    std::wcout
        << L"\nRecovery passwords are sensitive. Export only to a protected location.\n"
        << L"Default export location: " << destination << L"\n";

    std::wstring path = prompt(L"Path (Enter to accept default): " );

    if (!path.empty() && !std::filesystem::path(path).is_absolute()) {
        std::wcout << L"An absolute file path is required.\n";
        waitForEnter();
        return;
    }

    if (path.empty()) {
        path = destination;
    }

    if (!confirmAction(L"EXPORT-RECOVERY")) {
        return;
    }

    exportRecoveryProtectorsTo(path);
    waitForEnter();
}

void addRecoveryPassword() {
    if (confirmAction(L"ADD-RECOVERY")) {
        const core::ProcessResult result = runManageBde(
            {L"-protectors", L"-add", selectedDrive, L"-recoverypassword"});

        if (result.exitCode == 0) {
            std::wcout
                << L"\nIMPORTANT: Write down the recovery password shown above"
                   L" or export it (option 6)!\n";
        }

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
