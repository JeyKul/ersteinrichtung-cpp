# Ersteinrichtung C++

Native C++20 rewrite of the original PowerShell setup workflow for Windows 10/11.
It uses Windows APIs and system executables directly for the normal workflow; PowerShell is retained only for the official Chocolatey bootstrap and Windows shortcut creation.

## Implemented

- Main menu and all four private/business batch variants
- Computer rename, Chocolatey, default programs, Supremo, optional bundled TeamViewer
- Windows Update trigger, system/security status, privacy registry/service changes
- Tools: WinDirStat, Advanced IP Scanner, Lenovo tools, system info, PowerShell 7, TEMP cleanup
- Extras: Store/App Installer links, LTSC-to-Pro registry path, driver export/import
- Sysprep/OOBE with bundled unattend file
- Full BitLocker management with typed confirmations and protected recovery export
- Per-session logs under `%ProgramData%\Ersteinrichtung\Logs\<PC>`
- Administrator manifest and GitHub Actions x64 build

## Build

```bat
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

Run `build\Release\Ersteinrichtung.exe`. The manifest requests elevation automatically.

## Optional business installer

The old repository did not contain `TeamViewer_Host_Setup.exe`. To enable that part, place a trusted installer at:

```text
assets\TeamViewer_Host_Setup.exe
```

The program otherwise installs Supremo and reports that TeamViewer was skipped.

## Safety

Test destructive/system-changing actions in a disposable Windows VM first. BitLocker recovery output is deliberately hidden and may only be exported to a different volume. Downloads use HTTPS and downloaded executables/MSIs are checked for an Authenticode signature before launch.
