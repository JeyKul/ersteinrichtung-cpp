#include "console.hpp"
#include <algorithm>
#include <cwctype>
#include <iostream>

namespace core::console {
void initialize() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"Ersteinrichtung");
}
void color(WORD value) { SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), value); }
void clear() {
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(out, &info)) return;
    const DWORD cells = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
    DWORD written{};
    const COORD origin{};
    FillConsoleOutputCharacterW(out, L' ', cells, origin, &written);
    FillConsoleOutputAttribute(out, info.wAttributes, cells, origin, &written);
    SetConsoleCursorPosition(out, origin);
}
void write(const std::wstring& text, WORD value) {
    color(value); std::wcout << text; color(normal);
}
void line(const std::wstring& text, WORD value) { write(text + L"\n", value); }
std::wstring prompt(const std::wstring& label) {
    write(label, accent);
    std::wstring value;
    std::getline(std::wcin, value);
    while (!value.empty() && std::iswspace(value.back())) value.pop_back();
    auto first = std::find_if_not(value.begin(), value.end(), [](wchar_t c) { return std::iswspace(c); });
    value.erase(value.begin(), first);
    return value;
}
bool yesNo(const std::wstring& label, bool defaultNo) {
    const auto answer = prompt(label + (defaultNo ? L" [y/N]: " : L" [Y/n]: "));
    if (answer.empty()) return !defaultNo;
    return answer == L"y" || answer == L"Y" || answer == L"j" || answer == L"J";
}
bool confirmExact(const std::wstring& phrase) {
    line(L"This action changes the system.", warning);
    return prompt(L"Type exactly [" + phrase + L"]: ") == phrase;
}
void pause() { (void)prompt(L"\nPress Enter to continue..."); }
}
