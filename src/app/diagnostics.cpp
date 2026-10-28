#include "actions.hpp"
#include "../core/console.hpp"
#include "../core/download.hpp"
#include "../core/i18n.hpp"
#include "../core/process_runner.hpp"
#include "../core/system.hpp"
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

namespace app {
namespace {
using core::console::line;
struct DiagnosticRun {
    core::Logger& log;
    unsigned passed{}, warnings{}, failed{}, skipped{};
    bool ask(const std::wstring& name) {
        line(L"\n----------------------------------------------------------------", core::console::accent);
        line(L"TEST: " + name, core::console::title);
        if (!core::console::yesNo(L"Run this test?", false)) {
            ++skipped; line(L"SKIPPED", core::console::warning); log.write(L"DIAG SKIP " + name); return false;
        }
        log.write(L"DIAG START " + name); return true;
    }
    void result(const std::wstring& name, bool ok, const std::wstring& detail = {}, bool warning = false) {
        const WORD color = ok ? core::console::good : warning ? core::console::warning : core::console::error;
        line((ok ? L"PASS: " : warning ? L"WARN: " : L"FAIL: ") + name + (detail.empty() ? L"" : L" -- " + detail), color);
        if (ok) ++passed; else if (warning) ++warnings; else ++failed;
        log.write(L"DIAG " + std::wstring(ok ? L"PASS " : warning ? L"WARN " : L"FAIL ") + name + (detail.empty() ? L"" : L" -- " + detail));
    }
    bool process(const std::wstring& name, const std::wstring& exe, const std::vector<std::wstring>& args,
                 const std::vector<unsigned long>& accepted = {0}) {
        if (!ask(name)) return true;
        if (exe.empty() || !std::filesystem::exists(exe)) { result(name, false, L"executable not found"); return false; }
        core::ProcessOptions options;
        options.onOutput = [this](const std::wstring& text) { if (!text.empty()) { line(L"  " + text); log.write(L"DIAG OUTPUT " + text); } };
        const auto r = core::runProcess(exe, args, options);
        const bool ok = r.started && std::find(accepted.begin(), accepted.end(), r.exitCode) != accepted.end();
        result(name, ok, L"exit=" + std::to_wstring(r.exitCode)); return ok;
    }
};
std::filesystem::path diagnosticRoot() {
    wchar_t value[MAX_PATH]{}; GetTempPathW(MAX_PATH, value);
    return std::filesystem::path(value) / L"Ersteinrichtung-Diagnostics";
}
std::vector<std::wstring> fixedDrives() {
    std::vector<std::wstring> result; const DWORD mask = GetLogicalDrives();
    for (wchar_t c=L'A'; c<=L'Z'; ++c) { std::wstring root{c,L':',L'\\'};
        if ((mask & (1UL << (c-L'A'))) && GetDriveTypeW(root.c_str()) == DRIVE_FIXED) result.push_back({c,L':'}); }
    return result;
}
bool readDword(HKEY root, const wchar_t* path, const wchar_t* name, DWORD& value) {
    HKEY key{}; DWORD type{}, size=sizeof(value);
    if (RegOpenKeyExW(root,path,0,KEY_QUERY_VALUE|KEY_WOW64_64KEY,&key)!=ERROR_SUCCESS) return false;
    const LONG rc=RegQueryValueExW(key,name,nullptr,&type,reinterpret_cast<BYTE*>(&value),&size); RegCloseKey(key);
    return rc==ERROR_SUCCESS && type==REG_DWORD;
}
bool canWriteKey(HKEY root, const wchar_t* path) {
    HKEY key{}; const LONG rc=RegOpenKeyExW(root,path,0,KEY_SET_VALUE|KEY_WOW64_64KEY,&key); if(key)RegCloseKey(key); return rc==ERROR_SUCCESS;
}
std::wstring powershell() { return core::windowsSystemExecutable(L"WindowsPowerShell\\v1.0\\powershell.exe"); }
}

void Actions::runDiagnostics() {
    core::console::clear(); core::console::separator(L"HIDDEN FULL DIAGNOSTICS 7000");
    line(L"Every test asks before it runs. Destructive operations are never executed.", core::console::warning);
    line(L"Downloads and a real driver export may use substantial time and disk space.", core::console::warning);
    line(L"BitLocker mutation commands are syntax-tested with their help switches only.", core::console::warning);
    if (!core::console::yesNo(L"Start the intensive diagnostic suite?", false)) return;
    DiagnosticRun d{logger_}; const auto root=diagnosticRoot(); std::error_code ec; std::filesystem::create_directories(root,ec);

    if (d.ask(L"Administrator token, localization and Unicode console")) {
        const bool ok=core::isAdministrator() && core::i18n::loaded();
        line(L"  Language: "+core::i18n::language()+L" | ä ö ü Ä Ö Ü ß | English",core::console::accent);
        d.result(L"Administrator/localization/Unicode",ok,core::i18n::loadedFile().wstring());
    }
    if (d.ask(L"Native system information and fixed-drive enumeration")) {
        const auto s=core::querySystemStatus(); const auto drives=fixedDrives();
        line(L"  "+s.computerName+L" | "+s.manufacturer+L" "+s.model+L" | "+s.cpu);
        line(L"  Windows "+s.windowsVersion+L" build "+s.windowsBuild+L" | RAM "+std::to_wstring(s.ramGiB)+L" GiB");
        for(const auto& drive:drives) line(L"  Fixed drive: "+drive);
        d.result(L"System information",!s.computerName.empty()&&!s.cpu.empty()&&!drives.empty());
    }
    if (d.ask(L"Internet connectivity")) d.result(L"Internet connectivity",core::hasInternet());

    const std::vector<std::pair<std::wstring,std::wstring>> tools={
        {L"manage-bde",core::windowsSystemExecutable(L"manage-bde.exe")},{L"DISM",core::windowsSystemExecutable(L"dism.exe")},
        {L"PnPUtil",core::windowsSystemExecutable(L"pnputil.exe")},{L"PowerCfg",core::windowsSystemExecutable(L"powercfg.exe")},
        {L"USOClient",core::windowsSystemExecutable(L"UsoClient.exe")},{L"Sysprep",core::windowsSystemExecutable(L"Sysprep\\sysprep.exe")},
        {L"WSReset",core::windowsSystemExecutable(L"wsreset.exe")},{L"Service Control",core::windowsSystemExecutable(L"sc.exe")},
        {L"PowerShell",powershell()}};
    for(const auto& [name,path]:tools) if(d.ask(L"Executable availability: "+name)) d.result(name,!path.empty()&&std::filesystem::exists(path),path);

    struct DownloadCase { const wchar_t* name; const wchar_t* url; const wchar_t* file; bool signedFile; };
    const DownloadCase downloads[]={
        {L"Chocolatey bootstrap",L"https://community.chocolatey.org/install.ps1",L"chocolatey-install.ps1",false},
        {L"Supremo",L"https://www.ct-t.de/prog/Supremo.exe",L"Supremo.exe",true},
        {L"WhyNotWin11",L"https://github.com/rcmaehl/WhyNotWin11/releases/download/2.7.0/WhyNotWin11.exe",L"WhyNotWin11.exe",true},
        {L"WinDirStat",L"https://github.com/windirstat/windirstat/releases/latest/download/WinDirStat-x64.msi",L"WinDirStat-x64.msi",true},
        {L"Advanced IP Scanner",L"https://download.advanced-ip-scanner.com/download/files/Advanced_IP_Scanner_2.5.4594.1.exe",L"Advanced_IP_Scanner.exe",true},
        {L"Lenovo Service Bridge",L"https://download.lenovo.com/lsbv4/LSBSetup.exe",L"LSBSetup.exe",true},
        {L"Lenovo System Update",L"https://download.lenovo.com/pccbbs/thinkvantage_en/system_update_5.08.03.59.exe",L"Lenovo-System-Update.exe",true},
        {L"PowerShell 7",L"https://github.com/PowerShell/PowerShell/releases/download/v7.6.0/PowerShell-7.6.0-win-x64.msi",L"PowerShell-7.msi",true},
        {L"Chris Titus WinUtil",L"https://christitus.com/win",L"winutil.ps1",false}};
    const auto downloadDir=root/L"downloads"; std::filesystem::create_directories(downloadDir,ec);
    for(const auto& item:downloads) {
        const std::wstring test=L"Download without execution: "+std::wstring(item.name);
        if(!d.ask(test)) continue;
        const auto file=downloadDir/item.file; std::filesystem::remove(file,ec);
        line(L"  GET "+std::wstring(item.url)); const auto r=core::downloadFile(item.url,file);
        bool ok=r.ok&&std::filesystem::exists(file)&&std::filesystem::file_size(file,ec)>0;
        std::wstring detail=L"HTTP "+std::to_wstring(r.status);
        if(ok) detail+=L", "+std::to_wstring(std::filesystem::file_size(file,ec))+L" bytes";
        if(ok&&item.signedFile){const bool signature=core::hasTrustedSignature(file);detail+=signature?L", signature valid":L", signature INVALID";ok=ok&&signature;}
        if(!r.error.empty())detail+=L", "+r.error; d.result(test,ok,detail); std::filesystem::remove(file,ec);
    }

    const auto ps=powershell();
    d.process(L"AppX inventory for all users and provisioned image",ps,{L"-NoProfile",L"-Command",L"Get-AppxPackage -AllUsers | Select Name,PackageFullName; Get-AppxProvisionedPackage -Online | Select DisplayName,PackageName"});
    d.process(L"Windows Update read-only search",ps,{L"-NoProfile",L"-Command",L"$s=(New-Object -ComObject Microsoft.Update.Session).CreateUpdateSearcher();$r=$s.Search('IsInstalled=0 and IsHidden=0');Write-Host ('Applicable updates: '+$r.Updates.Count)"});
    d.process(L"Power configuration read-only query",core::windowsSystemExecutable(L"powercfg.exe"),{L"/query"});
    d.process(L"Driver inventory through DISM",core::windowsSystemExecutable(L"dism.exe"),{L"/online",L"/get-drivers",L"/format:table"});
    d.process(L"Driver inventory through PnPUtil",core::windowsSystemExecutable(L"pnputil.exe"),{L"/enum-drivers"});

    if(d.ask(L"REAL non-destructive driver export to temporary storage")) {
        const auto destination=root/L"driver-export"; std::filesystem::remove_all(destination,ec); std::filesystem::create_directories(destination,ec);
        core::ProcessOptions options; options.onOutput=[&](const std::wstring& text){line(L"  "+text);logger_.write(L"DIAG DRIVER "+text);};
        const auto r=core::runProcess(core::windowsSystemExecutable(L"dism.exe"),{L"/online",L"/export-driver",L"/destination:"+destination.wstring()},options);
        std::size_t infCount{}; if(std::filesystem::exists(destination))for(const auto& e:std::filesystem::recursive_directory_iterator(destination,ec))if(e.is_regular_file()&&_wcsicmp(e.path().extension().wstring().c_str(),L".inf")==0)++infCount;
        d.result(L"Driver export",r.started&&r.exitCode==0&&infCount>0,L"exit="+std::to_wstring(r.exitCode)+L", INF="+std::to_wstring(infCount));
        if(core::console::yesNo(L"Delete the temporary exported drivers now?",true))std::filesystem::remove_all(destination,ec);
    }
    if(d.ask(L"Driver installation path parser and recursive INF discovery")) {
        const auto probe=root/L"path test äöü"/L"sub";std::filesystem::create_directories(probe,ec);std::ofstream(probe/L"test.inf")<<"[Version]\nSignature=\"$Windows NT$\"\n";
        std::size_t found{};for(const auto& e:std::filesystem::recursive_directory_iterator(root/L"path test äöü",ec))if(e.path().extension()==L".inf")++found;
        d.result(L"Driver path/INF discovery",found==1,(root/L"path test äöü").wstring());std::filesystem::remove_all(root/L"path test äöü",ec);
    }

    if(d.ask(L"Sysprep unattended asset and XML parsing")) {
        const auto asset=std::filesystem::path(core::executableDirectory())/L"assets\\unattend.xml";
        const auto r=core::runProcess(ps,{L"-NoProfile",L"-Command",L"[xml](Get-Content -LiteralPath '"+asset.wstring()+L"' -Raw)|Out-Null"});
        d.result(L"Sysprep asset/XML",std::filesystem::exists(asset)&&r.started&&r.exitCode==0,asset.wstring());
    }
    if(d.ask(L"Registry write permissions for privacy and LTSC functions")) {
        const bool privacy=canWriteKey(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows")||core::isAdministrator();
        const bool edition=canWriteKey(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
        d.result(L"Registry permissions",privacy&&edition,L"privacy="+std::wstring(privacy?L"yes":L"no")+L", edition="+(edition?L"yes":L"no"));
    }
    struct Policy { HKEY root; const wchar_t* path; const wchar_t* name; DWORD expected; };
    const Policy policies[]={
        {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",L"AllowTelemetry",0},
        {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\Explorer",L"DisableSearchBoxSuggestions",1},
        {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent",L"DisableWindowsConsumerFeatures",1},
        {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\System",L"PublishUserActivities",0},
        {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",L"AllowCloudSearch",0}};
    for(const auto& p:policies)if(d.ask(L"Privacy policy audit: "+std::wstring(p.name))){DWORD v{};const bool present=readDword(p.root,p.path,p.name,v);d.result(p.name,present&&v==p.expected,present?L"current="+std::to_wstring(v)+L", expected="+std::to_wstring(p.expected):L"not configured",true);}
    d.process(L"Telemetry service state: DiagTrack",core::windowsSystemExecutable(L"sc.exe"),{L"qc",L"DiagTrack"});
    d.process(L"Error reporting service state: WerSvc",core::windowsSystemExecutable(L"sc.exe"),{L"qc",L"WerSvc"});
    d.process(L"Persistent privacy enforcement task state",core::windowsSystemExecutable(L"schtasks.exe"),{L"/Query",L"/TN",L"Ersteinrichtung Privacy Enforcement",L"/V",L"/FO",L"LIST"},{0,1});

    const auto manage=core::windowsSystemExecutable(L"manage-bde.exe");
    const auto drives=fixedDrives();
    for(const auto& drive:drives) {
        d.process(L"BitLocker status: "+drive,manage,{L"-status",drive},{0,1});
        d.process(L"BitLocker protectors: "+drive,manage,{L"-protectors",L"-get",drive},{0,1});
    }
    d.process(L"BitLocker all-volume status",manage,{L"-status"},{0,1});
    d.process(L"BitLocker auto-unlock status",manage,{L"-autounlock",L"-status"},{0,1});
    d.process(L"PowerShell Get-BitLockerVolume",ps,{L"-NoProfile",L"-Command",L"Get-BitLockerVolume | Format-List MountPoint,VolumeType,VolumeStatus,ProtectionStatus,EncryptionPercentage,EncryptionMethod,AutoUnlockEnabled,KeyProtector"});
    d.process(L"TPM status",ps,{L"-NoProfile",L"-Command",L"Get-Tpm | Format-List *"},{0,1});
    const std::vector<std::wstring> bitlockerHelp={L"-on",L"-off",L"-pause",L"-resume",L"-autounlock",L"-protectors",L"-upgrade",L"-wipefreespace"};
    for(const auto& command:bitlockerHelp)d.process(L"BitLocker SAFE syntax/help test: manage-bde "+command,manage,{command,L"-?"},{0,1});

    if(d.ask(L"Batch workflow dependency simulation (no changes)")) {
        wchar_t pd[MAX_PATH]{};GetEnvironmentVariableW(L"ProgramData",pd,MAX_PATH);
        const bool choco=std::filesystem::exists(std::filesystem::path(pd)/L"chocolatey\\bin\\choco.exe");
        const bool remote=core::hasInternet();const bool update=!core::windowsSystemExecutable(L"UsoClient.exe").empty();
        d.result(L"Batch simulation",remote&&update,L"Chocolatey="+std::wstring(choco?L"present":L"would install")+L", network="+(remote?L"yes":L"no")+L", update client="+(update?L"yes":L"no"),!choco);
    }
    std::filesystem::remove_all(downloadDir,ec);
    core::console::separator(L"DIAGNOSTIC SUMMARY");
    line(L"Passed: "+std::to_wstring(d.passed),core::console::good);
    line(L"Warnings: "+std::to_wstring(d.warnings),core::console::warning);
    line(L"Failed: "+std::to_wstring(d.failed),d.failed?core::console::error:core::console::good);
    line(L"Skipped: "+std::to_wstring(d.skipped),core::console::warning);
    line(L"Full details were written to the normal Ersteinrichtung session log.");
    logger_.write(L"DIAG SUMMARY pass="+std::to_wstring(d.passed)+L" warn="+std::to_wstring(d.warnings)+L" fail="+std::to_wstring(d.failed)+L" skip="+std::to_wstring(d.skipped));
}
}
