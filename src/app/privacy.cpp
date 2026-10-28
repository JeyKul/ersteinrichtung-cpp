#include "actions.hpp"
#include "../core/console.hpp"
#include "../core/process_runner.hpp"
#include "../core/system.hpp"
#include <windows.h>
#include <sddl.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace app {
namespace {
using core::console::line;

struct DwordPolicy { HKEY root; const wchar_t* path; const wchar_t* name; DWORD value; };
const DwordPolicy machinePolicies[] = {
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",L"AllowTelemetry",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\DataCollection",L"AllowTelemetry",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",L"DoNotShowFeedbackNotifications",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",L"LimitDiagnosticLogCollection",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",L"LimitDumpCollection",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",L"DisableOneSettingsDownloads",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",L"AllowDeviceNameInDiagnosticData",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\System",L"PublishUserActivities",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\System",L"UploadUserActivities",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\System",L"EnableActivityFeed",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\System",L"AllowCrossDeviceClipboard",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\System",L"EnableCdp",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\System",L"AllowClipboardHistory",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\Explorer",L"DisableSearchBoxSuggestions",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",L"AllowCloudSearch",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",L"ConnectedSearchUseWeb",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",L"DisableWebSearch",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",L"AllowSearchToUseLocation",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent",L"DisableWindowsConsumerFeatures",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent",L"DisableConsumerAccountStateContent",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent",L"DisableCloudOptimizedContent",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent",L"DisableSoftLanding",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\AdvertisingInfo",L"DisabledByGroupPolicy",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat",L"AITEnable",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat",L"DisableInventory",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat",L"DisablePCA",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\LocationAndSensors",L"DisableLocation",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\LocationAndSensors",L"DisableWindowsLocationProvider",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\Maps",L"AutoDownloadAndUpdateMapData",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\OneDrive",L"DisableFileSyncNGSC",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Error Reporting",L"Disabled",1},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\DeliveryOptimization",L"DODownloadMode",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Feeds",L"EnableFeeds",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Dsh",L"AllowNewsAndInterests",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Windows\\Widgets",L"AllowWidgets",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Edge",L"PersonalizationReportingEnabled",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Edge",L"UserFeedbackAllowed",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Edge",L"DiagnosticData",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Edge",L"SearchSuggestEnabled",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Edge",L"ResolveNavigationErrorsUseWebService",0},
    {HKEY_LOCAL_MACHINE,L"SOFTWARE\\Policies\\Microsoft\\Edge",L"AlternateErrorPagesEnabled",0}
};

struct UserPolicy { const wchar_t* path; const wchar_t* name; DWORD value; };
const UserPolicy userPolicies[] = {
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo",L"Enabled",0},
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy",L"TailoredExperiencesWithDiagnosticDataEnabled",0},
    {L"Software\\Microsoft\\Speech_OneCore\\Settings\\OnlineSpeechPrivacy",L"HasAccepted",0},
    {L"Software\\Microsoft\\Input\\TIPC",L"Enabled",0},
    {L"Software\\Microsoft\\InputPersonalization",L"RestrictImplicitInkCollection",1},
    {L"Software\\Microsoft\\InputPersonalization",L"RestrictImplicitTextCollection",1},
    {L"Software\\Microsoft\\InputPersonalization\\TrainedDataStore",L"HarvestContacts",0},
    {L"Software\\Microsoft\\Personalization\\Settings",L"AcceptedPrivacyPolicy",0},
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",L"Start_TrackProgs",0},
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\Search",L"BingSearchEnabled",0},
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\Search",L"CortanaConsent",0},
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\SearchSettings",L"IsDynamicSearchBoxEnabled",0},
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",L"SilentInstalledAppsEnabled",0},
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",L"ContentDeliveryAllowed",0},
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",L"SystemPaneSuggestionsEnabled",0},
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",L"SubscribedContent-338388Enabled",0},
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",L"SubscribedContent-338389Enabled",0},
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",L"SubscribedContent-353694Enabled",0},
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",L"SubscribedContent-353696Enabled",0},
    {L"Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",L"SubscribedContent-310093Enabled",0},
    {L"Software\\Microsoft\\Siuf\\Rules",L"NumberOfSIUFInPeriod",0}
};

bool setDword(HKEY root,const std::wstring& path,const wchar_t* name,DWORD value,core::Logger& log){
    HKEY key{};const LONG opened=RegCreateKeyExW(root,path.c_str(),0,nullptr,0,KEY_SET_VALUE|KEY_QUERY_VALUE|KEY_WOW64_64KEY,nullptr,&key,nullptr);
    if(opened!=ERROR_SUCCESS){log.write(L"PRIVACY registry open failed "+path+L"\\"+name+L" rc="+std::to_wstring(opened));return false;}
    LONG rc=RegSetValueExW(key,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value));DWORD actual{},type{},size=sizeof(actual);
    if(rc==ERROR_SUCCESS)rc=RegQueryValueExW(key,name,nullptr,&type,reinterpret_cast<BYTE*>(&actual),&size);RegCloseKey(key);
    const bool ok=rc==ERROR_SUCCESS&&type==REG_DWORD&&actual==value;log.write(std::wstring(ok?L"PRIVACY OK ":L"PRIVACY FAIL ")+path+L"\\"+name+L"="+std::to_wstring(value));return ok;
}
bool applyUserHive(HKEY hive,const std::wstring& prefix,core::Logger& log){bool ok=true;for(const auto& e:userPolicies){const std::wstring path=prefix.empty()?e.path:prefix+L"\\"+e.path;ok=setDword(hive,path,e.name,e.value,log)&&ok;}return ok;}
std::vector<std::wstring> loadedUserSids(){std::vector<std::wstring> result;for(DWORD i=0;;++i){wchar_t name[256]{};DWORD size=256;FILETIME stamp{};const LONG rc=RegEnumKeyExW(HKEY_USERS,i,name,&size,nullptr,nullptr,nullptr,&stamp);if(rc==ERROR_NO_MORE_ITEMS)break;if(rc==ERROR_SUCCESS){std::wstring sid(name,size);if(sid.rfind(L"S-1-5-21-",0)==0&&sid.find(L"_Classes")==std::wstring::npos)result.push_back(sid);}}return result;}
std::vector<std::pair<std::wstring,std::filesystem::path>> profileHives(){
    std::vector<std::pair<std::wstring,std::filesystem::path>> result;HKEY list{};
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList",0,KEY_ENUMERATE_SUB_KEYS|KEY_QUERY_VALUE|KEY_WOW64_64KEY,&list)!=ERROR_SUCCESS)return result;
    for(DWORD i=0;;++i){wchar_t sid[256]{};DWORD sidSize=256;FILETIME stamp{};const LONG rc=RegEnumKeyExW(list,i,sid,&sidSize,nullptr,nullptr,nullptr,&stamp);if(rc==ERROR_NO_MORE_ITEMS)break;if(rc!=ERROR_SUCCESS)continue;
        std::wstring sidText(sid,sidSize);if(sidText.rfind(L"S-1-5-21-",0)!=0)continue;HKEY profile{};if(RegOpenKeyExW(list,sidText.c_str(),0,KEY_QUERY_VALUE,&profile)!=ERROR_SUCCESS)continue;
        wchar_t raw[32768]{};DWORD type{},bytes=sizeof(raw);if(RegQueryValueExW(profile,L"ProfileImagePath",nullptr,&type,reinterpret_cast<BYTE*>(raw),&bytes)==ERROR_SUCCESS&&(type==REG_SZ||type==REG_EXPAND_SZ)){wchar_t expanded[32768]{};ExpandEnvironmentStringsW(raw,expanded,static_cast<DWORD>(std::size(expanded)));result.push_back({sidText,std::filesystem::path(expanded)/L"NTUSER.DAT"});}RegCloseKey(profile);
    }RegCloseKey(list);return result;
}
bool run(const std::wstring& exe,const std::vector<std::wstring>& args,core::Logger& log,const std::wstring& label,const std::vector<DWORD>& accepted={0}){core::ProcessOptions options;options.onOutput=[&](const std::wstring& text){if(!text.empty())log.write(L"PRIVACY "+label+L" "+text);};const auto result=core::runProcess(exe,args,options);bool ok=result.started;if(ok){ok=false;for(const DWORD code:accepted)if(result.exitCode==code){ok=true;break;}}log.write(L"PRIVACY "+label+L" exit="+std::to_wstring(result.exitCode));return ok;}
bool configureService(const wchar_t* name,core::Logger& log){const auto sc=core::windowsSystemExecutable(L"sc.exe");run(sc,{L"stop",name},log,L"stop service",{0,1060,1062});return run(sc,{L"config",name,L"start=",L"disabled"},log,L"disable service",{0,1060});}
bool removeSelectedApps(core::Logger& log){
    const auto ps=core::windowsSystemExecutable(L"WindowsPowerShell\\v1.0\\powershell.exe");
    const std::wstring command=LR"PS(
$ErrorActionPreference='Continue'
$failed=0
$targets=@('Microsoft.OutlookForWindows','microsoft.windowscommunicationsapps','Microsoft.MicrosoftSolitaireCollection','Clipchamp.Clipchamp','7EE7776C.LinkedInforWindows','5319275A.WhatsAppDesktop')
foreach($pattern in $targets){
  Get-AppxPackage -AllUsers -Name $pattern -ErrorAction SilentlyContinue | ForEach-Object { try { Remove-AppxPackage -Package $_.PackageFullName -AllUsers -ErrorAction Stop } catch { $failed++; Write-Warning $_ } }
  Get-AppxProvisionedPackage -Online | Where-Object { $_.DisplayName -like $pattern -or $_.PackageName -like ('*'+$pattern+'*') } | ForEach-Object { try { Remove-AppxProvisionedPackage -Online -AllUsers -PackageName $_.PackageName -ErrorAction Stop | Out-Null } catch { $failed++; Write-Warning $_ } }
}
try { Set-MpPreference -SubmitSamplesConsent 2 -ErrorAction Stop } catch { Write-Warning $_ }
exit [Math]::Min($failed,255)
)PS";
    return run(ps,{L"-NoProfile",L"-NonInteractive",L"-ExecutionPolicy",L"Bypass",L"-Command",command},log,L"remove/deprovision selected AppX");
}
bool disableTelemetryTasks(core::Logger& log){const auto schtasks=core::windowsSystemExecutable(L"schtasks.exe");const wchar_t* tasks[]={L"\\Microsoft\\Windows\\Application Experience\\Microsoft Compatibility Appraiser",L"\\Microsoft\\Windows\\Application Experience\\ProgramDataUpdater",L"\\Microsoft\\Windows\\Customer Experience Improvement Program\\Consolidator",L"\\Microsoft\\Windows\\Customer Experience Improvement Program\\UsbCeip",L"\\Microsoft\\Windows\\Feedback\\Siuf\\DmClient",L"\\Microsoft\\Windows\\Feedback\\Siuf\\DmClientOnScenarioDownload",L"\\Microsoft\\Windows\\DiskDiagnostic\\Microsoft-Windows-DiskDiagnosticDataCollector"};bool ok=true;for(const auto* task:tasks)ok=run(schtasks,{L"/Change",L"/TN",task,L"/Disable"},log,L"disable task",{0,1})&&ok;return ok;}
bool applyNativePrivacy(core::Logger& log,bool includeApps,bool installEnforcement){
    bool ok=true;for(const auto& e:machinePolicies)ok=setDword(e.root,e.path,e.name,e.value,log)&&ok;ok=applyUserHive(HKEY_CURRENT_USER,L"",log)&&ok;const auto loadedSids=loadedUserSids();for(const auto& sid:loadedSids)ok=applyUserHive(HKEY_USERS,sid,log)&&ok;
    const auto reg=core::windowsSystemExecutable(L"reg.exe");unsigned mountIndex{};
    for(const auto& [sid,hiveFile]:profileHives()){if(std::find(loadedSids.begin(),loadedSids.end(),sid)!=loadedSids.end()||!std::filesystem::exists(hiveFile))continue;const std::wstring mount=L"ERSTEINRICHTUNG_USER_"+std::to_wstring(++mountIndex);run(reg,{L"unload",L"HKU\\"+mount},log,L"clear stale profile mount",{0,1});if(run(reg,{L"load",L"HKU\\"+mount,hiveFile.wstring()},log,L"load existing profile",{0})){ok=applyUserHive(HKEY_USERS,mount,log)&&ok;RegFlushKey(HKEY_USERS);ok=run(reg,{L"unload",L"HKU\\"+mount},log,L"unload existing profile",{0})&&ok;}else{log.write(L"PRIVACY could not load profile "+sid);ok=false;}}
    wchar_t systemDrive[MAX_PATH]{};GetEnvironmentVariableW(L"SystemDrive",systemDrive,MAX_PATH);const auto defaultHive=std::filesystem::path(systemDrive)/L"Users\\Default\\NTUSER.DAT";const std::wstring mount=L"ERSTEINRICHTUNG_DEFAULT";run(reg,{L"unload",L"HKU\\"+mount},log,L"clear stale default mount",{0,1});const bool loaded=run(reg,{L"load",L"HKU\\"+mount,defaultHive.wstring()},log,L"load default profile",{0});if(loaded){ok=applyUserHive(HKEY_USERS,mount,log)&&ok;RegFlushKey(HKEY_USERS);ok=run(reg,{L"unload",L"HKU\\"+mount},log,L"unload default profile",{0})&&ok;}else{log.write(L"PRIVACY default profile could not be loaded");ok=false;}
    ok=configureService(L"DiagTrack",log)&&ok;ok=configureService(L"WerSvc",log)&&ok;ok=disableTelemetryTasks(log)&&ok;if(includeApps)ok=removeSelectedApps(log)&&ok;
    if(installEnforcement){wchar_t programData[MAX_PATH]{};GetEnvironmentVariableW(L"ProgramData",programData,MAX_PATH);const auto installDir=std::filesystem::path(programData)/L"Ersteinrichtung";const auto installedExe=installDir/L"Ersteinrichtung.exe";std::error_code ec;std::filesystem::create_directories(installDir,ec);const auto sourceExe=std::filesystem::path(core::executableDirectory())/L"Ersteinrichtung.exe";std::filesystem::copy_file(sourceExe,installedExe,std::filesystem::copy_options::overwrite_existing,ec);if(ec){log.write(L"PRIVACY failed to install enforcement executable, error="+std::to_wstring(ec.value()));ok=false;}else{const auto schtasks=core::windowsSystemExecutable(L"schtasks.exe");const std::wstring command=L"\""+installedExe.wstring()+L"\" --privacy-enforce";ok=run(schtasks,{L"/Create",L"/TN",L"Ersteinrichtung Privacy Enforcement",L"/SC",L"ONSTART",L"/RU",L"SYSTEM",L"/RL",L"HIGHEST",L"/TR",command,L"/F"},log,L"create native privacy task")&&ok;}}
    return ok;
}
}
bool applyPrivacyEnforcement(core::Logger& logger){return applyNativePrivacy(logger,true,false);}
bool Actions::applyEnhancedPrivacy(){line(L"Applies supported computer policies, all loaded profiles and the Default profile for future users.",core::console::warning);line(L"It disables web/cloud search, telemetry and feedback controls, selected services/tasks, and deprovisions the selected AppX packages.",core::console::warning);line(L"A SYSTEM startup task calls this executable again to reapply the controls after Windows changes.",core::console::warning);if(!core::console::confirm(L"Apply enhanced computer-wide privacy and remove New Outlook, Mail/Calendar, LinkedIn, Solitaire, WhatsApp and Clipchamp?"))return false;const bool ok=applyNativePrivacy(logger_,true,true);line(ok?L"Computer-wide privacy was applied. Restart Windows to activate every policy.":L"Privacy was only partially applied. Inspect the session log for failed policies.",ok?core::console::good:core::console::warning);return ok;}
}
