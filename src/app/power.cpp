#include "power.hpp"

#include "../core/console.hpp"
#include "../core/process_runner.hpp"
#include "../core/system.hpp"

#include <windows.h>

namespace app {

SessionSleepGuard::SessionSleepGuard(core::Logger& logger) {
    active_ = SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED) != 0;
    logger.write(active_
        ? L"Session sleep prevention active; power-plan settings are unchanged."
        : L"WARNING: SetThreadExecutionState failed; automatic sleep is not blocked. Error=" +
              std::to_wstring(GetLastError()));
}

SessionSleepGuard::~SessionSleepGuard() {
    if (active_) {
        SetThreadExecutionState(ES_CONTINUOUS);
    }
}

bool disableHibernationAndFastStartup(core::Logger& logger) {
    const auto result = core::runProcess(
        core::windowsSystemExecutable(L"powercfg.exe"),
        {L"/hibernate", L"off"});

    logger.write(L"Startup policy: powercfg /hibernate off; exit=" +
                 std::to_wstring(result.exitCode));
    if (!result.output.empty()) logger.write(result.output);
    if (!result.error.empty()) logger.write(result.error);

    const bool ok = result.started && result.exitCode == 0;
    core::console::line(
        ok
            ? L"Hibernation and Windows Fast Startup are disabled."
            : L"WARNING: Could not disable hibernation/Fast Startup; check the log.",
        ok ? core::console::good : core::console::warning);
    return ok;
}

} // namespace app
