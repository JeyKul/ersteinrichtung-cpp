#include <windows.h>
#include <lmcons.h>
#include <array>
#include <cstdlib>
#include <cwctype>
#include <iostream>
#include <limits>
#include <string>
#include "bitlocker/bitlocker.hpp"
namespace {
constexpr WORD kNormal=FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE;
constexpr WORD kAccent=FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY;
void setColor(WORD c){SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),c);}
void clear(){HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE); CONSOLE_SCREEN_BUFFER_INFO i{}; if(!GetConsoleScreenBufferInfo(h,&i))return; DWORD n=0,cells=static_cast<DWORD>(i.dwSize.X)*i.dwSize.Y; COORD p{0,0}; FillConsoleOutputCharacterW(h,L' ',cells,p,&n);FillConsoleOutputAttribute(h,i.wAttributes,cells,p,&n);SetConsoleCursorPosition(h,p);}
bool admin(){HANDLE t=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&t))return false;TOKEN_ELEVATION e{};DWORD n=0;BOOL ok=GetTokenInformation(t,TokenElevation,&e,sizeof(e),&n);CloseHandle(t);return ok&&e.TokenIsElevated;}
void header(){setColor(kAccent);std::wcout<<L"\n  ERSTEINRICHTUNG  |  Native C++ prototype\n"<<std::wstring(72,L'=')<<L"\n";setColor(kNormal);std::wcout<<L"  Admin: "<<(admin()?L"Yes":L"No")<<L"\n"<<std::wstring(72,L'-')<<L"\n";}
void placeholder(const wchar_t* label){clear();header();std::wcout<<L"  "<<label<<L" is not implemented yet.\n\nPress Enter to return...";std::wstring s;std::getline(std::wcin,s);}
}
int wmain(){SetConsoleOutputCP(CP_UTF8);SetConsoleTitleW(L"Ersteinrichtung");if(!admin()){std::wcerr<<L"Run as administrator.\n";return 1;} for(;;){clear();header();setColor(kAccent);std::wcout<<L"  MAIN MENU\n\n";setColor(kNormal);std::wcout<<L"  [1] Run initial setup\n  [2] Windows Update\n  [3] Tools\n  [4] Extras\n  [5] BitLocker\n  [0] Exit\n\nSelect an option: ";std::wstring input;if(!std::getline(std::wcin,input))return 0;if(input==L"0"||input==L"q")return 0;if(input==L"5")bitlocker::menu();else if(input==L"1"||input==L"2"||input==L"3"||input==L"4")placeholder(L"This feature");}}
