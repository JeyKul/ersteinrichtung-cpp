#pragma once
#include "../core/logger.hpp"
#include <string>
#include <vector>

namespace app {
class Actions {
public:
    explicit Actions(core::Logger& logger) : logger_(logger) {}
    bool renameComputer();
    bool ensureChocolatey();
    bool installSupremo(bool businessShortcut = false);
    bool installBusinessRemote();
    bool installDefaultPrograms();
    bool installWindowsUpdates();
    bool applyPrivacy();
    bool runSysprep();
    bool runWhyNotWin11();
    bool showSecurityStatus();
    void showTools();
    void showExtras();
    bool setPreventSleep(bool enabled);
private:
    core::Logger& logger_;
    bool runLogged(const std::wstring& label, const std::wstring& exe,
                   const std::vector<std::wstring>& args, bool visible = false);
    bool downloadAndLaunch(const std::wstring& label, const std::wstring& url,
                           const std::wstring& fileName, const std::vector<std::wstring>& args = {}, bool wait = true);
    bool exportDrivers();
    bool installDrivers();
    bool enableLtscUpgradePath();
};
}
