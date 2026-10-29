#include "../actions.hpp"

#include "../../core/console.hpp"
#include "../../core/download.hpp"
#include "../../core/system.hpp"

#include <windows.h>
#include <shlobj.h>

#include <filesystem>

namespace app {
namespace {

std::filesystem::path publicDesktop() {
    wchar_t value[MAX_PATH]{};
    if (SHGetFolderPathW(
            nullptr,
            CSIDL_COMMON_DESKTOPDIRECTORY,
            nullptr,
            SHGFP_TYPE_CURRENT,
            value) == S_OK) {
        return value;
    }
    return L"C:\\Users\\Public\\Desktop";
}

} // namespace

bool Actions::installSupremo(bool businessShortcut) {
    const std::filesystem::path directory = L"C:\\CT-T";
    const auto target = directory / L"Supremo.exe";
    std::error_code error;
    std::filesystem::create_directories(directory, error);

    if (!std::filesystem::exists(target)) {
        const auto downloaded = core::downloadFile(
            L"https://www.ct-t.de/prog/Supremo.exe",
            target);
        if (!downloaded.ok) {
            core::console::line(downloaded.error, core::console::error);
            return false;
        }
    }

    if (!core::hasTrustedSignature(target)) {
        core::console::line(
            L"Supremo signature verification failed; shortcut was not created.",
            core::console::error);
        return false;
    }

    const auto shortcut = publicDesktop() /
        (businessShortcut
            ? L"CT-T Support Supremo.lnk"
            : L"CT-T Support.lnk");
    const std::wstring command =
        L"$s=(New-Object -ComObject WScript.Shell).CreateShortcut('" +
        shortcut.wstring() +
        L"');$s.TargetPath='" + target.wstring() +
        L"';$s.WorkingDirectory='C:\\CT-T';"
        L"$s.Description='CT-T Remote Support';$s.Save()";

    return runLogged(
        L"Create Supremo shortcut",
        core::windowsSystemExecutable(L"WindowsPowerShell\\v1.0\\powershell.exe"),
        {L"-NoProfile", L"-Command", command});
}

bool Actions::installBusinessRemote() {
    bool ok = installSupremo(true);
    const auto bundled = std::filesystem::path(core::executableDirectory()) /
                         L"assets\\TeamViewer_Host_Setup.exe";
    if (std::filesystem::exists(bundled)) {
        ok = runLogged(
            L"Install TeamViewer Host",
            bundled.wstring(),
            {L"/S"}) && ok;
    } else {
        core::console::line(
            L"TeamViewer installer not bundled; Supremo was installed. "
            L"Put it at assets\\TeamViewer_Host_Setup.exe to enable this step.",
            core::console::warning);
    }
    return ok;
}

} // namespace app
