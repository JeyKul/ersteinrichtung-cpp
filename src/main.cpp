#include <windows.h>
#include <shellapi.h>

#include "app/actions.hpp"
#include "app/menus.hpp"
#include "core/console.hpp"
#include "core/i18n.hpp"
#include "core/logger.hpp"
#include "core/system.hpp"

#include <exception>
#include <filesystem>
#include <string>
#include <string_view>

namespace {
std::wstring quoteArgument(const std::wstring& argument) {
    if (argument.empty()) return L"\"\"";
    if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) return argument;

    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        if (character == L'\"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(L'\"');
            backslashes = 0;
            continue;
        }
        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(character);
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'\"');
    return quoted;
}

bool hasArgument(int argc, wchar_t** argv, const std::wstring_view expected) {
    for (int index = 1; index < argc; ++index) {
        if (std::wstring_view(argv[index]) == expected) return true;
    }
    return false;
}

bool requestElevation(int argc, wchar_t** argv) {
    wchar_t executable[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, executable, MAX_PATH)) return false;

    std::wstring parameters;
    for (int index = 1; index < argc; ++index) {
        if (!parameters.empty()) parameters.push_back(L' ');
        parameters += quoteArgument(argv[index]);
    }
    if (!parameters.empty()) parameters.push_back(L' ');
    parameters += L"--elevation-requested";

    const auto directory = std::filesystem::path(executable).parent_path().wstring();
    const HINSTANCE result = ShellExecuteW(
        nullptr,
        L"runas",
        executable,
        parameters.c_str(),
        directory.c_str(),
        SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

void showFatal(const std::wstring& message) {
    core::console::line(message, core::console::error);
    MessageBoxW(nullptr, message.c_str(), L"Ersteinrichtung", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}
}

int wmain(int argc, wchar_t** argv) {
    core::console::initialize();
    core::i18n::initialize();

    if (!core::isAdministrator()) {
        if (!hasArgument(argc, argv, L"--elevation-requested") && requestElevation(argc, argv)) return 0;
        showFatal(core::i18n::tr("app.admin_required"));
        return 1;
    }

    try {
        core::Logger logger;
        if (argc > 1 && std::wstring_view(argv[1]) == L"--privacy-enforce") {
            return app::applyPrivacyEnforcement(logger) ? 0 : 3;
        }
        app::Actions actions(logger);
        app::runMainMenu(actions, logger);
    } catch (const std::exception&) {
        showFatal(core::i18n::tr("app.fatal"));
        return 2;
    } catch (...) {
        showFatal(core::i18n::tr("app.fatal"));
        return 2;
    }
    return 0;
}
