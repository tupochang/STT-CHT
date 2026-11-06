#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <mutex>
#include <algorithm>
extern "C" {
#include "cpu_state.h"
#include "psx_memory.h"
#include "func_override.h"
#include "host_overlay.h"
}
#include "core_panel.h"
#include "caption_progress.h"

namespace {
struct State {
    std::mutex mutex;
    std::vector<unsigned char> signature, pixels;
    std::wstring caption;
    std::vector<size_t> timeline;
    size_t visible=0,source_words=0;
    uint32_t base = 0;
    bool active = false;
    uint32_t owner_index = 0;
    bool loaded = false;
};
State& state() { static State s; return s; }
bool peek(uint32_t a, uint32_t& v) { return psx_peek_word(a, &v) != 0; }
bool match(uint32_t a, const std::vector<unsigned char>& signature) {
    uint32_t off;
    if (!psx_ram_resolve(a, static_cast<uint32_t>(signature.size()), &off)) return false;
    for (size_t i=0;i<signature.size();++i) {
        uint32_t word;
        if (!peek((a+static_cast<uint32_t>(i))&~3u, word) ||
            ((word >> (((a+i)&3)*8))&255) != signature[i]) return false;
    }
    return true;
}
void load(State& s) {
    s.loaded = true;
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr,exe,MAX_PATH);
    std::wstring dir(exe);dir.resize(dir.find_last_of(L"\\/")+1);
    std::ifstream sig(std::filesystem::path(dir+L"p1-data\\signature.bin"),std::ios::binary);
    s.signature.assign(std::istreambuf_iterator<char>(sig),{});
    std::ifstream txt(std::filesystem::path(dir+L"p1-data\\caption.txt"),std::ios::binary);
    std::string utf8((std::istreambuf_iterator<char>(txt)),{});
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),static_cast<int>(utf8.size()),nullptr,0);
    if(s.signature.empty() || s.signature.size()>2048 || count<=0) {
        std::fprintf(stderr,"STT_P1 data invalid or missing; Japanese fallback\n");s.signature.clear();return;
    }
    s.caption.resize(count);
    MultiByteToWideChar(CP_UTF8,0,utf8.data(),static_cast<int>(utf8.size()),s.caption.data(),count);
    s.caption.erase(std::remove(s.caption.begin(),s.caption.end(),L'\r'),s.caption.end());
    try{s.timeline=stt_caption::timeline(s.signature,s.caption);}
    catch(const std::exception& e){std::fprintf(stderr,"STT_P1 progress map rejected: %s\n",e.what());s.signature.clear();return;}
    std::fprintf(stderr,"STT_P1 loaded U00208 signature=%zu\n",s.signature.size());
}
void raster(State& s) {
    constexpr int W=640,H=176;
    HDC dc=CreateCompatibleDC(nullptr); BITMAPINFO bi{};
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=W;
    bi.bmiHeader.biHeight=-H;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    void* bits=nullptr; HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bm) {DeleteDC(dc);return;}
    auto old=SelectObject(dc,bm);
    RECT area{0,0,W,H};HBRUSH brush=CreateSolidBrush(RGB(18,24,30));FillRect(dc,&area,brush);DeleteObject(brush);
    HFONT font=CreateFontW(-28,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Microsoft JhengHei");
    auto oldFont=SelectObject(dc,font);SetTextColor(dc,RGB(255,255,255));SetBkMode(dc,TRANSPARENT);
    RECT text{18,10,W-18,H-8};DrawTextW(dc,s.caption.c_str(),static_cast<int>(s.visible),&text,DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);
    GdiFlush();s.pixels.resize(W*H*4);auto src=static_cast<unsigned char*>(bits);
    for(size_t i=0;i<s.pixels.size();i+=4){s.pixels[i]=src[i+2];s.pixels[i+1]=src[i+1];s.pixels[i+2]=src[i];s.pixels[i+3]=255;}
    SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);
}
void draw(int width,int height,void*) {
    auto& s=state();std::lock_guard<std::mutex> lock(s.mutex);
    if(!s.active) return;
    if(s.pixels.empty()) raster(s);
    if(s.pixels.empty()) return;
    GLboolean incomingStencil=glIsEnabled(GL_STENCIL_TEST),incomingMask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK,incomingMask);
    static PanelGL panel;
    if(!panel.image(width,height,s.pixels.data()))return;
    static bool reported=false;
    if(!reported) {std::fprintf(stderr,"STT_P1 panel submitted %dx%d incoming_stencil=%d mask=%d%d%d%d\n",width,height,
        incomingStencil,incomingMask[0],incomingMask[1],incomingMask[2],incomingMask[3]);reported=true;}
}
int refresh(CPUState*) {
    auto& s=state();
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if(!s.loaded) load(s);
        if(s.signature.empty()) return 0;
        bool found=false;
        for(unsigned slot=0;slot<3;++slot) {
            uint32_t table=0x800c6b88+slot*44,flags=0,modeWord=0,ptr=0;
            if(!peek(table,flags)||!(flags&1)||!peek(table+0x10,modeWord)||!peek(table+0x20,ptr))continue;
            unsigned mode=modeWord&255;
            if(ptr<0x800e0000 || ptr>0x80100000)continue;
            uint32_t base=0;
            if(s.base && ptr>=s.base && ptr-s.base<=s.signature.size() && match(s.base,s.signature))base=s.base;
            else for(size_t back=0;back<=s.signature.size();back+=2)
                if(ptr>=back && match(ptr-static_cast<uint32_t>(back),s.signature)){base=ptr-static_cast<uint32_t>(back);break;}
            if(!base)continue;
            auto sample=stt_caption::sample(base,s.signature.size(),flags,mode,ptr);
            if(!sample.visible || sample.words>=s.timeline.size())continue;
            size_t visible=s.timeline[sample.words];
            if(!s.active || s.base!=base || s.visible!=visible || s.source_words!=sample.words)
                std::fprintf(stderr,"STT_P1 progress slot=%u mode=%u ptr=%08x words=%zu chars=%zu\n",slot,mode,ptr,sample.words,visible);
            if(s.visible!=visible || !s.active)s.pixels.clear();
            s.active=true;s.base=base;s.owner_index=slot;s.visible=visible;s.source_words=sample.words;found=true;break;
        }
        if(!found && s.active){std::fprintf(stderr,"STT_P1 cleared inactive/replaced dialogue\n");s.active=false;s.base=0;s.visible=0;s.pixels.clear();}
    }
    psx_host_set_hud_draw_cb(draw,nullptr);
    return 0; // Original guest function and its cycle accounting execute unchanged.
}
struct Register {
    Register() {
        int a=func_override_add("stt.p1.update",0x8004a1c4,refresh,0);
        int b=func_override_add("stt.p1.dialogue",0x8004b92c,refresh,0);
        std::fprintf(stderr,"STT_P1 hooks=%d,%d\n",a,b);
    }
} registration;
}
