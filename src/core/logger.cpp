#include "logger.hpp"
#include <windows.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace core {
namespace {
std::string utf8(const std::wstring& value) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    if (size) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::filesystem::path executableDirectory() {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return std::filesystem::current_path();
        if (length < buffer.size() - 1) {
            return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring machineName() {
    wchar_t value[MAX_COMPUTERNAME_LENGTH + 1]{};
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    return GetComputerNameW(value, &size) ? std::wstring(value, size) : L"Unknown-PC";
}

std::wstring stamp(const wchar_t* format) {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
    localtime_s(&local, &now);
    std::wostringstream out;
    out << std::put_time(&local, format);
    return out.str();
}
}

Logger::Logger() {
    root_ = executableDirectory() / L"logs";
    std::error_code ec;
    std::filesystem::create_directories(root_, ec);
    file_ = root_ / (machineName() + L"_" + stamp(L"%Y-%m-%d_%H-%M-%S") + L".log");
    write(L"Session started");
}

void Logger::write(const std::wstring& message) const {
    std::ofstream stream(file_, std::ios::binary | std::ios::app);
    stream << utf8(stamp(L"%Y-%m-%d %H:%M:%S") + L" [Device: " + machineName() + L"] - " + message + L"\r\n");
}
}
