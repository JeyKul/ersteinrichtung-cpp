#include "../actions.hpp"

#include "../../core/console.hpp"

#include <windows.h>

#include <cwchar>

namespace app {
namespace {

bool setString(
    HKEY root,
    const wchar_t* path,
    const wchar_t* name,
    const wchar_t* value) {
    HKEY key{};
    if (RegCreateKeyExW(
            root, path, 0, nullptr, 0, KEY_SET_VALUE,
            nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    const DWORD size = static_cast<DWORD>(
        (std::wcslen(value) + 1) * sizeof(wchar_t));
    const LONG result = RegSetValueExW(
        key,
        name,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(value),
        size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

} // namespace

bool Actions::enableLtscUpgradePath() {
    core::console::line(
        L"This changes Windows edition registry identifiers to Professional "
        L"as preparation for an in-place upgrade.",
        core::console::warning);
    if (!core::console::confirm(
            L"Change the Windows edition identifiers? This may affect servicing and upgrades.")) {
        return false;
    }

    const wchar_t* path =
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    const bool ok =
        setString(HKEY_LOCAL_MACHINE, path, L"EditionID", L"Professional") &&
        setString(HKEY_LOCAL_MACHINE, path, L"ProductName", L"Windows 10 Pro");
    logger_.write(ok
        ? L"LTSC edition identifiers changed"
        : L"LTSC edition key change failed");
    return ok;
}

} // namespace app
