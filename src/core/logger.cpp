#include "logger.hpp"
#include <windows.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace core {
namespace {
std::string utf8(const std::wstring& value) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    if (size) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}
std::wstring machineName() {
    wchar_t value[MAX_COMPUTERNAME_LENGTH + 1]{}; DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    return GetComputerNameW(value, &size) ? std::wstring(value, size) : L"Unknown-PC";
}
std::wstring stamp(const wchar_t* format) {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{}; localtime_s(&local, &now); std::wostringstream out; out << std::put_time(&local, format); return out.str();
}
}
Logger::Logger() {
    wchar_t data[MAX_PATH]{};
    if (!GetEnvironmentVariableW(L"ProgramData", data, MAX_PATH)) wcscpy_s(data, L"C:\\ProgramData");
    root_ = std::filesystem::path(data) / L"Ersteinrichtung" / L"Logs";
    const auto directory = root_ / machineName();
    std::error_code ec; std::filesystem::create_directories(directory, ec);
    file_ = directory / (stamp(L"%Y-%m-%d_%H-%M-%S") + L".log");
    write(L"Session started");
}
void Logger::write(const std::wstring& message) const {
    std::ofstream stream(file_, std::ios::binary | std::ios::app);
    stream << utf8(stamp(L"%Y-%m-%d %H:%M:%S") + L" [Device: " + machineName() + L"] - " + message + L"\r\n");
}
}
