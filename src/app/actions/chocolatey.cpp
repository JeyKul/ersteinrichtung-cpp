#include "../actions.hpp"

#include "../../core/console.hpp"
#include "../../core/download.hpp"
#include "../../core/system.hpp"

#include <windows.h>

#include <filesystem>

namespace app {
namespace {

std::filesystem::path chocolateyExecutable() {
    wchar_t programData[MAX_PATH]{};
    GetEnvironmentVariableW(L"ProgramData", programData, MAX_PATH);
    return std::filesystem::path(programData) /
           L"chocolatey\\bin\\choco.exe";
}

std::filesystem::path temporaryScript() {
    wchar_t directory[MAX_PATH]{};
    GetTempPathW(MAX_PATH, directory);
    return std::filesystem::path(directory) / L"install-chocolatey.ps1";
}

} // namespace

bool Actions::ensureChocolatey() {
    const auto chocolatey = chocolateyExecutable();
    if (std::filesystem::exists(chocolatey)) {
        return runLogged(
            L"Upgrade Chocolatey",
            chocolatey.wstring(),
            {L"upgrade", L"chocolatey", L"-y"});
    }

    core::console::line(
        L"Chocolatey is missing. The official bootstrap script will be downloaded and run.",
        core::console::warning);
    if (!core::console::confirm(
            L"Install Chocolatey from community.chocolatey.org?")) {
        return false;
    }

    const auto script = temporaryScript();
    const auto downloaded = core::downloadFile(
        L"https://community.chocolatey.org/install.ps1",
        script);
    if (!downloaded.ok) {
        core::console::line(downloaded.error, core::console::error);
        return false;
    }

    return runLogged(
        L"Install Chocolatey",
        core::windowsSystemExecutable(L"WindowsPowerShell\\v1.0\\powershell.exe"),
        {L"-NoProfile", L"-ExecutionPolicy", L"Bypass", L"-File", script.wstring()});
}

} // namespace app
