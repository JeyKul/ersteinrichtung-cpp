#include "menus.hpp"

#include "../bitlocker/bitlocker_menu.hpp"
#include "../core/console.hpp"
#include "../core/download.hpp"
#include "../core/i18n.hpp"
#include "../core/system.hpp"

#include <string>

namespace app {
namespace {

using core::console::line;

void header() {
    const auto status = core::querySystemStatus();
    line(core::i18n::tr("app.title"), core::console::title);
    line();

    auto field = [](const std::wstring& label, bool known, bool enabled) {
        line(
            label + L": " + core::i18n::tr(
                !known ? "common.unknown" :
                enabled ? "common.enabled" : "common.disabled"),
            !known ? core::console::warning :
            enabled ? core::console::good : core::console::normal);
    };

    field(core::i18n::tr("header.secure_boot"), status.secureBootKnown, status.secureBoot);
    field(core::i18n::tr("header.bitlocker"), status.bitLockerKnown, status.bitLocker);
    field(core::i18n::tr("header.chocolatey"), true, status.chocolatey);
    line(core::i18n::tr("header.pc_name") + L": " + status.computerName);
    line(L"CPU: " + status.cpu + L" | RAM: " +
         std::to_wstring(status.ramGiB) + L" GiB");
    line();
}

void menuBody() {
    core::console::separator(core::i18n::tr("menu.setup"));
    line(core::i18n::tr("menu.batch_private_rename"));
    line(core::i18n::tr("menu.batch_business_rename"));
    core::console::separator(core::i18n::tr("menu.no_rename"));
    line(core::i18n::tr("menu.batch_private"));
    line(core::i18n::tr("menu.batch_business"));
    core::console::separator();
    line(core::i18n::tr("menu.actions"));
    core::console::separator(core::i18n::tr("menu.more"));
    line(core::i18n::tr("menu.submenus"));
    core::console::separator();
    line(L"0. " + core::i18n::tr("common.exit"), core::console::highlight);
    line();
}

bool runBatch(Actions& actions, bool rename, bool business) {
    if (!core::console::confirm(core::i18n::tr(
            business ? "confirm.batch_business" : "confirm.batch_private"))) {
        return false;
    }

    bool ok = true;
    if (rename) ok = actions.renameComputer() && ok;
    ok = actions.ensureChocolatey() && ok;
    ok = (business
        ? actions.installBusinessRemote()
        : actions.installSupremo()) && ok;
    ok = actions.installDefaultPrograms() && ok;
    ok = actions.installWindowsUpdates() && ok;
    if (business) ok = actions.applyEnhancedPrivacy() && ok;

    line(
        core::i18n::tr(ok ? "batch.done" : "batch.failed"),
        ok ? core::console::good : core::console::warning);
    return ok;
}

} // namespace

void runMainMenu(Actions& actions, core::Logger& logger) {
    if (!core::hasInternet()) {
        line(core::i18n::tr("network.offline"), core::console::warning);
    }

    for (;;) {
        core::console::clear();
        header();
        menuBody();

        const auto choice = core::console::prompt(core::i18n::tr("common.select"));
        if (choice == L"0" || choice == L"q" || choice == L"Q") {
            logger.write(L"Session finished");
            return;
        }

        if (choice == L"1") runBatch(actions, true, false);
        else if (choice == L"2") runBatch(actions, true, true);
        else if (choice == L"3") runBatch(actions, false, false);
        else if (choice == L"4") runBatch(actions, false, true);
        else if (choice == L"5") actions.renameComputer();
        else if (choice == L"6") actions.ensureChocolatey();
        else if (choice == L"7") actions.installSupremo();
        else if (choice == L"8") actions.installBusinessRemote();
        else if (choice == L"9") actions.installDefaultPrograms();
        else if (choice == L"10") actions.installWindowsUpdates();
        else if (choice == L"11") actions.showTools();
        else if (choice == L"12") actions.showExtrasEnhanced();
        else if (choice == L"13") bitlocker::showMenu(logger);
        else if (choice == L"19") actions.runSysprep();
        else if (choice == L"20") actions.runWhyNotWin11();
        else if (choice == L"21") actions.showSecurityStatus();
        else if (choice == L"22") actions.applyEnhancedPrivacy();
        else if (choice == L"7000") actions.runDiagnostics();
        else line(core::i18n::tr("common.invalid"), core::console::warning);

        if (choice != L"11" && choice != L"12" && choice != L"13") {
            core::console::pause();
        }
    }
}

} // namespace app
