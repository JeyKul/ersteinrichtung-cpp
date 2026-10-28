#include "process_runner.hpp"
#include <windows.h>
#include <shellapi.h>
#include <array>
#include <string>
#include <vector>

namespace core {
namespace {
std::wstring quote(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) return arg;
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (const wchar_t ch : arg) {
        if (ch == L'\\') { ++slashes; continue; }
        if (ch == L'\"') {
            result.append(slashes * 2 + 1, L'\\'); result += L'\"'; slashes = 0;
        } else {
            result.append(slashes, L'\\'); slashes = 0; result += ch;
        }
    }
    result.append(slashes * 2, L'\\');
    return result + L"\"";
}
std::wstring makeCommand(const std::wstring& exe, const std::vector<std::wstring>& args) {
    std::wstring command = quote(exe);
    for (const auto& arg : args) command += L" " + quote(arg);
    return command;
}
std::wstring decode(const std::string& bytes) {
    if (bytes.empty()) return {};
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    UINT page = CP_UTF8;
    if (!length) { page = CP_OEMCP; length = MultiByteToWideChar(page, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0); }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (length) MultiByteToWideChar(page, 0, bytes.data(), static_cast<int>(bytes.size()), result.data(), length);
    return result;
}
}
std::wstring findExecutable(const std::wstring& name) {
    std::array<wchar_t, 32768> path{};
    const DWORD length = SearchPathW(nullptr, name.c_str(), nullptr, static_cast<DWORD>(path.size()), path.data(), nullptr);
    return length && length < path.size() ? std::wstring(path.data(), length) : std::wstring{};
}
ProcessResult runProcess(const std::wstring& executable, const std::vector<std::wstring>& args, const ProcessOptions& options) {
    ProcessResult result;
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (options.captureOutput) {
        if (!CreatePipe(&readPipe, &writePipe, &security, 0)) { result.error = L"CreatePipe failed: " + std::to_wstring(GetLastError()); return result; }
        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    }
    std::wstring command = makeCommand(executable, args);
    std::vector<wchar_t> mutableCommand(command.begin(), command.end()); mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    if (options.captureOutput) {
        startup.dwFlags |= STARTF_USESTDHANDLES;
        startup.hStdOutput = writePipe; startup.hStdError = writePipe; startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    if (!options.showWindow) { startup.dwFlags |= STARTF_USESHOWWINDOW; startup.wShowWindow = SW_HIDE; }
    PROCESS_INFORMATION process{};
    const DWORD flags = options.showWindow ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;
    const BOOL ok = CreateProcessW(executable.c_str(), mutableCommand.data(), nullptr, nullptr,
                                   options.captureOutput, flags, nullptr,
                                   options.workingDirectory.empty() ? nullptr : options.workingDirectory.c_str(),
                                   &startup, &process);
    if (writePipe) CloseHandle(writePipe);
    if (!ok) {
        if (readPipe) CloseHandle(readPipe);
        result.exitCode = GetLastError(); result.error = L"CreateProcessW failed: " + std::to_wstring(result.exitCode); return result;
    }
    result.started = true;
    if (!options.wait) {
        if (readPipe) CloseHandle(readPipe);
        CloseHandle(process.hThread); CloseHandle(process.hProcess); result.exitCode = 0; return result;
    }
    std::string bytes;
    if (readPipe) {
        char buffer[8192]; DWORD count{};
        while (ReadFile(readPipe, buffer, sizeof(buffer), &count, nullptr) && count) bytes.append(buffer, count);
        CloseHandle(readPipe);
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &result.exitCode);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    result.output = decode(bytes);
    return result;
}
bool shellOpen(const std::wstring& target, const std::wstring& parameters) {
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", target.c_str(),
        parameters.empty() ? nullptr : parameters.c_str(), nullptr, SW_SHOWNORMAL)) > 32;
}
}
