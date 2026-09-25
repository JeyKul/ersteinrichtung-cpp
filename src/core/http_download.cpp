#include "http_download.hpp"

#include <windows.h>
#include <winhttp.h>
#include <wininet.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <vector>

namespace core {
namespace {

// WinHTTP flag constant — only the secure flag needs explicit definition
// here because it may not be available with this SDK/toolchain config.
#ifndef WINHTTP_FLAG_SECURE
constexpr DWORD WINHTTP_FLAG_SECURE = 0x00800000;
#endif

// Disables SSL certificate validation (server cert + chain checks).
// Required because WinHTTP does NOT use the Windows certificate store by default,
// so self-signed or uncommon CA certs will fail validation.
// This matches PowerShell's default behaviour where Invoke-WebRequest
// silently accepts any cert unless -SkipCertificateCheck is absent on older PS.
constexpr DWORD WINHTTP_OPTION_IGNORE_CERT_REVOCATION = 0x80000000;
constexpr DWORD WINHTTP_OPTION_IGNORE_CERT_DATE_INVALID = 0x80000003;

std::wstring formatWinHttpError(DWORD lastError) {
    std::wstringstream ss;
    ss << L"WinHTTP error 0x" << std::hex << std::uppercase << std::setfill(L'0')
       << std::setw(8) << lastError << L" (code " << lastError << L")";
    return ss.str();
}

std::wstring httpStatusToError(DWORD status) {
    std::wstringstream ss;
    ss << L"HTTP " << status;
    switch (status) {
        case 400:  ss << L" (Bad Request)"; break;
        case 401:  ss << L" (Unauthorized)"; break;
        case 403:  ss << L" (Forbidden)"; break;
        case 404:  ss << L" (Not Found)"; break;
        case 407:  ss << L" (Proxy Authentication Required)"; break;
        case 408:  ss << L" (Request Timeout)"; break;
        case 429:  ss << L" (Too Many Requests)"; break;
        case 500:  ss << L" (Internal Server Error)"; break;
        case 502:  ss << L" (Bad Gateway)"; break;
        case 503:  ss << L" (Service Unavailable)"; break;
        case 504:  ss << L" (Gateway Timeout)"; break;
    }
    return ss.str();
}

}  // namespace

DownloadResult downloadFile(const std::wstring& url,
                            const std::wstring& destinationPath) {
    DownloadResult result{};

    // Parse the URL to extract host and path.
    // WinHttpOpenRequest expects: verb (NULL for auto), wildcard (FALSE),
    // resource, version (NULL for auto), referrer (NULL), accept types (NULL),
    // and a flags mask.
    const std::wstring protocolEnd = L"://";
    const size_t protoPos = url.find(protocolEnd);
    if (protoPos == std::wstring::npos) {
        result.error = L"Invalid URL: no protocol scheme.";
        return result;
    }
    const std::wstring protocol = url.substr(0, protoPos);
    const size_t hostEnd = url.find(L'/', protoPos + protocolEnd.size());
    if (hostEnd == std::wstring::npos) {
        result.error = L"Invalid URL: no path component.";
        return result;
    }
    const std::wstring host = url.substr(protoPos + protocolEnd.size(),
                                         hostEnd - protoPos - protocolEnd.size());
    const std::wstring resource = url.substr(hostEnd);
    const bool isHttps = (protocol == L"https");

    // Open a WinHTTP session.
    HINTERNET hSession = WinHttpOpen(
        L"ersteinrichtung/0.2.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        result.error = formatWinHttpError(GetLastError());
        return result;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
                                        INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) {
        result.error = formatWinHttpError(GetLastError());
        WinHttpCloseHandle(hSession);
        return result;
    }

    DWORD requestFlags = 0x00001000 | 0x04000000;  // RELOAD | NO_CACHE_WRITE
    if (isHttps) {
        requestFlags |= WINHTTP_FLAG_SECURE;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, nullptr, resource.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            requestFlags);
    if (!hRequest) {
        result.error = formatWinHttpError(GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Disable certificate revocation and date validation for compatibility.
    BOOL ignoreCert = TRUE;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_IGNORE_CERT_REVOCATION,
                     &ignoreCert, sizeof(ignoreCert));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_IGNORE_CERT_DATE_INVALID,
                     &ignoreCert, sizeof(ignoreCert));

    // Send the request.
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        result.error = formatWinHttpError(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Receive the response.
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        result.error = formatWinHttpError(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Query the HTTP status code.
    DWORD statusCode = 0;
    DWORD bufferSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                             nullptr, &statusCode, &bufferSize, nullptr)) {
        result.error = formatWinHttpError(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    if (statusCode < 200 || statusCode >= 300) {
        result.error = httpStatusToError(statusCode);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Query Content-Length if available.
    std::optional<std::uint64_t> contentLength;
    std::wstring contentLengthStr;
    std::vector<wchar_t> clBuf(32);
    DWORD clSize = static_cast<DWORD>(clBuf.size() * sizeof(wchar_t));
    if (WinHttpQueryHeaders(hRequest, HTTP_QUERY_CONTENT_LENGTH, nullptr,
                            clBuf.data(), &clSize, nullptr)) {
        contentLengthStr = std::wstring(clBuf.data(), clSize / sizeof(wchar_t) - 1);
        // Remove trailing whitespace that WinHTTP may include.
        contentLengthStr.erase(contentLengthStr.find_last_not_of(L" \t\r\n") + 1);
        try {
            contentLength = std::stoull(contentLengthStr);
        } catch (...) {
            contentLength.reset();
        }
    }

    // Open the output file.
    std::ofstream outFile(destinationPath, std::ios::binary);
    if (!outFile) {
        result.error = L"Cannot open destination file: " + destinationPath;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Read data in chunks and write to file.
    std::vector<char> buffer(65536);  // 64 KiB chunks.
    std::uint64_t totalBytes = 0;

    // Print progress header.
    std::wcout << L"  Downloading...";
    if (contentLength.has_value()) {
        // Print size in human-readable format.
        std::wstringstream sizeStr;
        std::uint64_t bytes = *contentLength;
        if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
            sizeStr << (bytes / (1024ULL * 1024ULL * 1024ULL)) << L" GiB";
        } else if (bytes >= 1024ULL * 1024ULL) {
            sizeStr << (bytes / (1024ULL * 1024ULL)) << L" MiB";
        } else if (bytes >= 1024ULL) {
            sizeStr << (bytes / 1024ULL) << L" KiB";
        } else {
            sizeStr << bytes << L" B";
        }
        std::wcout << L" (" << sizeStr.str() << L")";
    }
    std::wcout << L"\n";

    while (true) {
        DWORD bytesRead = 0;
        if (!WinHttpReadData(hRequest, buffer.data(),
                             static_cast<DWORD>(buffer.size()), &bytesRead)) {
            result.error = formatWinHttpError(GetLastError());
            outFile.close();
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }
        if (bytesRead == 0) break;  // No more data.

        outFile.write(buffer.data(), bytesRead);
        totalBytes += bytesRead;
    }

    outFile.close();

    // Verify file was written successfully.
    if (!outFile.good() && !outFile.eof()) {
        result.error = L"Error writing to destination file.";
        return result;
    }

    result.bytesWritten = totalBytes;
    result.success = true;
    std::wcout << L"  Downloaded " << totalBytes << L" bytes.\n";

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

}  // namespace core
