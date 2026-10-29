#include "../actions.hpp"

#include "../../core/console.hpp"

namespace app {

void Actions::showExtras() {
    for (;;) {
        core::console::clear();
        core::console::separator(L"EXTRAS");
        core::console::line(L"1. Open Microsoft Store");
        core::console::line(L"2. Open App Installer");
        core::console::line(L"3. Prepare LTSC-to-Pro upgrade path");
        core::console::line();
        core::console::separator();
        core::console::line(L"0. Back", core::console::highlight);

        const auto choice = core::console::prompt(L"> ");
        if (choice == L"0") return;
        if (choice == L"1") {
            runLogged(
                L"Open Microsoft Store",
                L"explorer.exe",
                {L"ms-windows-store:"},
                true);
        } else if (choice == L"2") {
            runLogged(
                L"Open App Installer",
                L"explorer.exe",
                {L"ms-appinstaller:"},
                true);
        } else if (choice == L"3") {
            enableLtscUpgradePath();
        }
        core::console::pause();
    }
}

} // namespace app
