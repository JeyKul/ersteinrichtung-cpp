#include <windows.h>
#include "app/actions.hpp"
#include "app/menus.hpp"
#include "core/console.hpp"
#include "core/i18n.hpp"
#include "core/logger.hpp"
#include "core/system.hpp"
#include <exception>
int wmain(){core::console::initialize();core::i18n::initialize();if(!core::isAdministrator()){core::console::line(core::i18n::tr("app.admin_required"),core::console::error);return 1;}try{core::Logger l;app::Actions a(l);app::runMainMenu(a,l);}catch(const std::exception&){core::console::line(core::i18n::tr("app.fatal"),core::console::error);return 2;}return 0;}
