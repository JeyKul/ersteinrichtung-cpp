#include "../actions.hpp"

#include "../../core/console.hpp"
#include "../../core/download.hpp"
#include "../../core/process_runner.hpp"
#include "../../core/system.hpp"

#include <windows.h>

#include <filesystem>

namespace app {
namespace {

std::filesystem::path temporaryFile(const std::wstring& fileName) {
    wchar_t directory[MAX_PATH]{};
    if (!GetTempPathW(MAX_PATH, directory)) return fileName;
    return std::filesystem::path(directory) / fileName;
}

} // namespace

bool Actions::runLogged(
    const std::wstring& label,
    const std::wstring& executable,
    const std::vector<std::wstring>& arguments,
    bool visible) {
    logger_.write(L"START " + label);
    core::console::line(L"\n==> " + label, core::console::accent);

    core::ProcessOptions options;
    options.captureOutput = !visible;
    options.showWindow = visible;
    const auto result = core::runProcess(executable, arguments, options);

    if (!result.output.empty()) core::console::line(result.output);
    if (!result.error.empty()) {
        core::console::line(result.error, core::console::error);
    }

    const bool ok = result.started && result.exitCode == 0;
    core::console::line(
        ok ? L"Done." : L"Failed (exit code " +
             std::to_wstring(result.exitCode) + L").",
        ok ? core::console::good : core::console::error);
    logger_.write((ok ? L"OK " : L"FAIL ") + label +
                  L" exit=" + std::to_wstring(result.exitCode));
    return ok;
}

bool Actions::downloadAndLaunch(
    const std::wstring& label,
    const std::wstring& url,
    const std::wstring& fileName,
    const std::vector<std::wstring>& arguments,
    bool wait) {
    const auto destination = temporaryFile(fileName);
    if (!std::filesystem::exists(destination)) {
        core::console::line(L"Downloading " + label + L"...", core::console::accent);
        const auto downloaded = core::downloadFile(url, destination);
        if (!downloaded.ok) {
            core::console::line(
                L"Download failed: " + downloaded.error,
                core::console::error);
            logger_.write(L"Download failed: " + label);
            return false;
        }
    }

    const auto extension = destination.extension().wstring();
    const bool skipSignature = fileName == L"WhyNotWin11.exe";
    if (!skipSignature &&
        (extension == L".exe" || extension == L".msi") &&
        !core::hasTrustedSignature(destination)) {
        core::console::line(
            L"Warning: Windows could not verify the file's Authenticode signature.",
            core::console::warning);
        if (!core::console::yesNo(L"Launch it anyway?")) return false;
    } else if (skipSignature) {
        core::console::line(
            L"WhyNotWin11 signature verification is skipped by policy.",
            core::console::warning);
    }

    core::ProcessOptions options;
    options.captureOutput = false;
    options.showWindow = true;
    options.wait = wait;

    std::wstring executable = destination.wstring();
    std::vector<std::wstring> launchArguments = arguments;
    if (extension == L".msi") {
        executable = core::windowsSystemExecutable(L"msiexec.exe");
        launchArguments.insert(launchArguments.begin(), destination.wstring());
        launchArguments.insert(launchArguments.begin(), L"/i");
    }

    const auto result = core::runProcess(executable, launchArguments, options);
    logger_.write((result.started ? L"Launched " : L"Could not launch ") + label);
    return result.started && (!wait || result.exitCode == 0 || result.exitCode == 3010);
}

} // namespace app
