$ErrorActionPreference = 'Continue'
$failures = 0
function Set-Dword([string]$Path,[string]$Name,[int]$Value) {
    try {
        New-Item -Path $Path -Force | Out-Null
        New-ItemProperty -Path $Path -Name $Name -Value $Value -PropertyType DWord -Force | Out-Null
        $actual = Get-ItemPropertyValue -Path $Path -Name $Name -ErrorAction Stop
        if ($actual -ne $Value) { throw "verification returned $actual" }
        Write-Host "OK $Path\$Name=$Value"
    } catch { $script:failures++; Write-Warning "FAIL $Path\$Name : $_" }
}
$machine = @(
 @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\DataCollection','AllowTelemetry',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\DataCollection','AllowTelemetry',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\DataCollection','DoNotShowFeedbackNotifications',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\DataCollection','LimitDiagnosticLogCollection',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\DataCollection','LimitDumpCollection',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\DataCollection','DisableOneSettingsDownloads',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\System','PublishUserActivities',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\System','UploadUserActivities',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\System','EnableActivityFeed',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\System','AllowCrossDeviceClipboard',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\System','EnableCdp',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\Explorer','DisableSearchBoxSuggestions',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\Windows Search','AllowCloudSearch',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\Windows Search','ConnectedSearchUseWeb',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\Windows Search','DisableWebSearch',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\CloudContent','DisableWindowsConsumerFeatures',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\CloudContent','DisableConsumerAccountStateContent',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\CloudContent','DisableCloudOptimizedContent',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\AdvertisingInfo','DisabledByGroupPolicy',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\AppCompat','AITEnable',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\AppCompat','DisableInventory',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\LocationAndSensors','DisableLocation',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\Maps','AutoDownloadAndUpdateMapData',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\OneDrive','DisableFileSyncNGSC',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Windows\Windows Error Reporting','Disabled',1),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Edge','PersonalizationReportingEnabled',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Edge','UserFeedbackAllowed',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Edge','DiagnosticData',0),
 @('HKLM:\SOFTWARE\Policies\Microsoft\Edge','SearchSuggestEnabled',0)
)
foreach ($i in $machine) { Set-Dword $i[0] $i[1] $i[2] }
$users = Get-ChildItem Registry::HKEY_USERS -ErrorAction SilentlyContinue | Where-Object { $_.PSChildName -match '^S-1-5-21-' -and $_.PSChildName -notmatch '_Classes$' } | Select-Object -ExpandProperty PSChildName
$loadedDefault = $false
if (-not (Test-Path Registry::HKEY_USERS\ERSTEINRICHTUNG_DEFAULT)) {
    reg.exe load HKU\ERSTEINRICHTUNG_DEFAULT "$env:SystemDrive\Users\Default\NTUSER.DAT" | Out-Null
    $loadedDefault = $LASTEXITCODE -eq 0
}
$users += @('.DEFAULT','ERSTEINRICHTUNG_DEFAULT')
$userValues = @(
 @('Software\Microsoft\Windows\CurrentVersion\AdvertisingInfo','Enabled',0),
 @('Software\Microsoft\Windows\CurrentVersion\Privacy','TailoredExperiencesWithDiagnosticDataEnabled',0),
 @('Software\Microsoft\Speech_OneCore\Settings\OnlineSpeechPrivacy','HasAccepted',0),
 @('Software\Microsoft\Input\TIPC','Enabled',0),
 @('Software\Microsoft\InputPersonalization','RestrictImplicitInkCollection',1),
 @('Software\Microsoft\InputPersonalization','RestrictImplicitTextCollection',1),
 @('Software\Microsoft\InputPersonalization\TrainedDataStore','HarvestContacts',0),
 @('Software\Microsoft\Personalization\Settings','AcceptedPrivacyPolicy',0),
 @('Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced','Start_TrackProgs',0),
 @('Software\Microsoft\Windows\CurrentVersion\Search','BingSearchEnabled',0),
 @('Software\Microsoft\Windows\CurrentVersion\Search','CortanaConsent',0),
 @('Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager','SilentInstalledAppsEnabled',0),
 @('Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager','ContentDeliveryAllowed',0),
 @('Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager','SystemPaneSuggestionsEnabled',0),
 @('Software\Microsoft\Siuf\Rules','NumberOfSIUFInPeriod',0)
)
foreach ($sid in ($users | Select-Object -Unique)) { foreach ($i in $userValues) { Set-Dword "Registry::HKEY_USERS\$sid\$($i[0])" $i[1] $i[2] } }
if ($loadedDefault) { [gc]::Collect(); Start-Sleep -Milliseconds 250; reg.exe unload HKU\ERSTEINRICHTUNG_DEFAULT | Out-Null }
$targets = @('Microsoft.OutlookForWindows','microsoft.windowscommunicationsapps','Microsoft.MicrosoftSolitaireCollection','Clipchamp.Clipchamp','7EE7776C.LinkedInforWindows','5319275A.WhatsAppDesktop')
foreach ($pattern in $targets) {
    Get-AppxPackage -AllUsers -Name $pattern -ErrorAction SilentlyContinue | ForEach-Object { try { Remove-AppxPackage -Package $_.PackageFullName -AllUsers -ErrorAction Stop } catch { $failures++; Write-Warning $_ } }
    Get-AppxProvisionedPackage -Online | Where-Object { $_.DisplayName -like $pattern -or $_.PackageName -like "*$pattern*" } | ForEach-Object { try { Remove-AppxProvisionedPackage -Online -AllUsers -PackageName $_.PackageName -ErrorAction Stop | Out-Null } catch { $failures++; Write-Warning $_ } }
}
Set-MpPreference -SubmitSamplesConsent 2 -ErrorAction SilentlyContinue
foreach ($service in 'DiagTrack','WerSvc') { Stop-Service $service -Force -ErrorAction SilentlyContinue; Set-Service $service -StartupType Disabled -ErrorAction SilentlyContinue }
$tasks = @('\Microsoft\Windows\Application Experience\Microsoft Compatibility Appraiser','\Microsoft\Windows\Customer Experience Improvement Program\Consolidator','\Microsoft\Windows\Customer Experience Improvement Program\UsbCeip','\Microsoft\Windows\Feedback\Siuf\DmClient','\Microsoft\Windows\Feedback\Siuf\DmClientOnScenarioDownload')
foreach ($task in $tasks) { $leaf=$task.Split('\')[-1];$parent=$task.Substring(0,$task.LastIndexOf('\')+1);Disable-ScheduledTask -TaskName $leaf -TaskPath $parent -ErrorAction SilentlyContinue | Out-Null }
$enforce = "$env:ProgramData\Ersteinrichtung\privacy-enforce.ps1"
New-Item -ItemType Directory -Path (Split-Path $enforce) -Force | Out-Null
$payload = @'
$targets=@('Microsoft.OutlookForWindows','microsoft.windowscommunicationsapps','Microsoft.MicrosoftSolitaireCollection','Clipchamp.Clipchamp','7EE7776C.LinkedInforWindows','5319275A.WhatsAppDesktop')
foreach($p in $targets){Get-AppxPackage -AllUsers -Name $p -ErrorAction SilentlyContinue|%{Remove-AppxPackage -Package $_.PackageFullName -AllUsers -ErrorAction SilentlyContinue};Get-AppxProvisionedPackage -Online|?{$_.DisplayName-like$p-or$_.PackageName-like"*$p*"}|%{Remove-AppxProvisionedPackage -Online -AllUsers -PackageName $_.PackageName -ErrorAction SilentlyContinue|Out-Null}}
$items=@(@('HKLM:\SOFTWARE\Policies\Microsoft\Windows\DataCollection','AllowTelemetry',0),@('HKLM:\SOFTWARE\Policies\Microsoft\Windows\Explorer','DisableSearchBoxSuggestions',1),@('HKLM:\SOFTWARE\Policies\Microsoft\Windows\CloudContent','DisableWindowsConsumerFeatures',1),@('HKLM:\SOFTWARE\Policies\Microsoft\Windows\Windows Search','AllowCloudSearch',0),@('HKLM:\SOFTWARE\Policies\Microsoft\Windows\Windows Search','DisableWebSearch',1),@('HKLM:\SOFTWARE\Policies\Microsoft\Windows\System','PublishUserActivities',0),@('HKLM:\SOFTWARE\Policies\Microsoft\Windows\System','UploadUserActivities',0))
foreach($i in$items){New-Item -Path $i[0]-Force|Out-Null;New-ItemProperty -Path $i[0]-Name $i[1]-Value $i[2]-PropertyType DWord -Force|Out-Null}
foreach($s in'DiagTrack','WerSvc'){Stop-Service $s -Force -ErrorAction SilentlyContinue;Set-Service $s -StartupType Disabled -ErrorAction SilentlyContinue}
'@
Set-Content -LiteralPath $enforce -Value $payload -Encoding UTF8
$action=New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-NoProfile -ExecutionPolicy Bypass -File `"$enforce`""
$trigger=New-ScheduledTaskTrigger -AtStartup
$principal=New-ScheduledTaskPrincipal -UserId SYSTEM -LogonType ServiceAccount -RunLevel Highest
Register-ScheduledTask -TaskName 'Ersteinrichtung Privacy Enforcement' -Action $action -Trigger $trigger -Principal $principal -Force | Out-Null
if ($failures) { Write-Warning "$failures privacy operations failed"; exit 2 }
