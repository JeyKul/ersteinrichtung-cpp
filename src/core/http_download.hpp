#pragma once

#include <cstdint>
#include <string>

namespace core {

// Result returned by downloadFile.
struct DownloadResult {
    bool success{};
    std::wstring error;          // Human-readable error message (empty on success).
    std::uint64_t bytesWritten{}; // Bytes written to the output file.
};

// Downloads a file from |url| to |destinationPath| using WinHTTP.
// Returns a DownloadResult indicating success or failure.
// On failure, |error| contains a description (e.g. HTTP status code).
DownloadResult downloadFile(
    const std::wstring& url,
    const std::wstring& destinationPath);

}  // namespace core
