#include "tools_menu.hpp"

#include "../core/http_download.hpp"
#include "../core/process_runner.hpp"
#include "../core/wmi.hpp"
#include "../extra/extra_menu.hpp"
#include "../bitlocker/bitlocker_menu.hpp"

#include <windows.h>
#include <objbase.h>
#include <shlobj.h>

#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tools {
namespace {

// --- Helpers ---------------------------------------------------------------

std::wstring prompt(const std::wstring& label) {
    std::wcout << label;
    std::wstring value;
    std::getline(std::wcin, value);
    return value;
}

void waitForEnter() {
    std::wcout << L"\nPress Enter to continue...";
    std::wstring ignored;
    std::getline(std::wcin, ignored);
}

std::wstring getSystemDirectoryExecutable(const std::wstring& name) {
    wchar_t systemDirectory[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(systemDirectory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return {};
    return std::wstring(systemDirectory) + L"\\" + name;
}

void runAndReport(
    const std::wstring& executable,
    const std::vector<std::wstring>& arguments) {
    if (executable.empty()) {
        std::wcout << L"\nCould not locate the required executable.\n";
        return;
    }
    const core::ProcessResult result = core::runProcess(executable, arguments);
    if (!result.error.empty()) {
        std::wcout << L"\nError: " << result.error << L"\n";
        return;
    }
    if (!result.output.empty()) {
        std::wcout << L"\n" << result.output;
    }
    std::wcout << L"\nExit code: " << result.exitCode << L"\n";
}

// Get the directory where this executable lives.
std::wstring getExeDirectory() {
    wchar_t exePath[MAX_PATH]{};
    const DWORD exeLen = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (exeLen == 0 || exeLen >= MAX_PATH) return {};
    std::wstring exe(exePath);
    return exe.substr(0, exe.find_last_of(L"\\/") + 1);
}

// Write a PowerShell command to a temp .ps1 file, run it via
// cmd.exe/powershell, and pipe the output.  Used only by
// checkWindowsUpdates at this point.
static void runPsCommand(const std::wstring& command) {
    // Write command to temp file.
    wchar_t tempDir[MAX_PATH]{};
    const DWORD tempLen = GetTempPathW(MAX_PATH, tempDir);
    if (tempLen == 0 || tempLen >= MAX_PATH) return;
    const std::wstring psScript =
        std::wstring(tempDir) + L"__ersteinrichtung_.ps1";
    std::ofstream pf(psScript);
    if (!pf) return;

    // Encode command as UTF-8 for file writing.
    const int utf8Len =
        WideCharToMultiByte(CP_UTF8, 0, command.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, command.c_str(), -1,
                        utf8.data(), utf8Len, nullptr, nullptr);
    pf << utf8;
    pf.close();

    // Run via cmd.exe /c powershell -File ...
    std::wstring runCmd = L"cmd.exe /c powershell -ExecutionPolicy Bypass -File \"" + psScript + L"\"";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE rPipe = nullptr, wPipe = nullptr;
    if (!CreatePipe(&rPipe, &wPipe, &sa, 0)) return;
    SetHandleInformation(rPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = wPipe;
    si.hStdError  = wPipe;

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCmd(runCmd.begin(), runCmd.end());
    mutableCmd.push_back(L'\0');

    if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(rPipe);
        CloseHandle(wPipe);
        return;
    }
    CloseHandle(wPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Read captured output.
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    std::string bytes;
    char buf[4096];
    DWORD bytesRead = 0;
    while (ReadFile(rPipe, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
        bytes.append(buf, bytesRead);
    }
    CloseHandle(rPipe);

    // Delete temp script.
    std::error_code ec;
    std::filesystem::remove(psScript, ec);

    if (!bytes.empty()) {
        const int wcLen = MultiByteToWideChar(CP_OEMCP, 0,
            bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
        std::wstring output(static_cast<std::size_t>(wcLen), L' ');
        MultiByteToWideChar(CP_OEMCP, 0, bytes.data(), static_cast<int>(bytes.size()),
                            output.data(), wcLen);
        std::wcout << L"\n" << output;
    }
    std::wcout << L"\nExit code: " << exitCode << L"\n";
}

// Create a .lnk shortcut via native IShellLink COM interface.
static void createShortcut(
    const std::wstring& shortcutPath,
    const std::wstring& targetPath,
    const std::wstring& workingDir,
    const std::wstring& description,
    const std::wstring& iconLocation) {
    if (std::filesystem::exists(shortcutPath)) return;

    const HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize =
        initResult == S_OK || initResult == RPC_E_CHANGED_MODE;

    IShellLinkW* psl = nullptr;
    const HRESULT created = CoCreateInstance(
        CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
        IID_IShellLinkW, reinterpret_cast<void**>(&psl));

    if (created == S_OK && psl != nullptr) {
        psl->SetPath(targetPath.c_str());
        psl->SetWorkingDirectory(workingDir.c_str());
        psl->SetDescription(description.c_str());
        psl->SetIconLocation(iconLocation.c_str(), 0);

        // Query IPersistFile to save the shortcut.
        IPersistFile* ppf = nullptr;
        if (psl->QueryInterface(IID_IPersistFile,
                                reinterpret_cast<void**>(&ppf)) == S_OK) {
            const HRESULT saved = ppf->Save(
                shortcutPath.c_str(), TRUE);
            if (saved != S_OK) {
                std::wcout << L"Failed to save shortcut: 0x"
                           << std::hex << saved << L"\n";
            }
            ppf->Release();
        }
        psl->Release();
    }

    if (shouldUninitialize) {
        CoUninitialize();
    }
}

}  // namespace

// --- Public tool functions (callable from main.cpp) --------------------------

void renameComputer() {
    std::wcout << L"\n=== Rename Computer ===\n";
    const std::wstring currentName = []() -> std::wstring {
        wchar_t name[MAX_COMPUTERNAME_LENGTH + 1]{};
        DWORD length = MAX_COMPUTERNAME_LENGTH + 1;
        if (GetComputerNameExW(ComputerNameNetBIOS, name, &length)) {
            return std::wstring(name);
        }
        return L"<unknown>";
    }();

    std::wcout << L"Current name: " << currentName << L"\n";
    const std::wstring newName = prompt(L"Enter the new computer name: ");
    if (newName.empty()) {
        std::wcout << L"Aborted.\n";
        waitForEnter();
        return;
    }
    if (newName == currentName) {
        std::wcout << L"The computer is already named '" << newName << L"'.\n";
        waitForEnter();
        return;
    }
    if (newName.length() > 15) {
        std::wcout << L"Computer name must be 1-15 characters.\n";
        waitForEnter();
        return;
    }
    for (wchar_t c : newName) {
        if (!iswalnum(c) && c != L'-') {
            std::wcout << L"Computer name may only contain letters, digits, and hyphens.\n";
            waitForEnter();
            return;
        }
    }
    if (!SetComputerNameExW(ComputerNameNetBIOS, newName.c_str())) {
        std::wcout << L"Failed to rename computer. Error: " << GetLastError() << L"\n";
        waitForEnter();
        return;
    }
    std::wcout << L"Computer renamed to '" << newName
               << L"'. A reboot is required to take effect.\n";
    waitForEnter();
}

static bool chocoInstalled() {
    const std::vector<std::wstring> paths = {
        L"C:\\ProgramData\\chocolatey\\bin\\choco.exe",
        L"C:\\ProgramData\\chocolatey\\choco.exe",
    };
    for (const auto& path : paths) {
        if (std::filesystem::exists(path)) return true;
    }
    return false;
}

void installChocolatey() {
    std::wcout << L"\n=== Chocolatey Installer ===\n";
    if (chocoInstalled()) {
        std::wcout << L"Chocolatey already installed. Upgrading...\n";
        runAndReport(getSystemDirectoryExecutable(L"cmd.exe"), {L"/c", L"choco upgrade chocolatey -y"});
        waitForEnter();
        return;
    }
    std::wcout << L"Chocolatey not found. Downloading installer ...\n";

    // Download the official install.ps1 script to a temp file, then run it.
    wchar_t tempPath[MAX_PATH]{};
    const DWORD tempLen = GetTempPathW(MAX_PATH, tempPath);
    if (tempLen == 0 || tempLen >= MAX_PATH) {
        std::wcout << L"Could not get temp path.\n";
        waitForEnter();
        return;
    }
    const std::wstring psScriptPath = std::wstring(tempPath) + L"choco_install.ps1";

    const core::DownloadResult dlResult =
        core::downloadFile(
            L"https://community.chocolatey.org/install.ps1", psScriptPath);
    if (!dlResult.success) {
        std::wcout << L"Download failed: " << dlResult.error << L"\n";
        waitForEnter();
        return;
    }

    // Run the script via PowerShell (required for MSI install logic).
    std::wcout << L"Running Chocolatey installer ...\n";
    std::wstring runCmd =
        L"powershell.exe -ExecutionPolicy Bypass -File \"" + psScriptPath + L"\"";
    core::ProcessResult result =
        core::runProcess(getSystemDirectoryExecutable(L"cmd.exe"),
                         {L"/c", runCmd});
    if (!result.error.empty()) {
        std::wcout << L"Error: " << result.error << L"\n";
    }
    if (!result.output.empty()) {
        std::wcout << L"\n" << result.output;
    }
    std::wcout << L"\nExit code: " << result.exitCode << L"\n";

    // Clean up temp script.
    std::error_code ec;
    std::filesystem::remove(psScriptPath, ec);

    waitForEnter();
}

void installSupremoPK() {
    std::wcout << L"\n=== Install Supremo (Privatkunden) ===\n";
    const std::wstring targetDir = L"C:\\CT-T";
    const std::wstring targetExe = targetDir + L"\\Supremo.exe";
    std::error_code ec;
    std::filesystem::create_directories(targetDir, ec);
    if (std::filesystem::exists(targetExe)) {
        std::wcout << L"Supremo already installed at " << targetExe << L".\n";
        waitForEnter();
        return;
    }
    const std::wstring localSource = getExeDirectory() + L"Supremo.exe";
    if (std::filesystem::exists(localSource)) {
        std::filesystem::copy_file(
            localSource, targetExe, std::filesystem::copy_options::overwrite_existing);
        std::wcout << L"Supremo copied from local EXE folder.\n";
    } else {
        std::wcout << L"Downloading Supremo from ct-t.de ...\n";
        const core::DownloadResult dlResult =
            core::downloadFile(L"https://www.ct-t.de/prog/Supremo.exe", targetExe);
        if (!dlResult.success) {
            std::wcout << L"Download failed: " << dlResult.error << L"\n";
            waitForEnter();
            return;
        }
    }
    if (!std::filesystem::exists(targetExe)) {
        std::wcout << L"Supremo installation failed.\n";
        waitForEnter();
        return;
    }
    std::wcout << L"Creating desktop shortcut ...\n";
    createShortcut(
        L"C:\\Users\\Public\\Desktop\\CT-T Support.lnk",
        targetExe, targetDir,
        L"CT-T Remote Support",
        targetExe + L", 0");
    std::wcout << L"\nSupremo installation complete.\n";
    waitForEnter();
}

void installTeamViewerGK() {
    std::wcout << L"\n=== Install TeamViewer GK + Supremo ===\n";
    const std::wstring targetDir = L"C:\\CT-T";
    std::error_code ec;
    std::filesystem::create_directories(targetDir, ec);
    const std::wstring tvHostLocal = getExeDirectory() + L"TeamViewer_Host_Setup.exe";
    if (std::filesystem::exists(tvHostLocal)) {
        std::wcout << L"Installing TeamViewer Host Setup silently ...\n";
        runAndReport(tvHostLocal, {L"/S"});
    } else {
        std::wcout << L"TeamViewer_Host_Setup.exe not found in EXE/ folder. Skipping.\n";
    }
    const std::wstring tvQsLocal = getExeDirectory() + L"TeamViewerQS.exe";
    const std::wstring tvQsTarget = targetDir + L"\\TeamViewerQS.exe";
    if (std::filesystem::exists(tvQsLocal)) {
        std::filesystem::copy_file(
            tvQsLocal, tvQsTarget, std::filesystem::copy_options::overwrite_existing);
        std::wcout << L"TeamViewer QS copied to " << targetDir << L"\n";
    } else {
        std::wcout << L"TeamViewerQS.exe not found in EXE/ folder.\n";
    }
    installSupremoPK();
    std::wcout << L"\nTeamViewer GK installation complete.\n";
    waitForEnter();
}

void installDefaultPrograms() {
    std::wcout << L"\n=== Install Default Programs ===\n";
    if (!chocoInstalled()) {
        std::wcout << L"Chocolatey is not installed. Installing it first...\n";
        installChocolatey();
    }
    if (!chocoInstalled()) {
        std::wcout << L"Chocolatey installation failed. Cannot install programs.\n";
        waitForEnter();
        return;
    }
    const std::vector<std::pair<std::wstring, std::wstring>> programs = {
        {L"VLC Media Player",   L"vlc"},
        {L"7-Zip Zstandard",    L"7zip-zstd"},
        {L"Mozilla Firefox",    L"firefox"},
    };
    for (const auto& [name, id] : programs) {
        std::wcout << L"\nInstalling " << name << L" ...\n";
        std::wstring cmd = L"choco install " + id + L" -y";
        if (id == L"firefox") {
            cmd = L"choco install " + id + L" -y --install-arguments='/l:de'";
        }
        runAndReport(getSystemDirectoryExecutable(L"cmd.exe"), {L"/c", cmd});
    }
    std::wcout << L"\n--- Verification ---\n";
    const std::vector<std::pair<std::wstring, std::wstring>> paths = {
        {L"VLC",     L"C:\\Program Files\\VideoLAN\\VLC\\vlc.exe"},
        {L"7-Zip-Zstd", L"C:\\Program Files\\7-Zip-Zstandard\\7zFM.exe"},
        {L"Firefox", L"C:\\Program Files\\Mozilla Firefox\\firefox.exe"},
    };
    for (const auto& [name, path] : paths) {
        std::wcout << L"  " << name << L": "
                   << (std::filesystem::exists(path) ? L"installed" : L"NOT installed") << L"\n";
    }
    std::wcout << L"\nDefault programs installation complete.\n";
    waitForEnter();
}

void checkWindowsUpdates() {
    std::wcout << L"\n=== Windows Update Check ===\n";
    const std::wstring psCmd =
        L"$session = New-Object -ComObject Microsoft.Update.Session; "
        L"$searcher = $session.CreateUpdateSearcher(); "
        L"Write-Host 'Searching for updates...' -ForegroundColor Cyan; "
        L"$result = $searcher.Search('IsInstalled=0'); "
        L"if ($result.Updates.Count -eq 0) { "
        L"  Write-Host 'No updates available.' -ForegroundColor Green; "
        L"} else { "
        L"  Write-Host (($result.Updates.Count) + ' update(s) available:') -ForegroundColor Yellow; "
        L"  $result.Updates | ForEach-Object { Write-Host ('  - ' + $_.Title) -ForegroundColor White }; "
        L"}";
    runPsCommand(psCmd);
    std::wcout << L"\nWindows update check complete.\n";
    waitForEnter();
}

void disableTelemetry() {
    std::wcout << L"\n=== Disable Telemetry & Privacy Tweaks ===\n";
    std::wcout << L"This will apply registry tweaks and disable telemetry services.\n";

    struct RegItem {
        HKEY hKey;
        std::wstring path;
        std::wstring name;
        DWORD value;
        DWORD type;
    };
    const std::vector<RegItem> regItems = {
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo", L"Enabled", 0, REG_DWORD},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy", L"TailoredExperiencesWithDiagnosticDataEnabled", 0, REG_DWORD},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Speech_OneCore\\Settings\\OnlineSpeechPrivacy", L"HasAccepted", 0, REG_DWORD},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Input\\TIPC", L"Enabled", 0, REG_DWORD},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\InputPersonalization", L"RestrictImplicitInkCollection", 1, REG_DWORD},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\InputPersonalization", L"RestrictImplicitTextCollection", 1, REG_DWORD},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\InputPersonalization\\TrainedDataStore", L"HarvestContacts", 0, REG_DWORD},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Personalization\\Settings", L"AcceptedPrivacyPolicy", 0, REG_DWORD},
        {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\DataCollection", L"AllowTelemetry", 0, REG_DWORD},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_TrackProgs", 0, REG_DWORD},
        {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\System", L"PublishUserActivities", 0, REG_DWORD},
        {HKEY_CURRENT_USER, L"Software\\Microsoft\\Siuf\\Rules", L"NumberOfSIUFInPeriod", 0, REG_DWORD},
    };

    for (const auto& item : regItems) {
        HKEY key = nullptr;
        LSTATUS status = RegCreateKeyExW(
            item.hKey, item.path.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, nullptr);
        if (status == ERROR_SUCCESS && key != nullptr) {
            status = RegSetValueExW(key, item.name.c_str(), 0, item.type,
                                   reinterpret_cast<const BYTE*>(&item.value), sizeof(item.value));
            RegCloseKey(key);
        }
        if (status == ERROR_SUCCESS) {
            std::wcout << L"  OK: " << item.name.c_str() << L"\n";
        } else {
            std::wcout << L"  FAIL: " << item.name.c_str() << L" (error " << status << L")\n";
        }
    }

    std::wcout << L"\nDisabling services ...\n";
    auto disableService = [](const std::wstring& name) -> bool {
        SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!scm) return false;
        SC_HANDLE svc = OpenServiceW(scm, name.c_str(), SERVICE_CHANGE_CONFIG);
        if (!svc) { CloseServiceHandle(scm); return false; }
        BOOL ok = ChangeServiceConfigW(svc, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        CloseServiceHandle(svc);
        CloseServiceHandle(scm);
        return ok != FALSE;
    };
    if (disableService(L"DiagTrack"))      std::wcout << L"  DiagTrack disabled\n";
    else                                  std::wcout << L"  DiagTrack: could not disable\n";
    if (disableService(L"WerSvc"))        std::wcout << L"  WerSvc disabled\n";
    else                                  std::wcout << L"  WerSvc: could not disable\n";

    std::wcout << L"\nDisabling Defender auto sample submission ...\n";
    {
        HKEY key = nullptr;
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
                            L"SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Submission",
                            0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
                            nullptr, &key, nullptr) == ERROR_SUCCESS &&
            key != nullptr) {
            const DWORD value = 2;
            RegSetValueExW(key, L"SubmitSamplesConsent", 0, REG_DWORD,
                           reinterpret_cast<const BYTE*>(&value), sizeof(value));
            RegCloseKey(key);
            std::wcout << L"  Defender submission disabled (registry).\n";
        } else {
            std::wcout << L"  Could not set Defender submission policy.\n";
        }
    }

    std::wcout << L"\nSetting SvcHostSplitThresholdInKB ...\n";
    auto totalPhys = core::queryWmiUintProperty(L"Win32_ComputerSystem", L"TotalPhysicalMemory");
    if (totalPhys.has_value()) {
        const ULONGLONG kb = static_cast<ULONGLONG>(*totalPhys) / 1024;
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control",
                          0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
            RegSetValueExW(key, L"SvcHostSplitThresholdInKB", 0, REG_DWORD,
                          reinterpret_cast<const BYTE*>(&kb), sizeof(kb));
            RegCloseKey(key);
            std::wcout << L"  Set to " << kb << L" KB\n";
        }
    }
    std::wcout << L"\nTelemetry tweaks applied.\n";
    waitForEnter();
}

static void checkSecureBootAndBitLocker() {
    std::wcout << L"\n=== System Check ===\n\n";
    auto sb = core::queryWmiUintProperty(L"Win32_SecurityBootstrapVerification", L"PlatformRole");
    if (sb.has_value() && *sb == 2) {
        std::wcout << L"  Secure Boot: ON\n";
    } else {
        std::wcout << L"  Secure Boot: OFF\n";
    }
    auto tpm = core::queryWmiUintProperty(L"Win32_Tpm", L"SpecId");
    bool tpmOk = tpm.has_value() && *tpm != 0 && *tpm != 0xFFFFFFFFu;
    std::wcout << L"  TPM: " << (tpmOk ? L"present" : L"not found") << L"\n";
    std::wcout << L"\n--- BitLocker Status ---\n";
    runAndReport(getSystemDirectoryExecutable(L"manage-bde.exe"), {L"-status"});
    std::wcout << L"\nSystem check complete.\n";
    waitForEnter();
}

static void runSysprep() {
    std::wcout << L"\n=== Sysprep OOBE ===\n";
    std::wcout << L"This will run sysprep /oobe /reboot and exit this program.\n"
               << L"Are you sure? (y/N): ";
    std::wstring answer;
    std::getline(std::wcin, answer);
    if (answer != L"y" && answer != L"Y") {
        std::wcout << L"Cancelled.\n";
        waitForEnter();
        return;
    }
    std::wstring sysprepCmd = L"sysprep.exe /oobe /reboot";
    wchar_t exePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
        std::wstring exe(exePath);
        const std::wstring exeDir = exe.substr(0, exe.find_last_of(L"\\/") + 1);
        const std::wstring unattend = exeDir + L"unattend.xml";
        if (std::filesystem::exists(unattend)) {
            sysprepCmd = L"sysprep.exe /oobe /reboot /unattend:" + unattend;
        }
    }
    runAndReport(getSystemDirectoryExecutable(L"cmd.exe"), {L"/c", sysprepCmd});
    std::wcout << L"\nProgram exiting. Sysprep will reboot the machine.\n";
    std::exit(0);
}

static void checkWhyNotWin11() {
    std::wcout << L"\n=== Windows 11 Compatibility Check ===\n";
    std::wstring whyExe = getExeDirectory() + L"WhyNotWin11.exe";
    if (!std::filesystem::exists(whyExe)) {
        std::wcout << L"WhyNotWin11.exe not found in EXE/ folder.\n";
        std::wcout << L"Downloading from GitHub ...\n";
        const core::DownloadResult dlResult =
            core::downloadFile(
                L"https://github.com/rcmaehl/WhyNotWin11/releases/download/2.7.0/WhyNotWin11.exe",
                whyExe);
        if (!dlResult.success) {
            std::wcout << L"Download failed: " << dlResult.error << L"\n";
            waitForEnter();
            return;
        }
    }
    if (!std::filesystem::exists(whyExe)) {
        std::wcout << L"Download failed.\n";
        waitForEnter();
        return;
    }
    wchar_t exePath[MAX_PATH]{};
    DWORD exeLen = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring logPath;
    if (exeLen > 0 && exeLen < MAX_PATH) {
        std::wstring exe(exePath);
        logPath = exe.substr(0, exe.find_last_of(L"\\/") + 1) + L"whynotwin11.txt";
    }
    std::wstring launchCmd = whyExe + L" /f /silent";
    if (!logPath.empty()) {
        launchCmd = whyExe + L" /e txt \"" + logPath + L"\" /f /silent";
    }
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE rPipe = nullptr, wPipe = nullptr;
    if (!CreatePipe(&rPipe, &wPipe, &sa, 0)) {
        std::wcout << L"Pipe creation failed.\n";
        waitForEnter();
        return;
    }
    SetHandleInformation(rPipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = wPipe;
    si.hStdError = wPipe;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCmd(launchCmd.begin(), launchCmd.end());
    mutableCmd.push_back(L'\0');
    if (CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(wPipe);
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        std::string bytes;
        char buf[4096];
        DWORD bytesRead = 0;
        while (ReadFile(rPipe, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
            bytes.append(buf, bytesRead);
        }
        CloseHandle(rPipe);
        if (!bytes.empty()) {
            const int wcLen = MultiByteToWideChar(CP_OEMCP, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
            std::wstring output(static_cast<std::size_t>(wcLen), L' ');
            MultiByteToWideChar(CP_OEMCP, 0, bytes.data(), static_cast<int>(bytes.size()),
                               output.data(), wcLen);
            std::wcout << L"\n" << output;
        }
        std::wcout << L"\nWhyNotWin11 exited with code " << exitCode << L"\n";
    } else {
        std::wcout << L"Failed to launch WhyNotWin11.\n";
    }
    waitForEnter();
}

void disableFastBoot() {
    std::wcout << L"\n=== Disable Fast Boot & Standby ===\n";
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"/h", L"off"});
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"-change", L"-standby-timeout-ac", L"0"});
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"-change", L"-standby-timeout-dc", L"0"});
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"-change", L"-hibernate-timeout-ac", L"0"});
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"-change", L"-hibernate-timeout-dc", L"0"});
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"-change", L"-monitor-timeout-ac", L"0"});
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"-change", L"-monitor-timeout-dc", L"0"});
    std::wcout << L"Fast boot and all timeouts disabled.\n";
    waitForEnter();
}

void revertFastBoot() {
    std::wcout << L"\n=== Revert Fast Boot & Standby ===\n";
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"/h", L"on"});
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"-change", L"-standby-timeout-ac", L"30"});
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"-change", L"-standby-timeout-dc", L"15"});
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"-change", L"-hibernate-timeout-ac", L"180"});
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"-change", L"-hibernate-timeout-dc", L"60"});
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"-change", L"-monitor-timeout-ac", L"10"});
    runAndReport(getSystemDirectoryExecutable(L"powercfg.exe"), {L"-change", L"-monitor-timeout-dc", L"5"});
    std::wcout << L"Timeouts reverted to defaults.\n";
    waitForEnter();
}

// --- Tools menu -------------------------------------------------------------

void showMenu() {
    while (true) {
        std::wcout
            << L"\n=============================================\n"
            << L"  TOOLS\n"
            << L"=============================================\n"
            << L"  1.  Rename computer\n"
            << L"  2.  Install / update Chocolatey\n"
            << L"  3.  Install Supremo (Privatkunden)\n"
            << L"  4.  Install TeamViewer GK + Supremo\n"
            << L"  5.  Install default programs (VLC, 7-Zip, Firefox)\n"
            << L"  6.  Check Windows updates\n"
            << L"  7.  Disable telemetry & privacy tweaks\n"
            << L"  8.  SecureBoot + BitLocker system check\n"
            << L"  9.  Sysprep OOBE\n"
            << L"  10. Windows 11 compatibility (WhyNotWin11)\n"
            << L"  11. Disable fast boot & standby\n"
            << L"  12. Revert fast boot & standby\n"
            << L"  20. Extract drivers\n"
            << L"  21. Install drivers\n"
            << L"  30. Extras menu\n"
            << L"  31. BitLocker menu\n"
            << L"  0.  Back\n\n";

        std::wstring choice;
        std::wcout << L"Select an option: ";
        if (!std::getline(std::wcin, choice)) return;

        if (choice == L"0") return;
        else if (choice == L"1")       renameComputer();
        else if (choice == L"2")       installChocolatey();
        else if (choice == L"3")       installSupremoPK();
        else if (choice == L"4")       installTeamViewerGK();
        else if (choice == L"5")       installDefaultPrograms();
        else if (choice == L"6")       checkWindowsUpdates();
        else if (choice == L"7")       disableTelemetry();
        else if (choice == L"8")       checkSecureBootAndBitLocker();
        else if (choice == L"9")       runSysprep();
        else if (choice == L"10")      checkWhyNotWin11();
        else if (choice == L"11")      disableFastBoot();
        else if (choice == L"12")      revertFastBoot();
        else if (choice == L"20")      extra::extractDrivers();
        else if (choice == L"21")      extra::installDrivers();
        else if (choice == L"30")      extra::showMenu();
        else if (choice == L"31")      bitlocker::showMenu();
        else                           std::wcout << L"Invalid selection. Try again.\n";
    }
}

}  // namespace tools
