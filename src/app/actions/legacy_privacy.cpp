#include "../actions.hpp"

#include "../../core/console.hpp"
#include "../../core/system.hpp"

#include <windows.h>

namespace app {
namespace {

bool setDword(
    HKEY root,
    const wchar_t* path,
    const wchar_t* name,
    DWORD value) {
    HKEY key{};
    if (RegCreateKeyExW(
            root, path, 0, nullptr, 0, KEY_SET_VALUE,
            nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const LONG result = RegSetValueExW(
        key,
        name,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&value),
        sizeof(value));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

} // namespace

bool Actions::applyPrivacy() {
    core::console::line(
        L"Applies the legacy privacy set. Prefer enhanced privacy from the main menu.",
        core::console::warning);
    if (!core::console::confirm(
            L"Apply the legacy privacy settings? Some changes may not be reversible.")) {
        return false;
    }

    struct Entry {
        HKEY root;
        const wchar_t* path;
        const wchar_t* name;
        DWORD value;
    };
    const Entry entries[] = {
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo", L"Enabled", 0},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy", L"TailoredExperiencesWithDiagnosticDataEnabled", 0},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Speech_OneCore\\Settings\\OnlineSpeechPrivacy", L"HasAccepted", 0},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Input\\TIPC", L"Enabled", 0},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\InputPersonalization", L"RestrictImplicitInkCollection", 1},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\InputPersonalization", L"RestrictImplicitTextCollection", 1},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\InputPersonalization\\TrainedDataStore", L"HarvestContacts", 0},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Personalization\\Settings", L"AcceptedPrivacyPolicy", 0},
        {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\DataCollection", L"AllowTelemetry", 0},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_TrackProgs", 0},
        {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\System", L"PublishUserActivities", 0},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Siuf\\Rules", L"NumberOfSIUFInPeriod", 0},
    };

    bool ok = true;
    for (const auto& entry : entries) {
        ok = setDword(entry.root, entry.path, entry.name, entry.value) && ok;
    }

    const auto serviceControl = core::windowsSystemExecutable(L"sc.exe");
    ok = runLogged(
        L"Disable DiagTrack",
        serviceControl,
        {L"config", L"DiagTrack", L"start=", L"disabled"}) && ok;
    ok = runLogged(
        L"Disable Windows Error Reporting",
        serviceControl,
        {L"config", L"WerSvc", L"start=", L"disabled"}) && ok;

    core::console::line(
        ok ? L"Privacy settings applied."
           : L"Some privacy settings failed; see the log.",
        ok ? core::console::good : core::console::warning);
    logger_.write(ok
        ? L"Legacy privacy settings applied"
        : L"Legacy privacy settings partially failed");
    return ok;
}

} // namespace app
