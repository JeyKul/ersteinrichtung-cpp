#include "../actions.hpp"

namespace app {

bool Actions::runWhyNotWin11() {
    return downloadAndLaunch(
        L"WhyNotWin11",
        L"https://github.com/rcmaehl/WhyNotWin11/releases/download/2.7.0/WhyNotWin11.exe",
        L"WhyNotWin11.exe",
        {},
        false);
}

} // namespace app
