#include "actions.hpp"
#include "../core/console.hpp"
#include "../core/process_runner.hpp"
#include "../core/system.hpp"
#include <cwctype>
#include <filesystem>
namespace app {
namespace {
using core::console::line;
std::wstring cleanModel(std::wstring value){for(auto& c:value)if(!(std::iswalnum(c)||c==L'-'||c==L'_'))c=L'_';return value;}
std::filesystem::path resolveDriverPath(const std::wstring& input){
    if(input.size()==1&&std::iswalpha(input.front()))return std::filesystem::path(std::wstring(1,static_cast<wchar_t>(std::towupper(input.front())))+L":\\extracted_drivers")/cleanModel(core::systemModel());
    if(input.size()==2&&std::iswalpha(input.front())&&input[1]==L':')return std::filesystem::path(std::wstring(1,static_cast<wchar_t>(std::towupper(input.front())))+L":\\extracted_drivers")/cleanModel(core::systemModel());
    return std::filesystem::path(input);
}
}
bool Actions::exportDriversEnhanced(){
    const auto input=core::console::prompt(L"Destination drive letter or absolute export directory: ");if(input.empty())return false;
    const auto destination=resolveDriverPath(input);if(!destination.is_absolute()){line(L"Enter a drive letter or absolute path.",core::console::error);return false;}
    std::error_code ec;std::filesystem::create_directories(destination,ec);if(ec){line(L"Cannot create: "+destination.wstring(),core::console::error);return false;}
    if(!core::console::confirm(L"Export every installed third-party driver to "+destination.wstring()+L"? Existing files can be overwritten."))return false;
    core::ProcessOptions options;options.onOutput=[this](const std::wstring& text){if(!text.empty()){line(L"  "+text);logger_.write(L"DRIVER EXPORT "+text);}};
    const auto result=core::runProcess(core::windowsSystemExecutable(L"dism.exe"),{L"/online",L"/export-driver",L"/destination:"+destination.wstring()},options);
    const bool ok=result.started&&result.exitCode==0;line(ok?L"Driver export completed.":L"Driver export failed; check the log.",ok?core::console::good:core::console::error);return ok;
}
bool Actions::installDriversEnhanced(){
    const auto input=core::console::prompt(L"Drive letter or absolute directory containing drivers: ");if(input.empty())return false;
    const auto source=resolveDriverPath(input);if(!source.is_absolute()){line(L"Enter a drive letter or absolute path.",core::console::error);return false;}
    if(!std::filesystem::exists(source)){line(L"Driver directory not found: "+source.wstring(),core::console::error);return false;}
    std::error_code ec;std::size_t count{};for(const auto& e:std::filesystem::recursive_directory_iterator(source,ec))if(e.is_regular_file()&&_wcsicmp(e.path().extension().wstring().c_str(),L".inf")==0)++count;
    if(!count){line(L"No INF packages found under: "+source.wstring(),core::console::error);return false;}
    line(L"Found "+std::to_wstring(count)+L" INF driver packages.",core::console::accent);
    if(!core::console::confirm(L"Install these drivers recursively? Installed devices and drivers can change."))return false;
    core::ProcessOptions options;options.onOutput=[this](const std::wstring& text){if(!text.empty()){line(L"  "+text);logger_.write(L"DRIVER INSTALL "+text);}};
    const auto result=core::runProcess(core::windowsSystemExecutable(L"pnputil.exe"),{L"/add-driver",(source/L"*.inf").wstring(),L"/subdirs",L"/install"},options);
    const bool ok=result.started&&result.exitCode==0;line(ok?L"Driver installation completed.":L"Driver installation failed; check the log.",ok?core::console::good:core::console::error);return ok;
}
void Actions::showExtrasEnhanced(){
    for(;;){core::console::clear();core::console::separator(L"EXTRAS");line(L"1.  Windows Store installieren (LTSC)\n2.  App Installer / WinGet Store-Seite öffnen\n3.  LTSC -> Pro Upgrade-Pfad aktivieren\n10. Treiber exportieren (Laufwerk oder Pfad)\n11. Exportierte Treiber installieren (Laufwerk oder Pfad)");core::console::separator();line(L"0.  Zurück",core::console::highlight);line();
        const auto c=core::console::prompt(L"Select: ");if(c==L"0")return;
        if(c==L"1")runLogged(L"Install Windows Store",core::windowsSystemExecutable(L"wsreset.exe"),{L"-i"},true);
        else if(c==L"2")core::shellOpen(L"ms-windows-store://pdp/?ProductId=9NBLGGH4NNS1");
        else if(c==L"3")enableLtscUpgradePath();else if(c==L"10")exportDriversEnhanced();else if(c==L"11")installDriversEnhanced();core::console::pause();}
}
}
