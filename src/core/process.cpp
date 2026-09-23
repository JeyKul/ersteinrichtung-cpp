#include <windows.h>
#include "process.hpp"
#include <vector>
namespace core {
Result run(const std::wstring& exe, const std::vector<std::wstring>& args) {
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE}; HANDLE read = nullptr, write = nullptr;
  if (!CreatePipe(&read, &write, &sa, 0)) return {1,L"",L"CreatePipe failed"};
  SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
  std::wstring cmd=L"\""+exe+L"\""; for (const auto& arg: args) cmd+=L" "+arg;
  std::vector<wchar_t> buffer(cmd.begin(),cmd.end()); buffer.push_back(0);
  STARTUPINFOW si{sizeof(si)}; si.dwFlags=STARTF_USESTDHANDLES; si.hStdOutput=write; si.hStdError=write; si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION pi{}; BOOL ok=CreateProcessW(exe.c_str(),buffer.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi);
  CloseHandle(write);
  if (!ok) { auto code=GetLastError(); CloseHandle(read); return {code,L"",L"CreateProcessW failed: "+std::to_wstring(code)}; }
  std::string bytes; char chunk[4096]; DWORD n=0; while (ReadFile(read,chunk,sizeof(chunk),&n,nullptr) && n) bytes.append(chunk,n);
  CloseHandle(read); WaitForSingleObject(pi.hProcess,INFINITE); DWORD code=1; GetExitCodeProcess(pi.hProcess,&code); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
  int length=MultiByteToWideChar(CP_OEMCP,0,bytes.data(),static_cast<int>(bytes.size()),nullptr,0);
  std::wstring text(length,L' '); if(length) MultiByteToWideChar(CP_OEMCP,0,bytes.data(),static_cast<int>(bytes.size()),text.data(),length);
  return {code,text,L""};
}
}
