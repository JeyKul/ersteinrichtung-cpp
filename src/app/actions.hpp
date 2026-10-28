#pragma once
#include "../core/logger.hpp"
#include <string>
#include <vector>
namespace app {class Actions {public:explicit Actions(core::Logger& l):logger_(l){}bool renameComputer();bool ensureChocolatey();bool installSupremo(bool=false);bool installBusinessRemote();bool installDefaultPrograms();bool installWindowsUpdates();bool applyPrivacy();bool runSysprep();bool runWhyNotWin11();bool showSecurityStatus();bool setPreventSleep(bool);bool runChrisTitus();void showSystemInformation();void showTools();void showExtras();void runDiagnostics();private:core::Logger& logger_;bool runLogged(const std::wstring&,const std::wstring&,const std::vector<std::wstring>&,bool=false);bool downloadAndLaunch(const std::wstring&,const std::wstring&,const std::wstring&,const std::vector<std::wstring>& args = {}, bool wait=true);bool exportDrivers();bool installDrivers();bool enableLtscUpgradePath();};}
