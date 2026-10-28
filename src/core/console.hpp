#pragma once
#include <windows.h>
#include <string>

namespace core::console {
constexpr WORD background = 0;
constexpr WORD menu = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD title = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD highlight = title;
constexpr WORD normal = menu;
constexpr WORD accent = title;
constexpr WORD good = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD warning = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD error = FOREGROUND_RED | FOREGROUND_INTENSITY;

void initialize();
void clear();
void color(WORD value);
void write(const std::wstring& text, WORD value = normal);
void line(const std::wstring& text = {}, WORD value = normal);
void separator(const std::wstring& label = {}, WORD value = title, int width = 47);
std::wstring prompt(const std::wstring& label);
bool yesNo(const std::wstring& label, bool defaultNo = true);
bool confirmExact(const std::wstring& phrase);
void pause();
}
