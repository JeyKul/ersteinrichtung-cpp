#include "menus.hpp"
#include "../bitlocker/bitlocker_menu.hpp"
#include "../core/console.hpp"
#include "../core/download.hpp"
#include "../core/system.hpp"
#include <iomanip>
#include <iostream>

namespace app {
namespace {
using core::console::line;
void statusField(const std::wstring& label, bool known, bool enabled) {
    const std::wstring value = !known ? L"Unknown" : enabled ? L"Enabled" : L"Disabled";
    const WORD color = !known ? core::console::warning : enabled ? core::console::good : core::console::error;
    core::console::write(label + L": ", color);
    core::console::write(value, color);
}
void header() {
    const auto s = core::querySystemStatus();
    line(L"ERSTEINRICHTUNG", core::console::title);
    line();
    statusField(L"SecureBoot", s.secureBootKnown, s.secureBoot);
    core::console::write(L" | ");
    statusField(L"BitLocker", s.bitLockerKnown, s.bitLocker);
    core::console::write(L" | ");
    statusField(L"Choco", true, s.chocolatey);
    core::console::write(L" | PC Name: " + s.computerName + L"\n\n");
    core::console::write(L"CPU: " + s.cpu + L" | RAM: ");
    std::wcout << std::fixed << std::setprecision(1) << s.ramGiB << L" GB\n\n";
}
void menuBody() {
    core::console::separator(L"Ersteinrichtung");
    line(L"1.  [BATCH] Privatkunde (Führt 5, 6, 7, 9 und 10 aus)");
    line(L"2.  [BATCH] Geschäftskunde (Führt 5, 6, 8, 9, 10 und 22 aus)");
    core::console::separator(L"Kein Umbenennen");
    line(L"3.  [BATCH] Privatkunde (Führt 6, 7, 9 und 10 aus)");
    line(L"4.  [BATCH] Geschäftskunde (Führt 6, 8, 9, 10 und 22 aus)");
    core::console::separator();
    line();
    line(L"5.  Rechner umbenennen");
    line(L"6.  Choco installieren/updaten");
    line(L"7.  Supremo für Privatkunden installieren");
    line(L"8.  TeamViewer und Supremo für Geschäftskunden installieren");
    line(L"9.  Installiere Standardprogramme");
    line(L"10. Nach Updates suchen (Windows 11)");
    line(L"19. Sysprep Setup OOBE");
    line(L"20. WhyNotWin11 -> logs");
    line(L"21. Secureboot and Bitlocker test");
    line(L"22. Disable Windows ads and personalized settings");
    core::console::separator(L"Weitere Menüs");
    line(L"11. Tools");
    line(L"12. Extras");
    line(L"13. Bitlocker Menu");
    core::console::separator();
    line();
    line(L"0. Beenden", core::console::highlight);
    line();
}
bool runBatch(Actions& a,bool rename,bool business) {
    line(L"The batch will run rename (optional), Chocolatey, remote support, default apps, Windows Update, and privacy tweaks for business.",core::console::warning);
    if(!core::console::confirmExact(business?L"RUN BUSINESS SETUP":L"RUN PRIVATE SETUP")) return false;
    a.setPreventSleep(true); bool ok=true;
    if(rename) ok=a.renameComputer()&&ok;
    ok=a.ensureChocolatey()&&ok;
    ok=(business?a.installBusinessRemote():a.installSupremo())&&ok;
    ok=a.installDefaultPrograms()&&ok;
    ok=a.installWindowsUpdates()&&ok;
    if(business) ok=a.applyPrivacy()&&ok;
    a.setPreventSleep(false);
    line(ok?L"Batch finished.":L"Batch finished with errors. Check the log.",ok?core::console::good:core::console::warning);
    return ok;
}
}
void runMainMenu(Actions& a,core::Logger& logger) {
    if(!core::hasInternet()){line(L"Warning: no Internet connection detected. Download/install steps may fail.",core::console::warning); Sleep(2500);}
    for(;;){
        core::console::clear(); header(); menuBody();
        const auto c=core::console::prompt(L"Select an option: "); if(c==L"0"||c==L"q"||c==L"Q"){logger.write(L"Session finished");return;}
        if(c==L"1")runBatch(a,true,false); else if(c==L"2")runBatch(a,true,true); else if(c==L"3")runBatch(a,false,false); else if(c==L"4")runBatch(a,false,true);
        else if(c==L"5")a.renameComputer(); else if(c==L"6")a.ensureChocolatey(); else if(c==L"7")a.installSupremo(); else if(c==L"8")a.installBusinessRemote();
        else if(c==L"9")a.installDefaultPrograms(); else if(c==L"10")a.installWindowsUpdates(); else if(c==L"11")a.showTools(); else if(c==L"12")a.showExtras(); else if(c==L"13")bitlocker::showMenu(logger);
        else if(c==L"19")a.runSysprep(); else if(c==L"20")a.runWhyNotWin11(); else if(c==L"21")a.showSecurityStatus(); else if(c==L"22")a.applyPrivacy();
        if(c!=L"11"&&c!=L"12"&&c!=L"13") core::console::pause();
    }
}
}
