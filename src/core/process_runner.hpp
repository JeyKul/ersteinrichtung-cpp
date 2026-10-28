#pragma once
#include <functional>
#include <string>
#include <vector>
namespace core {struct ProcessResult{unsigned long exitCode{1};std::wstring output,error;bool started{false};};struct ProcessOptions{bool captureOutput{true},showWindow{false},wait{true};std::wstring workingDirectory;std::function<void(const std::wstring&)> onOutput;};ProcessResult runProcess(const std::wstring&,const std::vector<std::wstring>& args = {}, const ProcessOptions& options = {});std::wstring findExecutable(const std::wstring&);bool shellOpen(const std::wstring&,const std::wstring& arguments = {});}
