#include "../actions.hpp"

#include "../../core/console.hpp"
#include "../../core/system.hpp"

namespace app {

bool Actions::showSecurityStatus() {
    const auto status = core::querySystemStatus();
    core::console::line(L"Secure Boot: " + status.secureBoot);
    core::console::line(L"BitLocker: " + status.bitLocker);
    core::console::line(L"Chocolatey: " + status.chocolatey);
    return true;
}

} // namespace app
