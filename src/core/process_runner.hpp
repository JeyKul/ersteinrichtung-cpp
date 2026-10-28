#pragma once
#include <string>
#include <vector>

namespace core {
struct ProcessResult {
    unsigned long exitCode{1};
    std::wstring output;
    std::wstring error;
    bool started{false};
};
struct ProcessOptions {
    bool captureOutput{true};
    bool showWindow{false};
    bool wait{true};
    std::wstring workingDirectory;
};
ProcessResult runProcess(const std::wstring& executable,
                         const std::vector<std::wstring>& arguments = {},
                         const ProcessOptions& options = {});
std::wstring findExecutable(const std::wstring& name);
bool shellOpen(const std::wstring& target, const std::wstring& parameters = {});
}
