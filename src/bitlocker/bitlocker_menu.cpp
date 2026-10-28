#include "bitlocker_menu.hpp"
#include "../core/console.hpp"
#include "../core/logger.hpp"
#include "../core/process_runner.hpp"
#include "../core/system.hpp"
#include <windows.h>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace bitlocker {
namespace {
std::wstring selectedDrive=L"C:";
core::ProcessResult manage(const std::vector<std::wstring>& args, core::Logger& log, bool sensitive=false) {
    const auto exe=core::windowsSystemExecutable(L"manage-bde.exe");
    if(exe.empty()) return {1,{},L"manage-bde.exe not found",false};
    log.write(L"manage-bde operation on "+selectedDrive);
    const auto r=core::runProcess(exe,args);
    if(!r.error.empty()) core::console::line(r.error,core::console::error);
    if(!sensitive&&!r.output.empty()) std::wcout<<L"\n"<<r.output;
    else if(sensitive) core::console::line(L"Sensitive protector output hidden.",core::console::warning);
    core::console::line(L"Exit code: "+std::to_wstring(r.exitCode),r.exitCode==0?core::console::good:core::console::error);
    return r;
}
bool confirm(const std::wstring& action){return core::console::confirmExact(action+L" "+selectedDrive);}
void selectDrive(){
    core::console::line(L"\nFixed drives:"); const DWORD mask=GetLogicalDrives();
    for(wchar_t c=L'A';c<=L'Z';++c){std::wstring root{c,L':',L'\\'};if((mask&(1UL<<(c-L'A')))&&GetDriveTypeW(root.c_str())==DRIVE_FIXED)std::wcout<<L"  "<<c<<L":\n";}
    const auto input=core::console::prompt(L"Drive letter: ");if(input.empty())return;const wchar_t c=std::towupper(input[0]);std::wstring root{c,L':',L'\\'};
    if(c>=L'A'&&c<=L'Z'&&GetDriveTypeW(root.c_str())==DRIVE_FIXED)selectedDrive={c,L':'};else core::console::line(L"Not a fixed mounted drive.",core::console::error);
}
bool destinationSafe(const std::filesystem::path& path){
    if(!path.is_absolute()||path.filename().empty())return false;
    const auto root=path.root_name().wstring(); return root.empty()||_wcsicmp(root.c_str(),selectedDrive.c_str())!=0;
}
void exportRecovery(core::Logger& log){
    core::console::line(L"Recovery passwords are secrets. Export outside the encrypted target volume.",core::console::warning);
    if(!confirm(L"EXPORT-RECOVERY")) return;
    const std::filesystem::path path=core::console::prompt(L"Absolute output file: ");
    if(!destinationSafe(path)){core::console::line(L"Use an absolute file path on a different volume.",core::console::error);return;}
    const auto r=manage({L"-protectors",L"-get",selectedDrive},log,true);if(r.exitCode!=0)return;
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);std::ofstream file(path,std::ios::binary|std::ios::trunc);
    const int n=WideCharToMultiByte(CP_UTF8,0,r.output.data(),static_cast<int>(r.output.size()),nullptr,0,nullptr,nullptr);std::string utf8(static_cast<std::size_t>(n),'\0');
    if(n) WideCharToMultiByte(CP_UTF8,0,r.output.data(),static_cast<int>(r.output.size()),utf8.data(),n,nullptr,nullptr);
    file.write(utf8.data(),static_cast<std::streamsize>(utf8.size()));
    core::console::line(file.good()?L"Recovery information exported.":L"Export failed.",file.good()?core::console::good:core::console::error);
}
void mutate(const std::wstring& action,const std::vector<std::wstring>& args,core::Logger& log,bool sensitive=false){if(confirm(action))manage(args,log,sensitive);}
}
void showMenu(core::Logger& log){
    for(;;){
        core::console::clear();core::console::separator(L"BITLOCKER | Laufwerk: "+selectedDrive);
        core::console::line(L"1.  Übersicht\n2.  Festes Laufwerk auswählen\n3.  Verschlüsseln (belegter Speicher + Wiederherstellungspasswort)\n4.  Entschlüsseln\n5.  Verschlüsselung pausieren\n6.  Verschlüsselung fortsetzen\n7.  Automatische Entsperrung aktivieren\n8.  Automatische Entsperrung deaktivieren\n9.  Wiederherstellungsschutz exportieren\n10. Wiederherstellungspasswort hinzufügen\n11. Metadaten aktualisieren\n12. Freien Speicher bereinigen");
        core::console::separator(); core::console::line(L"0.  Zurück",core::console::highlight); core::console::line();
        const auto c=core::console::prompt(L"Select: ");if(c==L"0")return;
        if(c==L"1")manage({L"-status"},log);else if(c==L"2")selectDrive();
        else if(c==L"3")mutate(L"ENCRYPT",{L"-on",selectedDrive,L"-recoverypassword",L"-usedspaceonly"},log,true);
        else if(c==L"4")mutate(L"DECRYPT",{L"-off",selectedDrive},log);else if(c==L"5")mutate(L"PAUSE",{L"-pause",selectedDrive},log);
        else if(c==L"6")mutate(L"RESUME",{L"-resume",selectedDrive},log);else if(c==L"7")mutate(L"ENABLE-AUTOUNLOCK",{L"-autounlock",L"-enable",selectedDrive},log);
        else if(c==L"8")mutate(L"DISABLE-AUTOUNLOCK",{L"-autounlock",L"-disable",selectedDrive},log);else if(c==L"9")exportRecovery(log);
        else if(c==L"10")mutate(L"ADD-RECOVERY",{L"-protectors",L"-add",selectedDrive,L"-recoverypassword"},log,true);
        else if(c==L"11")mutate(L"UPGRADE",{L"-upgrade",selectedDrive},log);else if(c==L"12")mutate(L"WIPE-FREE-SPACE",{L"-wipefreespace",selectedDrive},log);
        if(c!=L"2")core::console::pause();
    }
}
}
