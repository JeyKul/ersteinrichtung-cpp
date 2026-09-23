#pragma once
#include <string>
#include <vector>
namespace core {
struct Result { DWORD exitCode; std::wstring output; std::wstring error; };
Result run(const std::wstring& exe, const std::vector<std::wstring>& args);
}
