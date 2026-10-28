#pragma once
#include <filesystem>
#include <initializer_list>
#include <string>
#include <utility>
namespace core::i18n {
bool initialize();
std::wstring tr(const std::string& key);
std::wstring format(const std::string& key,std::initializer_list<std::pair<std::wstring,std::wstring>> replacements);
const std::wstring& language();
const std::filesystem::path& loadedFile();
bool loaded();
}
