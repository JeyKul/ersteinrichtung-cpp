#include "../actions.hpp"

#include "../../core/console.hpp"
#include "../../core/download.hpp"
#include "../../core/system.hpp"

#include <windows.h>

#include <filesystem>

namespace app {

bool Actions::runChrisTitus() {
    core::console::line(
        L"Downloads and executes the current Chris Titus Tech WinUtil script from https://christitus.com/win.",
        core::console::warning);
    if (!core::console::confirm(
            L"Run this third-party script with administrator privileges?")) {
        return false;
    }

    wchar_t directory[MAX_PATH]{};
    GetTempPathW(MAX_PATH, directory);
    const auto script = std::filesystem::path(directory) / L"winutil.ps1";
    const auto downloaded = core::downloadFile(
        L"https://christitus.com/win",
        script);
    if (!downloaded.ok) {
        core::console::line(
            L"Download failed: " + downloaded.error,
            core::console::error);
        return false;
    }

    return runLogged(
        L"Chris Titus Tech WinUtil",
        core::windowsSystemExecutable(L"WindowsPowerShell\\v1.0\\powershell.exe"),
        {L"-NoProfile", L"-ExecutionPolicy", L"Bypass", L"-File", script.wstring()},
        true);
}

} // namespace app
