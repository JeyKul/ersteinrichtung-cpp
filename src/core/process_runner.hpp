#pragma once

#include <string>
#include <vector>

namespace core {

struct ProcessResult {
    unsigned long exitCode{};
    std::wstring output;
    std::wstring error;
};

ProcessResult runProcess(
    const std::wstring& executable,
    const std::vector<std::wstring>& arguments);

}  // namespace core
