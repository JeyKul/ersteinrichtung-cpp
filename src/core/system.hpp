#pragma once
#include <string>

namespace core {
struct SystemStatus {
    std::wstring computerName;
    std::wstring cpu;
    double ramGiB{};
    bool secureBoot{};
    bool secureBootKnown{};
    bool bitLocker{};
    bool bitLockerKnown{};
    bool chocolatey{};
};
SystemStatus querySystemStatus();
bool isAdministrator();
std::wstring windowsSystemExecutable(const std::wstring& name);
std::wstring executableDirectory();
std::wstring systemModel();
}
