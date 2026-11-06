#pragma once
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>
namespace stt_caption {
// Plain single-page dialogue. Unknown controls decline rather than guessing.
inline std::vector<size_t> timeline(const std::vector<unsigned char>& raw,const std::wstring& text,size_t header_words=0) {
    if(raw.empty() || raw.size()%2) throw std::runtime_error("Invalid source words");
    if(header_words>=raw.size()/2)throw std::runtime_error("Invalid speaker header");
    if(header_words){
        std::vector<unsigned char> body(raw.begin()+header_words*2,raw.end());
        auto map=timeline(body,text);std::vector<size_t> out(header_words,0);out.insert(out.end(),map.begin(),map.end());return out;
    }
    std::vector<std::wstring> lines(1);
    for(wchar_t c:text){if(c==L'\n')lines.emplace_back();else if(c!=L'\r')lines.back()+=c;}
    std::vector<size_t> lengths(1,0);
    for(size_t i=0;i<raw.size();i+=2){unsigned code=raw[i]|(raw[i+1]<<8);
        if(code==0xfffb)lengths.push_back(0);
        else if(code==0xfffd || code==0xfffe){if(i+2!=raw.size())throw std::runtime_error("Unexpected source tail");}
        else if(code<0xff00)++lengths.back();
        else throw std::runtime_error("Unsupported source control");}
    if(lines.size()!=lengths.size())throw std::runtime_error("Source/translation line count differs");
    std::vector<size_t> result{0};size_t line=0,consumed=0,offset=0;
    for(size_t i=0;i<raw.size();i+=2){unsigned code=raw[i]|(raw[i+1]<<8);
        if(code==0xfffb){offset+=lines[line].size()+1;++line;consumed=0;result.push_back(offset);}
        else if(code==0xfffd || code==0xfffe)result.push_back(text.size());
        else{++consumed;size_t n=offset+consumed*lines[line].size()/lengths[line];
            // Do not split a UTF-16 surrogate pair on Windows.
            if(n>0 && n<text.size() && text[n-1]>=0xd800 && text[n-1]<=0xdbff)--n;
            result.push_back(n);}}
    return result;
}
struct Sample {bool visible=false;size_t words=0;};
inline Sample sample(uint32_t base,size_t size,uint32_t flags,unsigned mode,uint32_t pointer) {
    if(!(flags&1) || (mode!=0 && mode!=1 && mode!=4 && mode!=5) || pointer<base || ((pointer-base)&1))return {};
    size_t delta=pointer-base;
    if(delta>size || (delta==size && mode==0))return {};
    return {true,delta/2};
}
}
