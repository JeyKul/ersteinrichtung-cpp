#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace core {

// Reads a property from the first instance of a WMI class in root\CIMV2.
// Returns std::nullopt if the class is missing or the query fails.
std::optional<std::wstring> queryWmiStringProperty(
    const std::wstring& wmiClass,
    const std::wstring& property);

std::optional<std::uint32_t> queryWmiUintProperty(
    const std::wstring& wmiClass,
    const std::wstring& property);

}  // namespace core
