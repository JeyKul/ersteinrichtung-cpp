#pragma once

#include "../core/logger.hpp"

#include <string>
#include <vector>

namespace app {

bool applyPrivacyEnforcement(core::Logger& logger);

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
    bool applyEnhancedPrivacy();
    bool runSysprep();
    bool runWhyNotWin11();
    bool showSecurityStatus();
    bool runChrisTitus();

    void showSystemInformation();
    void showTools();
    void showExtras();
    void showExtrasEnhanced();
    void runDiagnostics();

private:
    core::Logger& logger_;

    bool runLogged(
        const std::wstring& label,
        const std::wstring& executable,
        const std::vector<std::wstring>& arguments,
        bool visible = false);

    bool downloadAndLaunch(
        const std::wstring& label,
        const std::wstring& url,
        const std::wstring& fileName,
        const std::vector<std::wstring>& arguments = {},
        bool wait = true);

    bool exportDriversEnhanced();
    bool installDriversEnhanced();
    bool enableLtscUpgradePath();
};

} // namespace app
