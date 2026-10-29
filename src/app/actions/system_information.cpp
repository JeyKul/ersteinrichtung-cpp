#include "../actions.hpp"

#include "../../core/console.hpp"
#include "../../core/system.hpp"

namespace app {

void Actions::showSystemInformation() {
    const auto status = core::querySystemStatus();
    core::console::clear();
    core::console::separator(L"SYSTEM INFORMATION");
    core::console::line(L"Computer: " + status.computerName);
    core::console::line(L"Manufacturer: " + status.manufacturer);
    core::console::line(L"Model: " + status.model);
    core::console::line(L"CPU: " + status.cpu);
    core::console::line(
        L"RAM: " + std::to_wstring(status.ramGiB) + L" GiB");
    core::console::line(
        L"Windows: " + status.windowsVersion +
        L" (Build " + status.windowsBuild + L")");
    core::console::line(L"Secure Boot: " + status.secureBoot);
    core::console::line(L"BitLocker: " + status.bitLocker);
    core::console::line(L"Chocolatey: " + status.chocolatey);
    core::console::pause();
}

} // namespace app
