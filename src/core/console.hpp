#pragma once
#include <windows.h>
#include <string>
namespace core::console {
constexpr WORD background=0,menu=FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE,title=menu|FOREGROUND_INTENSITY,highlight=title,normal=menu,accent=title,good=FOREGROUND_GREEN|FOREGROUND_INTENSITY,warning=FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY,error=FOREGROUND_RED|FOREGROUND_INTENSITY;
void initialize();void clear();void color(WORD);void write(const std::wstring&,WORD=normal);void line(const std::wstring& text = {},WORD=normal);void separator(const std::wstring& text = {},WORD=title,int=47);std::wstring prompt(const std::wstring&);bool yesNo(const std::wstring&,bool=true);bool confirm(const std::wstring&,bool=true);void pause();
}
