#pragma once
#include <filesystem>
#include <string>

namespace core {
struct DownloadResult { bool ok{}; unsigned long status{}; std::wstring error; };
DownloadResult downloadFile(const std::wstring& url, const std::filesystem::path& destination);
bool hasInternet();
bool hasTrustedSignature(const std::filesystem::path& file);
}
