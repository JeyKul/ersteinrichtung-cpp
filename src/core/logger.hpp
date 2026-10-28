#pragma once
#include <filesystem>
#include <string>

namespace core {
class Logger {
public:
    Logger();
    void write(const std::wstring& message) const;
    const std::filesystem::path& root() const { return root_; }
    const std::filesystem::path& file() const { return file_; }
private:
    std::filesystem::path root_;
    std::filesystem::path file_;
};
}
