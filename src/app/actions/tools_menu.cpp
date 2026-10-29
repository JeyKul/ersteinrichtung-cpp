#include "../actions.hpp"

#include "../../bitlocker/bitlocker_menu.hpp"
#include "../../core/console.hpp"
#include "../../core/i18n.hpp"
#include "../power.hpp"

namespace app {
namespace {

std::wstring localized(const wchar_t* german, const wchar_t* english) {
    return core::i18n::language().rfind(L"de", 0) == 0 ? german : english;
}

} // namespace

void Actions::showTools() {
    for (;;) {
        core::console::clear();
        core::console::line();
        core::console::separator(core::i18n::tr("tools.title"));
        core::console::line();
        core::console::line(localized(
            L"1.  Systeminformationen",
            L"1.  System information"));
        core::console::line(L"2.  WinDirStat");
        core::console::line(L"3.  Advanced IP Scanner");
        core::console::line(L"4.  Lenovo Service Bridge");
        core::console::line(L"5.  Lenovo System Update");
        core::console::line(L"6.  PowerShell 7");
        core::console::line(L"7.  WhyNotWin11");
        core::console::line(localized(
            L"8.  Sicherheitsstatus",
            L"8.  Security status"));
        core::console::line(L"9.  BitLocker");
        core::console::line(L"10. Chris Titus Tech WinUtil");
        core::console::line(localized(
            L"11. Ruhezustand/Schnellstart erneut deaktivieren",
            L"11. Reapply hibernation/Fast Startup disable"));
        core::console::line();
        core::console::separator();
        core::console::line(
            L"0.  " + core::i18n::tr("common.back"),
            core::console::highlight);
        core::console::line();

        const auto choice =
            core::console::prompt(core::i18n::tr("common.select"));
        if (choice == L"0") return;

        bool pauseAfter = true;
        if (choice == L"1") {
            showSystemInformation();
            pauseAfter = false;
        } else if (choice == L"2") {
            downloadAndLaunch(
                L"WinDirStat",
                L"https://github.com/windirstat/windirstat/releases/latest/download/WinDirStat-x64.msi",
                L"WinDirStat-x64.msi");
        } else if (choice == L"3") {
            downloadAndLaunch(
                L"Advanced IP Scanner",
                L"https://download.advanced-ip-scanner.com/download/files/Advanced_IP_Scanner_2.5.4594.1.exe",
                L"Advanced_IP_Scanner.exe");
        } else if (choice == L"4") {
            downloadAndLaunch(
                L"Lenovo Service Bridge",
                L"https://download.lenovo.com/lsbv4/LSBSetup.exe",
                L"LSBSetup.exe");
        } else if (choice == L"5") {
            downloadAndLaunch(
                L"Lenovo System Update",
                L"https://download.lenovo.com/pccbbs/thinkvantage_en/system_update_5.08.03.59.exe",
                L"Lenovo-System-Update.exe");
        } else if (choice == L"6") {
            downloadAndLaunch(
                L"PowerShell 7",
                L"https://github.com/PowerShell/PowerShell/releases/download/v7.6.0/PowerShell-7.6.0-win-x64.msi",
                L"PowerShell-7.msi");
        } else if (choice == L"7") {
            runWhyNotWin11();
        } else if (choice == L"8") {
            showSecurityStatus();
        } else if (choice == L"9") {
            bitlocker::showMenu(logger_);
            pauseAfter = false;
        } else if (choice == L"10") {
            runChrisTitus();
        } else if (choice == L"11") {
            disableHibernationAndFastStartup(logger_);
        } else {
            core::console::line(
                core::i18n::tr("common.invalid"),
                core::console::warning);
        }

        if (pauseAfter) core::console::pause();
    }
}

} // namespace app
