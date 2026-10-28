#pragma once
#include <windows.h>
#include <string>

namespace core::console {
constexpr WORD normal = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD accent = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD good = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD warning = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD error = FOREGROUND_RED | FOREGROUND_INTENSITY;

void initialize();
void clear();
void color(WORD value);
void write(const std::wstring& text, WORD value = normal);
void line(const std::wstring& text = {}, WORD value = normal);
std::wstring prompt(const std::wstring& label);
bool yesNo(const std::wstring& label, bool defaultNo = true);
bool confirmExact(const std::wstring& phrase);
void pause();
}
