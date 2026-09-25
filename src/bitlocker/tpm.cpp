#include "tpm.hpp"

#include "../core/wmi.hpp"

#include <cstdint>

namespace bitlocker {

// The Win32_Tpm SpecId is the same source PowerShell's Get-Tpm uses: a
// valid (non-zero, non-0xFFFFFFFF) value means a TPM chip was detected.
bool isTpmAvailable() {
    const std::optional<std::uint32_t> specId =
        core::queryWmiUintProperty(L"Win32_Tpm", L"SpecId");

    return specId.has_value() && *specId != 0 && *specId != 0xFFFFFFFFu;
}

}  // namespace bitlocker
