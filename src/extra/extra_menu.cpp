#include "extra_menu.hpp"

#include "../core/process_runner.hpp"
#include "../core/wmi.hpp"

#include <windows.h>
#include <shellapi.h>

#include <cwctype>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace extra {
namespace {

constexpr wchar_t kRegistryCurrentVersion[] =
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

std::wstring prompt(const std::wstring& label) {
    std::wcout << label;

    std::wstring value;
    std::getline(std::wcin, value);
    return value;
}

bool confirm(const std::wstring& question) {
    std::wcout << question << L" (y/N): ";

    std::wstring answer;
    std::getline(std::wcin, answer);
    return answer == L"y" || answer == L"Y";
}

void waitForEnter() {
    std::wcout << L"\nPress Enter to continue...";
    std::wstring ignored;
    std::getline(std::wcin, ignored);
}

std::wstring getSystemDirectoryExecutable(const std::wstring& name) {
    wchar_t systemDirectory[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(systemDirectory, MAX_PATH);

    if (length == 0 || length >= MAX_PATH) {
        return {};
    }

    return std::wstring(systemDirectory) + L"\\" + name;
}

void runAndReport(
    const std::wstring& executable,
    const std::vector<std::wstring>& arguments) {
    if (executable.empty()) {
        std::wcout << L"\nCould not locate the required executable.\n";
        return;
    }

    const core::ProcessResult result = core::runProcess(executable, arguments);

    if (!result.error.empty()) {
        std::wcout << L"\nError: " << result.error << L"\n";
        return;
    }

    if (!result.output.empty()) {
        std::wcout << L"\n" << result.output;
    }
    std::wcout << L"\nExit code: " << result.exitCode << L"\n";
}

// --- [LTSC] Install Windows Store -------------------------------------------

void installWindowsStore() {
    std::wcout
        << L"\nResetting the Windows Store app (wsreset -i)...\n"
           L"The reset can take several minutes.\n";

    runAndReport(getSystemDirectoryExecutable(L"wsreset.exe"), {L"-i"});
    waitForEnter();
}

// --- [LTSC] Install Winget ---------------------------------------------------

void installWinget() {
    std::wcout
        << L"\nOpening the Microsoft Store page for the App Installer\n"
           L"(provides Winget). Please accept the installation there.\n"
           L"Only use this after the Windows Store has been installed.\n";

    const HINSTANCE opened = ShellExecuteW(
        nullptr,
        L"open",
        L"ms-windows-store://pdp/?ProductId=9NBLGGH4NNS1",
        nullptr,
        nullptr,
        SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(opened) <= 32) {
        std::wcout
            << L"\nCould not open the Store. Install the Windows Store first "
               L"(option 1).\n";
    }

    waitForEnter();
}

// --- [LTSC to Pro] Enable upgrade path ---------------------------------------

std::optional<std::wstring> readRegistryString(
    const std::wstring& valueName) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(
            HKEY_LOCAL_MACHINE, kRegistryCurrentVersion, 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return std::nullopt;
    }

    wchar_t buffer[256]{};
    DWORD size = sizeof(buffer);
    DWORD type = 0;

    const LSTATUS status = RegQueryValueExW(
        key, valueName.c_str(), nullptr, &type,
        reinterpret_cast<LPBYTE>(buffer), &size);

    RegCloseKey(key);

    if (status != ERROR_SUCCESS || type != REG_SZ) {
        return std::nullopt;
    }
    return std::wstring(buffer);
}

void enableDowngradePath() {
    std::wcout
        << L"\n=== Windows Edition Downgrade Path ===\n"
           L"This edits the registry so Windows setup with Pro media can\n"
           L"upgrade an LTSC/Enterprise machine in place.\n";

    const std::optional<std::wstring> productName =
        readRegistryString(L"ProductName");
    const std::optional<std::wstring> editionId =
        readRegistryString(L"EditionID");

    std::wcout << L"\nCurrent values:\n"
               << L"  ProductName: "
               << productName.value_or(L"<unknown>") << L"\n"
               << L"  EditionID:   "
               << editionId.value_or(L"<unknown>") << L"\n";

    if (!confirm(L"\nSet EditionID=Professional, ProductName=Windows 10 Pro?")) {
        waitForEnter();
        return;
    }

    HKEY key = nullptr;
    const LSTATUS opened = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        kRegistryCurrentVersion,
        0,
        KEY_WRITE,
        &key);

    if (opened != ERROR_SUCCESS) {
        std::wcout << L"\nError: could not open the registry key.\n";
        waitForEnter();
        return;
    }

    constexpr wchar_t kNewEdition[] = L"Professional";
    constexpr wchar_t kNewProduct[] = L"Windows 10 Pro";

    const LSTATUS editionStatus = RegSetValueExW(
        key, L"EditionID", 0, REG_SZ,
        reinterpret_cast<const BYTE*>(kNewEdition),
        sizeof(kNewEdition));
    const LSTATUS productStatus = RegSetValueExW(
        key, L"ProductName", 0, REG_SZ,
        reinterpret_cast<const BYTE*>(kNewProduct),
        sizeof(kNewProduct));

    RegCloseKey(key);

    if (editionStatus == ERROR_SUCCESS && productStatus == ERROR_SUCCESS) {
        std::wcout
            << L"\nUpdated registry successfully!\n"
               L"  EditionID -> Professional\n"
               L"  ProductName -> Windows 10 Pro\n\n"
               L"Next steps:\n"
               L"1. Run Windows setup with Pro media (same or newer build).\n"
               L"2. Choose 'Keep files and apps'.\n"
               L"3. Enter a valid Windows Pro key when prompted.\n";
    } else {
        std::wcout << L"\nError while updating the registry.\n";
    }

    waitForEnter();
}

// --- Drivers -----------------------------------------------------------------

std::wstring cleanModelName(const std::wstring& model) {
    std::wstring cleaned;
    bool previousWasSpace = true;

    for (const wchar_t character : model) {
        if (character == L' ' || character == L'\t') {
            if (!previousWasSpace && !cleaned.empty()) {
                cleaned.push_back(L'_');
            }
            previousWasSpace = true;
        } else if (iswalnum(character) || character == L'_' ||
                   character == L'-') {
            cleaned.push_back(character);
            previousWasSpace = false;
        }
    }

    return cleaned;
}

std::wstring getDriverModel() {
    const std::optional<std::wstring> model =
        core::queryWmiStringProperty(L"Win32_ComputerSystem", L"Model");

    if (!model || model->empty()) {
        return L"Unknown";
    }

    return cleanModelName(*model);
}

std::wstring promptForDriverPath(const std::wstring& suffix) {
    const std::wstring input = prompt(
        L"Enter device letter (e.g. E): " );

    if (input.empty()) {
        return {};
    }

    const wchar_t letter = static_cast<wchar_t>(
        input.front() >= L'a' && input.front() <= L'z'
            ? input.front() - L'a' + L'A'
            : input.front());

    if (letter < L'A' || letter > L'Z') {
        return {};
    }

    return std::wstring(1, letter) + L":\\" + suffix;
}

void extractDrivers() {
    const std::wstring model = getDriverModel();
    std::wcout << L"\nModel: " << model << L"\n";

    std::wstring targetDirectory = promptForDriverPath(
        L"extracted_drivers\\" + model);

    if (targetDirectory.empty()) {
        std::wcout << L"Aborted.\n";
        waitForEnter();
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(targetDirectory, error);

    std::wcout << L"\nExporting drivers to " << targetDirectory
               << L" ..." << std::flush;

    runAndReport(
        getSystemDirectoryExecutable(L"dism.exe"),
        {L"/online",
         L"/export-driver",
         L"/destination:" + targetDirectory});
    waitForEnter();
}

void installDrivers() {
    const std::wstring driverPath = promptForDriverPath(
        L"extracted_drivers\\" + getDriverModel());

    if (driverPath.empty()) {
        std::wcout << L"Aborted.\n";
        waitForEnter();
        return;
    }

    const DWORD attributes = GetFileAttributesW(driverPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        std::wcout << L"\nDriver path '" << driverPath
                   << L"' does not exist. Aborting.\n";
        waitForEnter();
        return;
    }

    std::wcout << L"\nInstalling drivers from " << driverPath
               << L" ..." << std::flush;

    runAndReport(
        getSystemDirectoryExecutable(L"pnputil.exe"),
        {L"/add-driver", driverPath + L"\\*.inf", L"/subdirs", L"/install"});
    waitForEnter();
}

void printMenu() {
    std::wcout
        << L"\n=============================================\n"
        << L"  EXTRAS\n"
        << L"=============================================\n"
        << L"  1.  [LTSC] Install Windows Store\n"
        << L"  2.  [LTSC] Install Winget (after Store)\n"
        << L"  3.  [LTSC to Pro] Enable upgrade path\n"
        << L"  10. Extract drivers from this PC\n"
        << L"  11. Install drivers to this PC\n"
        << L"  0.  Back\n\n";
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
            installWindowsStore();
        } else if (choice == L"2") {
            installWinget();
        } else if (choice == L"3") {
            enableDowngradePath();
        } else if (choice == L"10") {
            extractDrivers();
        } else if (choice == L"11") {
            installDrivers();
        }
    }
}

}  // namespace extra
