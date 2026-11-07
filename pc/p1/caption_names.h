#pragma once
#include <array>
#include <unordered_map>
#include "caption_pages.h"
namespace stt_caption {
constexpr uint32_t name_offsets[4]={0x24e,0x272,0x260,0x27e};
constexpr unsigned name_limits[4]={8,5,8,5};
struct NamePointer {uint32_t source=0;size_t typed=0;int variable=-1;};
template<class Peek>bool half(Peek peek,uint32_t address,uint16_t& value){uint32_t word=0;if(!peek(address&~3u,word))return false;value=uint16_t(word>>((address&3)*8));return true;}
template<class Peek>NamePointer name_pointer(Peek peek,uint32_t table,uint32_t pointer){
    uint32_t config=0,saved=0;if(!peek(0x800bd350,config)||config<0x80010000||config>0x801ff000||!peek(table+0x24,saved))return {pointer};
    for(unsigned i=0;i<4;++i){uint32_t begin=config+name_offsets[i];uint16_t code=0;
        if(pointer>=begin&&pointer<=begin+name_limits[i]*2 && !((pointer-begin)&1) && half(peek,saved,code)&&code==0xff01+i)
            return {saved,(pointer-begin)/2,int(i)};
    }
    return {pointer};
}
struct ResolvedPage {Page page;std::vector<size_t> offsets;std::wstring key;};
template<class Peek>bool resolve_page(const Page& page,Peek peek,const std::unordered_map<uint16_t,wchar_t>& glyphs,ResolvedPage& out){
    out={};out.page=page;out.page.raw.clear();out.offsets.push_back(0);
    for(size_t p=0;p<page.raw.size();p+=2){unsigned c=page.raw[p]|page.raw[p+1]<<8;
        if(c>=0xff01&&c<=0xff04){
            unsigned i=c-0xff01;uint32_t config=0;if(!peek(0x800bd350,config)||config<0x80010000||config>0x801ff000)return false;
            std::wstring name;size_t glyph_count=0;
            // The guest writes FF05 into the fixed end slot on first use.
            // Before that write, decode only the proven 8/5-character field.
            for(unsigned j=0;j<name_limits[i];++j){uint16_t code=0;if(!half(peek,config+name_offsets[i]+j*2,code))return false;
                if(code==0xff05)break;auto it=glyphs.find(code);if(it==glyphs.end())return false;name+=it->second;++glyph_count;}
            while(!name.empty()&&name.back()==L' ')name.pop_back();
            if(name.empty())return false;
            std::wstring token=L"[HERO"+std::to_wstring(i+1)+L"]";size_t pos=0;bool replaced=false;
            while((pos=out.page.text.find(token,pos))!=std::wstring::npos){out.page.text.replace(pos,token.size(),name);pos+=name.size();replaced=true;}
            // A repeated source variable may have had all text occurrences replaced already.
            (void)replaced;out.key+=std::to_wstring(i)+L":"+name+L":"+std::to_wstring(glyph_count)+L";";
            for(size_t n=0;n<glyph_count;++n){out.page.raw.push_back(1);out.page.raw.push_back(0);}
        }else{out.page.raw.push_back(c&255);out.page.raw.push_back(c>>8);}
        out.offsets.push_back(out.page.raw.size()/2);
    }
    return out.page.text.find(L"[HERO")==std::wstring::npos;
}
}
