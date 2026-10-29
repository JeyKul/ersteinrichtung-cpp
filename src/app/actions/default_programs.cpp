#include "../actions.hpp"

#include "../../core/console.hpp"

#include <windows.h>

#include <filesystem>

namespace app {

bool Actions::installDefaultPrograms() {
    wchar_t programData[MAX_PATH]{};
    GetEnvironmentVariableW(L"ProgramData", programData, MAX_PATH);
    const auto chocolatey = std::filesystem::path(programData) /
                            L"chocolatey\\bin\\choco.exe";

    if (!std::filesystem::exists(chocolatey) && !ensureChocolatey()) {
        return false;
    }
    if (!std::filesystem::exists(chocolatey)) {
        core::console::line(
            L"Chocolatey still not found. Restart this application and retry.",
            core::console::error);
        return false;
    }

    return runLogged(
        L"Install default programs",
        chocolatey.wstring(),
        {L"install", L"vlc", L"7zip-zstd", L"firefox", L"-y"});
}

} // namespace app
