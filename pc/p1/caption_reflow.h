#pragma once
#include <string>
#include <vector>
namespace stt_caption {
inline bool latin_word(wchar_t c){return (c>=L'A'&&c<=L'Z')||(c>=L'a'&&c<=L'z')||(c>=L'0'&&c<=L'9')||c==L'-'||c==L'_';}
inline bool closes_line(wchar_t c){return std::wstring(L"，。、！？：；）］】』」…,.!?;:)]").find(c)!=std::wstring::npos;}
template<class Measure>bool reflow(const std::wstring& text,int width,unsigned max_lines,Measure measure,std::wstring& result){
    if(width<=0||max_lines==0)return false;
    std::vector<std::wstring> lines(1);for(wchar_t c:text){if(c==L'\n')lines.emplace_back();else if(c!=L'\r')lines.back()+=c;}
    bool fits=lines.size()<=max_lines;for(const auto& line:lines)if(measure(line)>width)fits=false;
    if(fits){result=text;return true;}
    // Reflow only within this original page; never invent an extra input wait.
    std::wstring flat;for(wchar_t c:text){if(c==L'\r')continue;if(c==L'\n')c=L' ';if(c==L' '&&(flat.empty()||flat.back()==L' '))continue;flat+=c;}
    lines.clear();size_t start=0;
    while(start<flat.size()){
        while(start<flat.size()&&flat[start]==L' ')++start;if(start==flat.size())break;
        size_t end=start,last_break=start;
        while(end<flat.size()){
            size_t next=end+1;if(flat[end]>=0xd800&&flat[end]<=0xdbff&&next<flat.size())++next;
            if(measure(flat.substr(start,next-start))>width)break;
            end=next;if(end==flat.size()||!latin_word(flat[end-1])||!latin_word(flat[end]))last_break=end;
        }
        if(end==start)return false;
        if(end<flat.size()&&last_break>start)end=last_break;
        if(end<flat.size()&&closes_line(flat[end])&&end>start+1)--end;
        if(end>start&&flat[end-1]>=0xd800&&flat[end-1]<=0xdbff)--end;
        if(end==start)return false;
        auto line=flat.substr(start,end-start);while(!line.empty()&&line.back()==L' ')line.pop_back();
        lines.push_back(line);if(lines.size()>max_lines)return false;start=end;
    }
    if(lines.empty())return false;result.clear();for(size_t i=0;i<lines.size();++i){if(i)result+=L'\n';result+=lines[i];}return true;
}
}
