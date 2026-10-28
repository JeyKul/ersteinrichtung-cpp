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
void header() {
    const auto s=core::querySystemStatus();
    line(L"ERSTEINRICHTUNG - Native C++",core::console::accent);
    std::wcout<<L"PC: "<<s.computerName<<L" | RAM: "<<std::fixed<<std::setprecision(1)<<s.ramGiB<<L" GiB | SecureBoot: "
              <<(s.secureBoot?L"ON":L"OFF/unknown")<<L" | BitLocker C: "<<(s.bitLocker?L"ON":L"OFF/unknown")<<L" | Choco: "<<(s.chocolatey?L"yes":L"no")<<L"\n\n";
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
        core::console::clear(); header();
        line(L"  1. [BATCH] Private customer (rename)\n  2. [BATCH] Business customer (rename)\n  3. [BATCH] Private customer (no rename)\n  4. [BATCH] Business customer (no rename)\n\n  5. Rename computer\n  6. Install/update Chocolatey\n  7. Install Supremo (private)\n  8. Install Supremo + TeamViewer (business)\n  9. Install default programs\n 10. Windows Update\n 11. Tools\n 12. Extras\n 13. BitLocker menu\n 19. Sysprep OOBE\n 20. WhyNotWin11\n 21. Secure Boot / BitLocker check\n 22. Apply privacy settings\n\n  0. Exit");
        const auto c=core::console::prompt(L"Select an option: "); if(c==L"0"||c==L"q"||c==L"Q"){logger.write(L"Session finished");return;}
        if(c==L"1")runBatch(a,true,false); else if(c==L"2")runBatch(a,true,true); else if(c==L"3")runBatch(a,false,false); else if(c==L"4")runBatch(a,false,true);
        else if(c==L"5")a.renameComputer(); else if(c==L"6")a.ensureChocolatey(); else if(c==L"7")a.installSupremo(); else if(c==L"8")a.installBusinessRemote();
        else if(c==L"9")a.installDefaultPrograms(); else if(c==L"10")a.installWindowsUpdates(); else if(c==L"11")a.showTools(); else if(c==L"12")a.showExtras(); else if(c==L"13")bitlocker::showMenu(logger);
        else if(c==L"19")a.runSysprep(); else if(c==L"20")a.runWhyNotWin11(); else if(c==L"21")a.showSecurityStatus(); else if(c==L"22")a.applyPrivacy();
        if(c!=L"11"&&c!=L"12"&&c!=L"13") core::console::pause();
    }
}
}
