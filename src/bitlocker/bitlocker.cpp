#include "bitlocker.hpp"
#include "../core/process.hpp"
#include <windows.h>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
namespace bitlocker {
namespace {
std::wstring drive=L"C:";
std::wstring manageBde() { wchar_t dir[MAX_PATH]{}; UINT n=GetSystemDirectoryW(dir,MAX_PATH); return n && n<MAX_PATH ? std::wstring(dir)+L"\manage-bde.exe" : L""; }
void pause() { std::wcout<<L"\nPress Enter to continue..."; std::wstring s; std::getline(std::wcin,s); }
std::wstring prompt(const wchar_t* label) { std::wcout<<label; std::wstring s; std::getline(std::wcin,s); return s; }
bool confirm(const std::wstring& verb) { auto phrase=verb+L" "+drive; std::wcout<<L"\nType exactly ["<<phrase<<L"] to confirm: "; std::wstring answer; std::getline(std::wcin,answer); return answer==phrase; }
core::Result command(const std::vector<std::wstring>& args, bool sensitive=false) { auto exe=manageBde(); if(exe.empty()) return {1,L"",L"Cannot locate manage-bde.exe"}; auto r=core::run(exe,args); if(!r.error.empty()) std::wcout<<L"\n"<<r.error; if(!sensitive) std::wcout<<L"\n"<<r.output; else std::wcout<<L"\nSensitive protector output hidden. Use export to save it securely."; std::wcout<<L"\nExit code: "<<r.exitCode<<L"\n"; return r; }
void status() { command({L"-status"}); pause(); }
void selectDrive() { std::wcout<<L"\nMounted drives:\n"; DWORD mask=GetLogicalDrives(); for(wchar_t c=L'A';c<=L'Z';++c) if(mask & (1UL<<(c-L'A'))) { std::wstring root{c,L':',L'\'}; if(GetDriveTypeW(root.c_str())==DRIVE_FIXED) std::wcout<<L"  "<<c<<L":\n"; } auto input=prompt(L"Select fixed drive letter: " ); if(input.size()!=1 && !(input.size()==2 && input[1]==L':')) return; wchar_t c=std::towupper(input[0]); if(c<L'A'||c>L'Z'||!(mask & (1UL<<(c-L'A')))) return; std::wstring root{c,L':',L'\'}; if(GetDriveTypeW(root.c_str())!=DRIVE_FIXED) return; drive={c,L':'}; }
void mutate(const std::wstring& verb, const std::vector<std::wstring>& args, bool sensitive=false) { if(confirm(verb)) { command(args,sensitive); pause(); } }
void crypt() { command({L"-status",drive}); auto c=prompt(L"[o] Encrypt  [f] Decrypt  [Enter] Cancel: " ); if(c==L"o") mutate(L"ENCRYPT",{L"-on",drive,L"-recoverypassword",L"-usedspaceonly"},true); if(c==L"f") mutate(L"DECRYPT",{L"-off",drive}); }
void pauseResume() { auto c=prompt(L"[p] Pause encryption  [r] Resume encryption: " ); if(c==L"p") mutate(L"PAUSE",{L"-pause",drive}); if(c==L"r") mutate(L"RESUME",{L"-resume",drive}); }
void autoUnlock() { wchar_t os[MAX_PATH]{}; GetWindowsDirectoryW(os,MAX_PATH); wchar_t osDrive=std::towupper(os[0]); if(drive[0]==osDrive){std::wcout<<L"\nAuto-unlock is unavailable for the OS drive.\n";pause();return;} auto c=prompt(L"[e] Enable auto-unlock [d] Disable: " ); if(c==L"e") mutate(L"ENABLE-AUTOUNLOCK",{L"-autounlock",L"-enable",drive}); if(c==L"d") mutate(L"DISABLE-AUTOUNLOCK",{L"-autounlock",L"-disable",drive}); }
void protectors() { std::wcout<<L"\nRecovery passwords are secrets; export only to a safe destination.\n"; auto c=prompt(L"[e] Export to file  [Enter] Cancel: " ); if(c!=L"e")return; if(!confirm(L"EXPORT-RECOVERY"))return; auto path=prompt(L"Absolute destination file path (not inside encrypted target volume): " ); if(path.empty()) return; std::filesystem::path p(path); if(!p.is_absolute()){std::wcout<<L"Absolute path required.\n";pause();return;} auto r=command({L"-protectors",L"-get",drive},true); if(r.exitCode==0){std::ofstream file(p,std::ios::binary|std::ios::trunc); int len=WideCharToMultiByte(CP_UTF8,0,r.output.data(),static_cast<int>(r.output.size()),nullptr,0,nullptr,nullptr); std::string utf8(len,'\0'); if(len) WideCharToMultiByte(CP_UTF8,0,r.output.data(),static_cast<int>(r.output.size()),utf8.data(),len,nullptr,nullptr); file.write(utf8.data(),utf8.size()); std::wcout<<(file.good()?L"Exported. Store it securely outside the VM.":L"Export failed."); } pause(); }
void addRecovery() { mutate(L"ADD-RECOVERY",{L"-protectors",L"-add",drive,L"-rp"},true); std::wcout<<L"Export your recovery key using option 6 before rebooting.\n"; }
}
void menu() { for(;;) { std::wcout<<L"\n=============================================\n   BITLOCKER  |  Current drive: "<<drive<<L"\n=============================================\n  1  Overview\n  2  Select drive\n  3  Encrypt / decrypt\n  4  Pause / resume\n  5  Auto-unlock (data drive)\n  6  Export recovery protectors\n  7  Add recovery password\n  11 Upgrade metadata\n  12 Wipe free space\n  0  Back\nSelect: "; std::wstring c; if(!std::getline(std::wcin,c))return; if(c==L"0")return; if(c==L"1")status(); else if(c==L"2")selectDrive(); else if(c==L"3")crypt(); else if(c==L"4")pauseResume(); else if(c==L"5")autoUnlock(); else if(c==L"6")protectors(); else if(c==L"7")addRecovery(); else if(c==L"11")mutate(L"UPGRADE",{L"-upgrade",drive}); else if(c==L"12")mutate(L"WIPE-FREE-SPACE",{L"-wipefreespace",drive}); } }
}
