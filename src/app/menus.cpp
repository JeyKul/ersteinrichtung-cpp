#include "menus.hpp"
#include "../bitlocker/bitlocker_menu.hpp"
#include "../core/console.hpp"
#include "../core/download.hpp"
#include "../core/system.hpp"
#include "../core/i18n.hpp"
#include "../core/process_runner.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace app {
namespace {
using core::console::line;
std::wstring text(const wchar_t* de, const wchar_t* en) {
    return core::i18n::language().rfind(L"de", 0) == 0 ? de : en;
}
class SleepGuard {
public:
    explicit SleepGuard(core::Logger& logger) {
        active_ = SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED) != 0;
        logger.write(active_ ? L"Session sleep prevention active; power plan unchanged." : L"WARNING: Session sleep prevention failed.");
    }
    ~SleepGuard() { if (active_) SetThreadExecutionState(ES_CONTINUOUS); }
    SleepGuard(const SleepGuard&) = delete;
    SleepGuard& operator=(const SleepGuard&) = delete;
private:
    bool active_ = false;
};
bool disableHibernation(core::Logger& logger) {
    if (!core::console::yesNo(text(
        L"Ruhezustand und Windows-Schnellstart dauerhaft deaktivieren? Normaler Standby bleibt verfügbar. Rückgängig mit: powercfg /hibernate on.",
        L"Permanently disable hibernation and Windows Fast Startup? Normal sleep remains available. Undo with: powercfg /hibernate on."), false)) return false;
    const auto result = core::runProcess(core::windowsSystemExecutable(L"powercfg.exe"), {L"/hibernate", L"off"});
    logger.write(L"powercfg /hibernate off: " + std::to_wstring(result.exitCode));
    logger.write(result.output + result.error);
    const bool ok = result.started && result.exitCode == 0;
    line(ok ? core::i18n::tr("common.done") : text(L"Deaktivieren fehlgeschlagen. Protokoll prüfen.", L"Disabling failed. Review the log."), ok ? core::console::good : core::console::error);
    return ok;
}
void downloadTool(const wchar_t* url, const wchar_t* filename, core::Logger& logger) {
    wchar_t temporary[MAX_PATH]{};
    if (!GetTempPathW(MAX_PATH, temporary)) { line(L"GetTempPathW failed", core::console::error); return; }
    const auto destination = std::filesystem::path(temporary) / filename;
    line(text(L"Download läuft...", L"Downloading..."), core::console::accent);
    const auto downloaded = core::downloadFile(url, destination);
    if (!downloaded.ok) { line(downloaded.error, core::console::error); logger.write(L"Download failed: " + downloaded.error); return; }
    if (!core::hasTrustedSignature(destination)) {
        line(core::i18n::tr("action.signature_warning"), core::console::warning);
        if (!core::console::yesNo(core::i18n::tr("action.launch_anyway"), false)) return;
    }
    core::ProcessOptions options;
    options.captureOutput = false;
    options.showWindow = true;
    core::ProcessResult result;
    if (destination.extension() == L".msi") result = core::runProcess(core::windowsSystemExecutable(L"msiexec.exe"), {L"/i", destination.wstring()}, options);
    else result = core::runProcess(destination.wstring(), {}, options);
    logger.write(std::wstring(filename) + L" exit=" + std::to_wstring(result.exitCode) + L" " + result.error);
    if (!result.started || (result.exitCode != 0 && result.exitCode != 3010)) line(text(L"Start oder Installation fehlgeschlagen. Protokoll prüfen.", L"Launch or installation failed. Review the log."), core::console::error);
}
void toolsMenu(Actions& a, core::Logger& logger) {
    for (;;) {
        core::console::clear();
        line();
        core::console::separator(core::i18n::tr("tools.title"));
        line();
        line(text(L"1.  Systeminformationen", L"1.  System information"));
        line(L"2.  WinDirStat");
        line(L"3.  Advanced IP Scanner");
        line(L"4.  Lenovo Service Bridge");
        line(L"5.  Lenovo System Update");
        line(L"6.  PowerShell 7");
        line(L"7.  WhyNotWin11");
        line(text(L"8.  Sicherheitsstatus", L"8.  Security status"));
        line(L"9.  BitLocker");
        line(L"10. Chris Titus Tech WinUtil");
        line(text(L"11. Ruhezustand und Windows-Schnellstart deaktivieren", L"11. Disable hibernation and Windows Fast Startup"));
        line();
        core::console::separator();
        line(L"0.  " + core::i18n::tr("common.back"), core::console::highlight);
        line();
        const auto c = core::console::prompt(core::i18n::tr("common.select"));
        if (c == L"0") return;
        if (c == L"1") { a.showSystemInformation(); continue; }
        if (c == L"9") { bitlocker::showMenu(logger); continue; }
        if (c == L"2") downloadTool(L"https://github.com/windirstat/windirstat/releases/latest/download/WinDirStat-x64.msi", L"WinDirStat-x64.msi", logger);
        else if (c == L"3") downloadTool(L"https://download.advanced-ip-scanner.com/download/files/Advanced_IP_Scanner_2.5.4594.1.exe", L"Advanced_IP_Scanner.exe", logger);
        else if (c == L"4") downloadTool(L"https://download.lenovo.com/lsbv4/LSBSetup.exe", L"LSBSetup.exe", logger);
        else if (c == L"5") downloadTool(L"https://download.lenovo.com/pccbbs/thinkvantage_en/system_update_5.08.03.59.exe", L"Lenovo-System-Update.exe", logger);
        else if (c == L"6") downloadTool(L"https://github.com/PowerShell/PowerShell/releases/download/v7.6.0/PowerShell-7.6.0-win-x64.msi", L"PowerShell-7.msi", logger);
        else if (c == L"7") a.runWhyNotWin11();
        else if (c == L"8") a.showSecurityStatus();
        else if (c == L"10") a.runChrisTitus();
        else if (c == L"11") disableHibernation(logger);
        else line(core::i18n::tr("common.invalid"), core::console::warning);
        core::console::pause();
    }
}
void header() {
    const auto s = core::querySystemStatus();
    line(core::i18n::tr("app.title"), core::console::title);
    line();
    auto field = [](const std::wstring& label, bool known, bool enabled) {
        line(label + L": " + core::i18n::tr(!known ? "common.unknown" : enabled ? "common.enabled" : "common.disabled"), !known ? core::console::warning : enabled ? core::console::good : core::console::normal);
    };
    field(core::i18n::tr("header.secure_boot"), s.secureBootKnown, s.secureBoot);
    field(core::i18n::tr("header.bitlocker"), s.bitLockerKnown, s.bitLocker);
    field(core::i18n::tr("header.chocolatey"), true, s.chocolatey);
    line(core::i18n::tr("header.pc_name") + L": " + s.computerName);
    line(L"CPU: " + s.cpu + L" | RAM: " + std::to_wstring(s.ramGiB) + L" GiB");
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
bool runBatch(Actions& a, bool rename, bool business, core::Logger& logger) {
    if (!core::console::confirm(core::i18n::tr(business ? "confirm.batch_business" : "confirm.batch_private"))) return false;
    // Hibernation is a persistent, separately confirmed setup change.
    // The session SleepGuard handles temporary sleep prevention independently.
    disableHibernation(logger);
    bool ok = true;
    if (rename) ok = a.renameComputer() && ok;
    ok = a.ensureChocolatey() && ok;
    ok = (business ? a.installBusinessRemote() : a.installSupremo()) && ok;
    ok = a.installDefaultPrograms() && ok;
    ok = a.installWindowsUpdates() && ok;
    if (business) ok = a.applyEnhancedPrivacy() && ok;
    line(core::i18n::tr(ok ? "batch.done" : "batch.failed"), ok ? core::console::good : core::console::warning);
    return ok;
}
}
void runMainMenu(Actions& a, core::Logger& logger) {
    SleepGuard sleepGuard(logger);
    if (!core::hasInternet()) line(core::i18n::tr("network.offline"), core::console::warning);
    for (;;) {
        core::console::clear(); header(); menuBody();
        const auto c = core::console::prompt(core::i18n::tr("common.select"));
        if (c == L"0" || c == L"q" || c == L"Q") { logger.write(L"Session finished"); return; }
        if (c == L"1") runBatch(a, true, false, logger);
        else if (c == L"2") runBatch(a, true, true, logger);
        else if (c == L"3") runBatch(a, false, false, logger);
        else if (c == L"4") runBatch(a, false, true, logger);
        else if (c == L"5") a.renameComputer();
        else if (c == L"6") a.ensureChocolatey();
        else if (c == L"7") a.installSupremo();
        else if (c == L"8") a.installBusinessRemote();
        else if (c == L"9") a.installDefaultPrograms();
        else if (c == L"10") a.installWindowsUpdates();
        else if (c == L"11") toolsMenu(a, logger);
        else if (c == L"12") a.showExtrasEnhanced();
        else if (c == L"13") bitlocker::showMenu(logger);
        else if (c == L"19") a.runSysprep();
        else if (c == L"20") a.runWhyNotWin11();
        else if (c == L"21") a.showSecurityStatus();
        else if (c == L"22") a.applyEnhancedPrivacy();
        else if (c == L"7000") a.runDiagnostics();
        else line(core::i18n::tr("common.invalid"), core::console::warning);
        if (c != L"11" && c != L"12" && c != L"13") core::console::pause();
    }
}
}
