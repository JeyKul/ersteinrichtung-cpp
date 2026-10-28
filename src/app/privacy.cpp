#include "actions.hpp"
#include "../core/console.hpp"
#include "../core/process_runner.hpp"
#include "../core/system.hpp"
#include <filesystem>
namespace app {
bool Actions::applyEnhancedPrivacy() {
    core::console::line(L"Applies machine policies, all loaded users and the default profile; disables telemetry tasks/services; and removes selected provisioned AppX packages.",core::console::warning);
    core::console::line(L"A SYSTEM startup task reapplies key controls if Windows restores them. Feature upgrades and managed policies can still override settings.",core::console::warning);
    if(!core::console::confirm(L"Apply enhanced computer-wide privacy and remove New Outlook, Mail/Calendar, LinkedIn, Solitaire, WhatsApp and Clipchamp?"))return false;
    const auto script=std::filesystem::path(core::executableDirectory())/L"assets\\privacy.ps1";
    if(!std::filesystem::exists(script)){core::console::line(L"Missing assets\\privacy.ps1",core::console::error);return false;}
    core::ProcessOptions options;options.onOutput=[this](const std::wstring& text){if(!text.empty()){core::console::line(L"  "+text);logger_.write(L"PRIVACY "+text);}};
    const auto result=core::runProcess(core::windowsSystemExecutable(L"WindowsPowerShell\\v1.0\\powershell.exe"),{L"-NoProfile",L"-ExecutionPolicy",L"Bypass",L"-File",script.wstring()},options);
    const bool ok=result.started&&result.exitCode==0;
    core::console::line(ok?L"Enhanced privacy applied. Restart Windows to activate all policies.":L"Privacy completed with errors; inspect the session log.",ok?core::console::good:core::console::warning);
    logger_.write(L"Enhanced privacy exit="+std::to_wstring(result.exitCode));return ok;
}
}
