#include <windows.h>
#include "app/actions.hpp"
#include "app/menus.hpp"
#include "core/console.hpp"
#include "core/logger.hpp"
#include "core/system.hpp"
#include <exception>

int wmain() {
    core::console::initialize();
    if (!core::isAdministrator()) { core::console::line(L"Administrator rights are required.", core::console::error); return 1; }
    try { core::Logger logger; app::Actions actions(logger); app::runMainMenu(actions, logger); }
    catch (const std::exception&) { core::console::line(L"Fatal error. See the application log.", core::console::error); return 2; }
    return 0;
}
