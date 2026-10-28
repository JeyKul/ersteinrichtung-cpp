#include "console.hpp"
#include <algorithm>
#include <cwctype>
#include <iostream>

namespace core::console {
namespace {
void setConsoleSize(short requestedWidth = 140, short requestedHeight = 45) {
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    const COORD largest = GetLargestConsoleWindowSize(out);
    if (largest.X <= 0 || largest.Y <= 0) return;
    const short width = (std::min)(requestedWidth, largest.X);
    const short height = (std::min)(requestedHeight, largest.Y);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(out, &info)) return;
    COORD buffer{(std::max)(info.dwSize.X, width), (std::max)(info.dwSize.Y, height)};
    SetConsoleScreenBufferSize(out, buffer);
    SMALL_RECT window{0, 0, static_cast<short>(width - 1), static_cast<short>(height - 1)};
    SetConsoleWindowInfo(out, TRUE, &window);
}
}
void initialize() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"Ersteinrichtung");
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode{};
    if (GetConsoleMode(out, &mode)) SetConsoleMode(out, mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    setConsoleSize();
    color(normal);
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
    FillConsoleOutputAttribute(out, normal, cells, origin, &written);
    SetConsoleCursorPosition(out, origin);
}
void write(const std::wstring& text, WORD value) {
    color(value); std::wcout << text; color(normal);
}
void line(const std::wstring& text, WORD value) { write(text + L"\n", value); }
void separator(const std::wstring& label, WORD value, int width) {
    if (label.empty()) { line(std::wstring(static_cast<std::size_t>(width), L'-'), value); return; }
    const int remaining = (std::max)(0, width - static_cast<int>(label.size()));
    const int left = remaining / 2;
    const int right = remaining - left;
    line(std::wstring(static_cast<std::size_t>(left), L'-') + label + std::wstring(static_cast<std::size_t>(right), L'-'), value);
}
std::wstring prompt(const std::wstring& label) {
    write(label, highlight);
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
