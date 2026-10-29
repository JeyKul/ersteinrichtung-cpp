#include "../actions.hpp"

#include "../../core/console.hpp"
#include "../../core/system.hpp"

#include <filesystem>

namespace app {

bool Actions::installWindowsUpdates() {
    const auto updateClient = core::windowsSystemExecutable(L"UsoClient.exe");
    if (!std::filesystem::exists(updateClient)) {
        core::console::line(
            L"Windows Update client was not found.",
            core::console::error);
        return false;
    }

    bool ok = runLogged(
        L"Scan for Windows updates",
        updateClient,
        {L"StartScan"});
    ok = runLogged(
        L"Download Windows updates",
        updateClient,
        {L"StartDownload"}) && ok;
    ok = runLogged(
        L"Install Windows updates",
        updateClient,
        {L"StartInstall"}) && ok;

    core::console::line(
        L"Windows Update continues in the background. "
        L"Check Settings for status or restart requests.",
        core::console::warning);
    return ok;
}

} // namespace app
