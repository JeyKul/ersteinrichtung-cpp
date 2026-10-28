#include "i18n.hpp"
#include "system.hpp"
#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <map>
#include <vector>
namespace core::i18n { namespace {
std::map<std::string,std::wstring> strings; std::wstring currentLanguage=L"en-US"; std::filesystem::path currentFile;
std::wstring utf8ToWide(const std::string& v){if(v.empty())return{};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.data(),static_cast<int>(v.size()),nullptr,0);if(!n)return{};std::wstring r(static_cast<size_t>(n),L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.data(),static_cast<int>(v.size()),r.data(),n);return r;}
std::wstring trim(std::wstring v){auto a=std::find_if_not(v.begin(),v.end(),[](wchar_t c){return std::iswspace(c);});auto b=std::find_if_not(v.rbegin(),v.rend(),[](wchar_t c){return std::iswspace(c);}).base();return a>=b?L"":std::wstring(a,b);}
std::wstring unescape(const std::wstring& v){std::wstring r;for(size_t i=0;i<v.size();++i){if(v[i]!=L'\\'||i+1>=v.size()){r+=v[i];continue;}wchar_t n=v[++i];r+=n==L'n'?L'\n':n==L'r'?L'\r':n==L't'?L'\t':n;}return r;}
bool load(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);if(!f)return false;std::string b((std::istreambuf_iterator<char>(f)),{});if(b.size()>=3&&(unsigned char)b[0]==0xef&&(unsigned char)b[1]==0xbb&&(unsigned char)b[2]==0xbf)b.erase(0,3);std::wstring t=utf8ToWide(b);if(t.empty()&&!b.empty())return false;std::map<std::string,std::wstring> m;size_t s=0;while(s<=t.size()){auto e=t.find(L'\n',s);auto l=trim(t.substr(s,e==std::wstring::npos?t.size()-s:e-s));if(!l.empty()&&l[0]!=L'#'&&l[0]!=L';'){auto q=l.find(L'=');if(q!=std::wstring::npos){auto wk=trim(l.substr(0,q));std::string key;key.reserve(wk.size());for(const wchar_t c:wk){if(c>L'\x7f')return false;key.push_back(static_cast<char>(c));}m[std::move(key)]=unescape(trim(l.substr(q+1)));}}if(e==std::wstring::npos)break;s=e+1;}if(m.empty())return false;strings=std::move(m);currentFile=p;return true;}
}
bool initialize(){strings.clear();currentFile.clear();wchar_t value[LOCALE_NAME_MAX_LENGTH]{};DWORD n=GetEnvironmentVariableW(L"ERSTEINRICHTUNG_LANG",value,LOCALE_NAME_MAX_LENGTH);if(n&&n<LOCALE_NAME_MAX_LENGTH)currentLanguage.assign(value,n);else if(!GetUserDefaultLocaleName(value,LOCALE_NAME_MAX_LENGTH))currentLanguage=L"en-US";else currentLanguage=value;std::vector<std::wstring> c{currentLanguage};if(currentLanguage.rfind(L"de",0)==0&&currentLanguage!=L"de-DE")c.push_back(L"de-DE");if(currentLanguage.rfind(L"en",0)==0&&currentLanguage!=L"en-US")c.push_back(L"en-US");c.push_back(L"en-US");for(auto& x:c)for(auto& root:{std::filesystem::path(executableDirectory())/L"lang",std::filesystem::current_path()/L"lang"})if(load(root/(x+L".lang"))){currentLanguage=x;return true;}return false;}
std::wstring tr(const std::string& k){auto i=strings.find(k);return i==strings.end()?L"["+std::wstring(k.begin(),k.end())+L"]":i->second;}
std::wstring format(const std::string& k,std::initializer_list<std::pair<std::wstring,std::wstring>> rs){auto r=tr(k);for(auto& [a,b]:rs){size_t p=0;while((p=r.find(a,p))!=std::wstring::npos){r.replace(p,a.size(),b);p+=b.size();}}return r;}
const std::wstring& language(){return currentLanguage;} const std::filesystem::path& loadedFile(){return currentFile;} bool loaded(){return !strings.empty();}
}
