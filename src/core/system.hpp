#pragma once
#include <string>
namespace core {struct SystemStatus{std::wstring computerName,cpu,manufacturer,model,windowsName,windowsVersion,windowsBuild,architecture;double ramGiB{},systemDriveTotalGiB{},systemDriveFreeGiB{};bool secureBoot{},secureBootKnown{},bitLocker{},bitLockerKnown{},chocolatey{};};SystemStatus querySystemStatus();bool isAdministrator();std::wstring windowsSystemExecutable(const std::wstring&);std::wstring executableDirectory();std::wstring systemModel();}
