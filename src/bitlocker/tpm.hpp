#pragma once

namespace bitlocker {

// Returns true if a TPM chip is present on this machine.
// Uses the same WMI class / COM interface that PowerShell's Get-Tpm uses.
bool isTpmAvailable();

}  // namespace bitlocker
