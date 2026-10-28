#include <cwctype>
#include "actions.hpp"
#include "../bitlocker/bitlocker_menu.hpp"
#include "../core/console.hpp"
#include "../core/download.hpp"
#include "../core/process_runner.hpp"
#include "../core/system.hpp"
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <vector>

namespace app {
namespace {
using core::console::line;
std::filesystem::path tempPath(const std::wstring& file) {
    wchar_t value[MAX_PATH]{}; GetTempPathW(MAX_PATH, value); return std::filesystem::path(value) / file;
}
std::filesystem::path publicDesktop() {
    wchar_t value[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_COMMON_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, value) == S_OK) return value;
    return L"C:\\Users\\Public\\Desktop";
}
bool setDword(HKEY root, const wchar_t* path, const wchar_t* name, DWORD value) {
    HKEY key{}; const LONG opened = RegCreateKeyExW(root, path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (opened != ERROR_SUCCESS) return false;
    const LONG result = RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key); return result == ERROR_SUCCESS;
}
bool setString(HKEY root, const wchar_t* path, const wchar_t* name, const wchar_t* value) {
    HKEY key{}; if (RegCreateKeyExW(root, path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    const DWORD size = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));
    const LONG result = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value), size);
    RegCloseKey(key); return result == ERROR_SUCCESS;
}
std::wstring cleanModel(std::wstring model) {
    for (auto& c : model) if (std::iswspace(c)) c = L'_';
    model.erase(std::remove_if(model.begin(), model.end(), [](wchar_t c){ return !(std::iswalnum(c) || c == L'_' || c == L'-'); }), model.end());
    return model.empty() ? L"Unknown_Model" : model;
}
}
bool Actions::runLogged(const std::wstring& label, const std::wstring& exe, const std::vector<std::wstring>& args, bool visible) {
    logger_.write(L"START " + label);
    line(L"\n==> " + label, core::console::accent);
    core::ProcessOptions options; options.captureOutput = !visible; options.showWindow = visible;
    const auto result = core::runProcess(exe, args, options);
    if (!result.output.empty()) std::wcout << result.output << L"\n";
    if (!result.error.empty()) line(result.error, core::console::error);
    const bool ok = result.started && result.exitCode == 0;
    line(ok ? L"Done." : L"Failed (exit code " + std::to_wstring(result.exitCode) + L").", ok ? core::console::good : core::console::error);
    logger_.write((ok ? L"OK " : L"FAIL ") + label + L" exit=" + std::to_wstring(result.exitCode));
    return ok;
}
bool Actions::downloadAndLaunch(const std::wstring& label, const std::wstring& url, const std::wstring& fileName,
                                const std::vector<std::wstring>& args, bool wait) {
    const auto destination = tempPath(fileName);
    if (!std::filesystem::exists(destination)) {
        line(L"Downloading " + label + L"...", core::console::accent);
        const auto result = core::downloadFile(url, destination);
        if (!result.ok) { line(L"Download failed: " + result.error, core::console::error); logger_.write(L"Download failed: " + label); return false; }
    }
    const auto extension = destination.extension().wstring();
    if ((extension == L".exe" || extension == L".msi") && !core::hasTrustedSignature(destination)) {
        line(L"Warning: Windows could not verify the file's Authenticode signature.", core::console::warning);
        if (!core::console::yesNo(L"Launch it anyway?")) return false;
    }
    core::ProcessOptions options; options.captureOutput = false; options.showWindow = true; options.wait = wait;
    std::wstring executable = destination.wstring();
    std::vector<std::wstring> launchArgs = args;
    if (extension == L".msi") {
        executable = core::windowsSystemExecutable(L"msiexec.exe");
        launchArgs.insert(launchArgs.begin(), destination.wstring());
        launchArgs.insert(launchArgs.begin(), L"/i");
    }
    const auto result = core::runProcess(executable, launchArgs, options);
    logger_.write((result.started ? L"Launched " : L"Could not launch ") + label);
    return result.started && (!wait || result.exitCode == 0);
}
bool Actions::renameComputer() {
    const auto name = core::console::prompt(L"New computer name (1-15 chars, Enter cancels): ");
    if (name.empty()) return true;
    if (name.size() > MAX_COMPUTERNAME_LENGTH || name.front() == L'-' || name.back() == L'-' ||
        std::any_of(name.begin(), name.end(), [](wchar_t c){ return !(std::iswalnum(c) || c == L'-'); })) {
        line(L"Invalid NetBIOS computer name.", core::console::error); return false;
    }
    if (!core::console::confirmExact(L"RENAME " + name)) return false;
    if (!SetComputerNameExW(ComputerNamePhysicalDnsHostname, name.c_str())) {
        line(L"Rename failed: " + std::to_wstring(GetLastError()), core::console::error); return false;
    }
    line(L"Computer renamed. A restart is required.", core::console::good); logger_.write(L"Computer renamed to " + name); return true;
}
bool Actions::ensureChocolatey() {
    wchar_t data[MAX_PATH]{}; GetEnvironmentVariableW(L"ProgramData", data, MAX_PATH);
    const auto choco = (std::filesystem::path(data) / L"chocolatey\\bin\\choco.exe").wstring();
    if (std::filesystem::exists(choco)) return runLogged(L"Upgrade Chocolatey", choco, {L"upgrade", L"chocolatey", L"-y"});
    line(L"Chocolatey is missing. The official bootstrap script will be downloaded and run.", core::console::warning);
    if (!core::console::confirmExact(L"INSTALL CHOCOLATEY")) return false;
    const auto script = tempPath(L"install-chocolatey.ps1");
    const auto downloaded = core::downloadFile(L"https://community.chocolatey.org/install.ps1", script);
    if (!downloaded.ok) { line(downloaded.error, core::console::error); return false; }
    const auto powershell = core::windowsSystemExecutable(L"WindowsPowerShell\\v1.0\\powershell.exe");
    return runLogged(L"Install Chocolatey", powershell,
        {L"-NoProfile", L"-ExecutionPolicy", L"Bypass", L"-File", script.wstring()});
}
bool Actions::installSupremo(bool businessShortcut) {
    const std::filesystem::path directory = L"C:\\CT-T"; const auto target = directory / L"Supremo.exe";
    std::error_code ec; std::filesystem::create_directories(directory, ec);
    if (!std::filesystem::exists(target)) {
        const auto result = core::downloadFile(L"https://www.ct-t.de/prog/Supremo.exe", target);
        if (!result.ok) { line(result.error, core::console::error); return false; }
    }
    if (!core::hasTrustedSignature(target)) { line(L"Supremo signature verification failed; shortcut was not created.", core::console::error); return false; }
    const auto link = publicDesktop() / (businessShortcut ? L"CT-T Support Supremo.lnk" : L"CT-T Support.lnk");
    const auto ps = core::windowsSystemExecutable(L"WindowsPowerShell\\v1.0\\powershell.exe");
    const std::wstring command = L"$s=(New-Object -ComObject WScript.Shell).CreateShortcut('" + link.wstring() +
        L"');$s.TargetPath='" + target.wstring() + L"';$s.WorkingDirectory='C:\\CT-T';$s.Description='CT-T Remote Support';$s.Save()";
    return runLogged(L"Create Supremo shortcut", ps, {L"-NoProfile", L"-Command", command});
}
bool Actions::installBusinessRemote() {
    bool ok = installSupremo(true);
    const auto bundled = std::filesystem::path(core::executableDirectory()) / L"assets\\TeamViewer_Host_Setup.exe";
    if (std::filesystem::exists(bundled)) ok = runLogged(L"Install TeamViewer Host", bundled.wstring(), {L"/S"}) && ok;
    else line(L"TeamViewer installer not bundled; Supremo was installed. Put it at assets\\TeamViewer_Host_Setup.exe to enable this step.", core::console::warning);
    return ok;
}
bool Actions::installDefaultPrograms() {
    wchar_t data[MAX_PATH]{}; GetEnvironmentVariableW(L"ProgramData", data, MAX_PATH);
    const auto choco = (std::filesystem::path(data) / L"chocolatey\\bin\\choco.exe").wstring();
    if (!std::filesystem::exists(choco) && !ensureChocolatey()) return false;
    if (!std::filesystem::exists(choco)) { line(L"Chocolatey still not found. Restart this application and retry.", core::console::error); return false; }
    return runLogged(L"Install default programs", choco, {L"install", L"vlc", L"7zip-zstd", L"firefox", L"-y"});
}
bool Actions::installWindowsUpdates() {
    const auto uso = core::windowsSystemExecutable(L"UsoClient.exe");
    if (!std::filesystem::exists(uso)) { line(L"Windows Update client was not found.", core::console::error); return false; }
    bool ok = runLogged(L"Scan for Windows updates", uso, {L"StartScan"});
    ok = runLogged(L"Download Windows updates", uso, {L"StartDownload"}) && ok;
    ok = runLogged(L"Install Windows updates", uso, {L"StartInstall"}) && ok;
    line(L"Windows Update continues in the background. Check Settings for status/restart requests.", core::console::warning);
    return ok;
}
bool Actions::applyPrivacy() {
    line(L"Applies the same privacy policy set as the PowerShell version. Some settings may be edition-dependent.", core::console::warning);
    if (!core::console::confirmExact(L"APPLY PRIVACY")) return false;
    struct Entry { HKEY root; const wchar_t* path; const wchar_t* name; DWORD value; };
    const Entry entries[] = {
        {HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo",L"Enabled",0},
        {HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy",L"TailoredExperiencesWithDiagnosticDataEnabled",0},
        {HKEY_CURRENT_USER,L"Software\\Microsoft\\Speech_OneCore\\Settings\\OnlineSpeechPrivacy",L"HasAccepted",0},
        {HKEY_CURRENT_USER,L"Software\\Microsoft\\Input\\TIPC",L"Enabled",0},
        {HKEY_CURRENT_USER,L"Software\\Microsoft\\InputPersonalization",L"RestrictImplicitInkCollection",1},
        {HKEY_CURRENT_USER,L"Software\\Microsoft\\InputPersonalization",L"RestrictImplicitTextCollection",1},
        {HKEY_CURRENT_USER,L"Software\\Microsoft\\InputPersonalization\\TrainedDataStore",L"HarvestContacts",0},
        {HKEY_CURRENT_USER,L"Software\\Microsoft\\Personalization\\Settings",L"AcceptedPrivacyPolicy",0},
        {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\DataCollection",L"AllowTelemetry",0},
        {HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",L"Start_TrackProgs",0},
        {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\System",L"PublishUserActivities",0},
        {HKEY_CURRENT_USER,L"Software\\Microsoft\\Siuf\\Rules",L"NumberOfSIUFInPeriod",0}
    };
    bool ok = true; for (const auto& e : entries) ok = setDword(e.root, e.path, e.name, e.value) && ok;
    const auto sc = core::windowsSystemExecutable(L"sc.exe");
    runLogged(L"Disable DiagTrack", sc, {L"config", L"DiagTrack", L"start=", L"disabled"});
    runLogged(L"Disable Windows Error Reporting", sc, {L"config", L"WerSvc", L"start=", L"disabled"});
    line(ok ? L"Privacy settings applied." : L"Some registry settings failed; see log.", ok ? core::console::good : core::console::warning);
    logger_.write(ok ? L"Privacy settings applied" : L"Privacy settings partially failed"); return ok;
}
bool Actions::runSysprep() {
    const auto source = std::filesystem::path(core::executableDirectory()) / L"assets\\unattend.xml";
    if (!std::filesystem::exists(source)) { line(L"Missing assets\\unattend.xml.", core::console::error); return false; }
    line(L"Sysprep closes this session and reboots into OOBE.", core::console::warning);
    if (!core::console::confirmExact(L"SYSPREP OOBE")) return false;
    const auto sysprep = core::windowsSystemExecutable(L"Sysprep\\sysprep.exe");
    return runLogged(L"Sysprep OOBE", sysprep, {L"/oobe", L"/reboot", L"/unattend:" + source.wstring()}, true);
}
bool Actions::runWhyNotWin11() {
    if (!downloadAndLaunch(L"WhyNotWin11", L"https://github.com/rcmaehl/WhyNotWin11/releases/download/2.7.0/WhyNotWin11.exe", L"WhyNotWin11.exe", {}, false)) return false;
    line(L"WhyNotWin11 launched. Export its report from the application if needed."); return true;
}
bool Actions::showSecurityStatus() {
    const auto status = core::querySystemStatus();
    line(L"\n=== System security ===", core::console::accent);
    line(L"Secure Boot: " + std::wstring(!status.secureBootKnown ? L"unknown" : status.secureBoot ? L"ON" : L"OFF"), status.secureBoot ? core::console::good : core::console::error);
    line(L"BitLocker C:: " + std::wstring(!status.bitLockerKnown ? L"unknown/unavailable" : status.bitLocker ? L"ON" : L"OFF"), status.bitLocker ? core::console::good : core::console::error);
    return status.secureBoot && status.bitLocker;
}
bool Actions::setPreventSleep(bool enabled) {
    const auto powercfg = core::windowsSystemExecutable(L"powercfg.exe");
    const std::vector<std::pair<std::wstring,std::wstring>> values = enabled ?
        std::vector<std::pair<std::wstring,std::wstring>>{{L"standby-timeout-ac",L"0"},{L"standby-timeout-dc",L"0"},{L"hibernate-timeout-ac",L"0"},{L"hibernate-timeout-dc",L"0"},{L"monitor-timeout-ac",L"0"},{L"monitor-timeout-dc",L"0"}} :
        std::vector<std::pair<std::wstring,std::wstring>>{{L"standby-timeout-ac",L"30"},{L"standby-timeout-dc",L"15"},{L"hibernate-timeout-ac",L"180"},{L"hibernate-timeout-dc",L"60"},{L"monitor-timeout-ac",L"10"},{L"monitor-timeout-dc",L"5"}};
    bool ok = true; for (const auto& [setting,value] : values) ok = runLogged((enabled?L"Disable ":L"Restore ")+setting, powercfg, {L"-change", setting, value}) && ok; return ok;
}
bool Actions::exportDrivers() {
    auto drive = core::console::prompt(L"Destination drive letter (e.g. E): "); if (drive.empty()) return false;
    const wchar_t letter = static_cast<wchar_t>(std::towupper(drive.front())); if (letter < L'A' || letter > L'Z') return false;
    const auto destination = std::filesystem::path(std::wstring(1,letter)+L":\\extracted_drivers") / cleanModel(core::systemModel());
    std::error_code ec; std::filesystem::create_directories(destination, ec);
    return runLogged(L"Export drivers", core::windowsSystemExecutable(L"dism.exe"), {L"/online", L"/export-driver", L"/destination:"+destination.wstring()});
}
bool Actions::installDrivers() {
    auto drive = core::console::prompt(L"Source drive letter (e.g. E): "); if (drive.empty()) return false;
    const wchar_t letter = static_cast<wchar_t>(std::towupper(drive.front()));
    const auto source = std::filesystem::path(std::wstring(1,letter)+L":\\extracted_drivers") / cleanModel(core::systemModel());
    if (!std::filesystem::exists(source)) { line(L"Driver directory not found: "+source.wstring(), core::console::error); return false; }
    if (!core::console::confirmExact(L"INSTALL DRIVERS")) return false;
    return runLogged(L"Install drivers", core::windowsSystemExecutable(L"pnputil.exe"), {L"/add-driver", (source/L"*.inf").wstring(), L"/subdirs", L"/install"});
}
bool Actions::enableLtscUpgradePath() {
    line(L"Changes Windows EditionID/ProductName to permit an in-place Pro setup. This does not activate Windows.", core::console::warning);
    if (!core::console::confirmExact(L"ENABLE LTSC TO PRO")) return false;
    const wchar_t* path=L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    const bool ok=setString(HKEY_LOCAL_MACHINE,path,L"EditionID",L"Professional") && setString(HKEY_LOCAL_MACHINE,path,L"ProductName",L"Windows 10 Pro");
    line(ok?L"Registry updated. Run equal/newer Pro setup and choose Keep files and apps.":L"Registry update failed.", ok?core::console::good:core::console::error); return ok;
}
void Actions::showTools() {
    for (;;) {
        core::console::clear(); core::console::separator(L"TOOLS");
        line(L"1.  WinDirStat\n2.  Advanced IP Scanner\n3.  Lenovo Service Bridge/System Update\n4.  System Informationen\n5.  Windows Update\n6.  PowerShell 7\n7.  TEMP leeren");
        core::console::separator(); line(L"0.  Zurück", core::console::highlight); line();
        const auto c=core::console::prompt(L"Select: "); if(c==L"0") return;
        if(c==L"1") downloadAndLaunch(L"WinDirStat installer",L"https://github.com/windirstat/windirstat/releases/latest/download/WinDirStat-x64.msi",L"WinDirStat-x64.msi",{},false);
        else if(c==L"2") downloadAndLaunch(L"Advanced IP Scanner",L"https://download.advanced-ip-scanner.com/download/files/Advanced_IP_Scanner_2.5.4594.1.exe",L"Advanced_IP_Scanner_2.5.4594.1.exe",{},false);
        else if(c==L"3" && core::console::confirmExact(L"INSTALL LENOVO TOOLS")) { downloadAndLaunch(L"Lenovo Service Bridge",L"https://download.lenovo.com/lsbv4/LSBSetup.exe",L"LSBSetup.exe",{L"/SILENT"}); downloadAndLaunch(L"Lenovo System Update",L"https://download.lenovo.com/pccbbs/thinkvantage_en/system_update_5.08.03.59.exe",L"system_update_5.08.03.59.exe",{L"/SILENT"}); }
        else if(c==L"4") { const auto s=core::querySystemStatus(); std::wcout<<L"\nPC: "<<s.computerName<<L"\nCPU: "<<s.cpu<<L"\nRAM: "<<std::fixed<<std::setprecision(1)<<s.ramGiB<<L" GiB\n"; }
        else if(c==L"5") installWindowsUpdates();
        else if(c==L"6") downloadAndLaunch(L"PowerShell 7",L"https://github.com/PowerShell/PowerShell/releases/download/v7.6.0/PowerShell-7.6.0-win-x64.msi",L"PowerShell-7.6.0-win-x64.msi",{L"/quiet",L"ADD_PATH=1"});
        else if(c==L"7" && core::console::confirmExact(L"CLEAR TEMP")) { std::error_code ec; for(const auto& e:std::filesystem::directory_iterator(tempPath(L""),ec)) std::filesystem::remove_all(e.path(),ec); line(L"TEMP cleanup attempted."); }
        core::console::pause();
    }
}
void Actions::showExtras() {
    for (;;) {
        core::console::clear(); core::console::separator(L"EXTRAS");
        line(L"1.  Windows Store installieren (LTSC)\n2.  App Installer / WinGet Store-Seite öffnen\n3.  LTSC -> Pro Upgrade-Pfad aktivieren\n10. Treiber exportieren\n11. Exportierte Treiber installieren");
        core::console::separator(); line(L"0.  Zurück", core::console::highlight); line();
        const auto c=core::console::prompt(L"Select: "); if(c==L"0") return;
        if(c==L"1") runLogged(L"Install Windows Store",core::windowsSystemExecutable(L"wsreset.exe"),{L"-i"},true);
        else if(c==L"2") core::shellOpen(L"ms-windows-store://pdp/?ProductId=9NBLGGH4NNS1");
        else if(c==L"3") enableLtscUpgradePath(); else if(c==L"10") exportDrivers(); else if(c==L"11") installDrivers();
        core::console::pause();
    }
}
}
