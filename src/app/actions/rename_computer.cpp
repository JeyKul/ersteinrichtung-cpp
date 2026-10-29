#include "../actions.hpp"

#include "../../core/console.hpp"

#include <windows.h>

#include <algorithm>
#include <cwctype>

namespace app {

bool Actions::renameComputer() {
    const auto name = core::console::prompt(
        L"New computer name (1-15 chars, Enter cancels): ");
    if (name.empty()) return true;

    const bool invalid =
        name.size() > MAX_COMPUTERNAME_LENGTH ||
        name.front() == L'-' ||
        name.back() == L'-' ||
        std::any_of(name.begin(), name.end(), [](wchar_t character) {
            return !(std::iswalnum(character) || character == L'-');
        });
    if (invalid) {
        core::console::line(L"Invalid NetBIOS computer name.", core::console::error);
        return false;
    }

    if (!core::console::confirm(
            L"Rename this computer to " + name +
            L"? A restart is required.")) {
        return false;
    }

    if (!SetComputerNameExW(ComputerNamePhysicalDnsHostname, name.c_str())) {
        core::console::line(
            L"Rename failed: " + std::to_wstring(GetLastError()),
            core::console::error);
        return false;
    }

    core::console::line(
        L"Computer renamed. A restart is required.",
        core::console::good);
    logger_.write(L"Computer renamed to " + name);
    return true;
}

} // namespace app
