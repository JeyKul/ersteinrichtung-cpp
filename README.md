# Ersteinrichtung C++

A native Windows C++20 console application for a setup workflow. It must be run elevated. The current implemented module is the BitLocker menu, which calls Windows `manage-bde.exe` directly; it does not ship or invoke PowerShell scripts.

## Build

Open an x64 Native Tools Command Prompt for Visual Studio, then run:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Run `build\Release\Ersteinrichtung.exe` as administrator.

## Implemented menu

Main-menu option 5 opens the BitLocker module:

- View status
- Select a fixed drive
- Encrypt or decrypt
- Pause or resume encryption
- Enable or disable auto-unlock on data drives
- Export recovery-protector information to an absolute file path
- Add a recovery password
- Upgrade metadata or wipe free space

Every state-changing command requires an exact typed confirmation that includes the selected drive. Recovery information is sensitive. Test only in a disposable VM and store exported recovery files outside the VM or encrypted target volume.

## Notes

- BitLocker availability depends on the Windows edition, disk layout, TPM state, and local policy.
- Encryption can take a long time and must not be interrupted.
- This application does not automatically create a TPM protector.
