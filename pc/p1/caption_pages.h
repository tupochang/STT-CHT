#pragma once
#include "caption_progress.h"
namespace stt_caption {
struct Page {size_t begin=0,end=0;std::wstring text;std::vector<unsigned char> raw;};
inline std::vector<Page> pages(const std::vector<unsigned char>& raw,const std::wstring& text,size_t header){
    if(raw.size()%2||header>=raw.size()/2)throw std::runtime_error("Invalid page source");
    std::vector<std::wstring> translations;size_t start=0;
    while(true){auto end=text.find(L"<p>",start);translations.push_back(text.substr(start,end==std::wstring::npos?end:end-start));if(end==std::wstring::npos)break;start=end+3;}
    std::vector<Page> out;size_t begin=header;
    for(size_t i=header;i<raw.size()/2;++i){unsigned code=raw[i*2]|raw[i*2+1]<<8;
        if(code>=0xff00&&code!=0xfffb&&code!=0xfffc&&code!=0xfffd&&code!=0xfffe)throw std::runtime_error("Unsupported page control");
        if(code==0xfffc||code==0xfffd||code==0xfffe){
            if(out.size()>=translations.size())throw std::runtime_error("Page count differs");
            Page p;p.begin=begin;p.end=i+1;p.text=translations[out.size()];p.raw.assign(raw.begin()+begin*2,raw.begin()+(i+1)*2);
            // Reuse the ordinary timing mapper within a single page.
            p.raw[p.raw.size()-2]=0xfd;p.raw.back()=0xff;out.push_back(std::move(p));begin=i+1;
            if(code!=0xfffc&&i+1!=raw.size()/2)throw std::runtime_error("Trailing page data");
        }
    }
    if(out.size()!=translations.size()||begin!=raw.size()/2)throw std::runtime_error("Page count differs");
    return out;
}
inline size_t page_index(const std::vector<Page>& pages,size_t words,unsigned mode){
    for(size_t i=0;i<pages.size();++i){
        if(words<pages[i].end || (words==pages[i].end && (mode==1 || i+1==pages.size())))return i;
    }
    throw std::runtime_error("Pointer beyond pages");
}
inline std::vector<size_t> page_timeline(const Page& page,const std::wstring& text){
    try{return timeline(page.raw,text);}catch(const std::runtime_error&){}
    size_t total=0;for(size_t i=0;i<page.raw.size();i+=2)if((page.raw[i]|page.raw[i+1]<<8)<0xff00)++total;
    if(!total)throw std::runtime_error("Empty source page");
    std::vector<size_t> out{0};size_t consumed=0;
    for(size_t i=0;i<page.raw.size();i+=2){unsigned c=page.raw[i]|page.raw[i+1]<<8;
        if(c<0xff00)++consumed;
        size_t n=c==0xfffd?text.size():consumed*text.size()/total;
        if(n>0&&n<text.size()&&text[n-1]>=0xd800&&text[n-1]<=0xdbff)--n;
        out.push_back(n);
    }
    return out;
}
}
