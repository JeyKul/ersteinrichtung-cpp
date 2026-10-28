#include "system.hpp"
#include "process_runner.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>

namespace core {
namespace {
bool readDword(HKEY root, const wchar_t* path, const wchar_t* name, DWORD& value) {
    DWORD size = sizeof(value); return RegGetValueW(root, path, name, RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS;
}
std::wstring readString(HKEY root, const wchar_t* path, const wchar_t* name) {
    DWORD size{}; if (RegGetValueW(root, path, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS) return {};
    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(root, path, name, RRF_RT_REG_SZ, nullptr, value.data(), &size) != ERROR_SUCCESS) return {};
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    return value;
}
}
bool isAdministrator() {
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY; PSID group{}; BOOL member = FALSE;
    if (AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0,0,0,0,0,0, &group)) { CheckTokenMembership(nullptr, group, &member); FreeSid(group); }
    return member != FALSE;
}
std::wstring windowsSystemExecutable(const std::wstring& name) {
    std::array<wchar_t, MAX_PATH> directory{}; const UINT length = GetSystemDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
    return length && length < directory.size() ? (std::filesystem::path(directory.data()) / name).wstring() : std::wstring{};
}
std::wstring executableDirectory() {
    std::array<wchar_t, 32768> path{}; const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    return length ? std::filesystem::path(std::wstring(path.data(), length)).parent_path().wstring() : L".";
}
std::wstring systemModel() {
    auto value = readString(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemProductName");
    return value.empty() ? L"Unknown_Model" : value;
}
SystemStatus querySystemStatus() {
    SystemStatus status;
    wchar_t name[MAX_COMPUTERNAME_LENGTH + 1]{}; DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(name, &size)) status.computerName.assign(name, size);
    status.cpu = readString(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString");
    MEMORYSTATUSEX memory{}; memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) status.ramGiB = static_cast<double>(memory.ullTotalPhys) / 1073741824.0;
    DWORD secure{}; status.secureBootKnown = readDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State", L"UEFISecureBootEnabled", secure); status.secureBoot = secure == 1;
    wchar_t programData[MAX_PATH]{}; GetEnvironmentVariableW(L"ProgramData", programData, MAX_PATH);
    status.chocolatey = std::filesystem::exists(std::filesystem::path(programData) / L"chocolatey\\bin\\choco.exe");
    const auto bde = windowsSystemExecutable(L"manage-bde.exe");
    if (!bde.empty()) {
        const auto result = runProcess(bde, {L"-status", L"C:"});
        std::wstring text = result.output; std::transform(text.begin(), text.end(), text.begin(), std::towlower);
        status.bitLockerKnown = result.exitCode == 0;
        status.bitLocker = text.find(L"protection on") != std::wstring::npos || text.find(L"schutz ist aktiviert") != std::wstring::npos || text.find(L"schutz aktiviert") != std::wstring::npos;
    }
    return status;
}
}
