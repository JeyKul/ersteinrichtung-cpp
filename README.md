# Ersteinrichtung C++

An early native C++ rewrite of the Windows first-setup menu from [`ersteinrichtung_script`](https://github.com/JeyKul/ersteinrichtung_script).

## Current scope

- Builds one Windows console executable: `Ersteinrichtung.exe`.
- Requests administrator privileges through its embedded application manifest.
- Renders an interactive main menu with keyboard selection.
- Includes a system status header for elevation, Windows version, computer name, and current user.
- Includes safe placeholder actions for Setup, Tools, Extras, BitLocker, and Windows Update.
- Does **not** yet perform system configuration, launch installers, modify BitLocker, or run updates.

## Build

Install a C++20-capable compiler and CMake. The most straightforward option is Visual Studio 2022 with the **Desktop development with C++** workload.

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The resulting executable is normally located at:

```text
build\Release\Ersteinrichtung.exe
```

The embedded manifest requests UAC elevation when you launch it.

## Controls

- Use `1`–`6` to choose a menu option.
- Use `0`, `Q`, or `Esc` to exit.
- Press Enter after a placeholder screen to return to the main menu.

## Roadmap

1. Match the exact PowerShell main-menu layout and choices.
2. Port shared helpers: logging, network checks, status detection, and step handling.
3. Port individual setup operations with explicit confirmation and error reporting.
4. Reimplement update and BitLocker workflows after safe testing in a VM.
5. Embed static resources so the distributed release remains a single executable.
