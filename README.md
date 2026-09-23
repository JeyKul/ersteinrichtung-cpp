# Ersteinrichtung C++
Windows C++20 console prototype, built as a single elevated executable. The BitLocker submenu under main-menu option 5 calls the Windows `manage-bde.exe` utility (no PowerShell files shipped). Other menus remain placeholders.

## Build
`cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -T host=x64`
`cmake --build build --config Release`
Run `build\Release\Ersteinrichtung.exe`.

## BitLocker test
Use a disposable VM and save recovery keys **outside** the VM. The menu offers overview, fixed-drive selection, encrypt/decrypt, pause/resume, data-drive auto-unlock, recovery-protector export, recovery-password addition, upgrade and wipe-free-space. Commands changing state require exact drive-specific typed confirmations. Output containing recovery passwords is hidden; export uses a user-specified absolute path. Exported key files are sensitive plaintext: protect them and verify contents before reboot. No TPM protector is created automatically; BitLocker policy/TPM can cause enablement to fail. This prototype is not yet CI-built or Windows-tested.
