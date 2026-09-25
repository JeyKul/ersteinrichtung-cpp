#include "process_runner.hpp"

#include <windows.h>

#include <string>
#include <vector>

namespace core {
namespace {

std::wstring quoteArgument(const std::wstring& value) {
    return L"\"" + value + L"\"";
}

std::wstring buildCommandLine(
    const std::wstring& executable,
    const std::vector<std::wstring>& arguments) {
    std::wstring commandLine = quoteArgument(executable);

    for (const std::wstring& argument : arguments) {
        commandLine += L" ";
        commandLine += argument;
    }

    return commandLine;
}

std::wstring readAllFromPipe(HANDLE pipe) {
    std::string bytes;
    char buffer[4096];
    DWORD bytesRead = 0;

    while (ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        bytes.append(buffer, bytesRead);
    }

    if (bytes.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(
        CP_OEMCP,
        0,
        bytes.data(),
        static_cast<int>(bytes.size()),
        nullptr,
        0);

    std::wstring output(static_cast<std::size_t>(length), L' ');
    MultiByteToWideChar(
        CP_OEMCP,
        0,
        bytes.data(),
        static_cast<int>(bytes.size()),
        output.data(),
        length);

    return output;
}

}  // namespace

ProcessResult runProcess(
    const std::wstring& executable,
    const std::vector<std::wstring>& arguments) {
    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;

    if (!CreatePipe(&readPipe, &writePipe, &securityAttributes, 0)) {
        return {GetLastError(), {}, L"CreatePipe failed."};
    }

    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    std::wstring commandLine = buildCommandLine(executable, arguments);
    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startupInfo.hStdOutput = writePipe;
    startupInfo.hStdError = writePipe;

    PROCESS_INFORMATION processInfo{};

    const BOOL started = CreateProcessW(
        executable.c_str(),
        mutableCommandLine.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo);

    CloseHandle(writePipe);

    if (!started) {
        const DWORD error = GetLastError();
        CloseHandle(readPipe);
        return {error, {}, L"CreateProcessW failed."};
    }

    const std::wstring output = readAllFromPipe(readPipe);
    CloseHandle(readPipe);

    WaitForSingleObject(processInfo.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    return {exitCode, output, {}};
}

}  // namespace core
