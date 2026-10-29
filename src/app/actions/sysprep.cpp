#include "../actions.hpp"

#include "../../core/console.hpp"
#include "../../core/system.hpp"

#include <filesystem>

namespace app {

bool Actions::runSysprep() {
    const auto answerFile =
        std::filesystem::path(core::executableDirectory()) /
        L"assets\\unattend.xml";
    if (!std::filesystem::exists(answerFile)) {
        core::console::line(
            L"Missing assets\\unattend.xml.",
            core::console::error);
        return false;
    }

    core::console::line(
        L"Sysprep closes this session and reboots into OOBE.",
        core::console::warning);
    if (!core::console::confirm(
            L"Run Sysprep OOBE now? The computer will reboot and this cannot be interrupted safely.")) {
        return false;
    }

    return runLogged(
        L"Sysprep OOBE",
        core::windowsSystemExecutable(L"Sysprep\\sysprep.exe"),
        {L"/oobe", L"/reboot", L"/unattend:" + answerFile.wstring()},
        true);
}

} // namespace app
