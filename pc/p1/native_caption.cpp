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
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <climits>
extern "C" {
#include "cpu_state.h"
#include "psx_memory.h"
#include "func_override.h"
#include "host_overlay.h"
void stt_pc_controls_frame(void);
}
#include "core_panel.h"
#include "caption_progress.h"
#include "caption_layout.h"
#include "caption_pages.h"
#include "caption_reflow.h"
#include "caption_names.h"

namespace {
struct Entry {std::string uid;std::vector<unsigned char> raw;std::wstring text;std::vector<size_t> timeline;std::vector<std::vector<unsigned char>> contexts;uint32_t header_words=0;std::vector<stt_caption::Page> pages;};
struct State {
    std::mutex mutex;
    std::vector<unsigned char> signature, pixels;
    std::wstring caption;
    std::vector<size_t> timeline;
    size_t visible=0,source_words=0;
    uint32_t base = 0;
    bool active = false;
    bool presentation_visible=true;
    uint32_t owner_index = 0;
    bool loaded = false;
    bool renderer_ready=false,in_frame=false;
    int text_x=0,text_y=0,lines=4,raster_w=640,raster_h=176;
    std::vector<Entry> entries;
    std::unordered_map<uint64_t,std::vector<size_t>> index;
    size_t entry_index=SIZE_MAX,max_signature=0;
    size_t page_index=0;
    std::unordered_set<std::string> rejected;
    float tint[3]={1,1,1};
    unsigned fixed_slot=0;
    std::unordered_map<uint16_t,wchar_t> name_glyphs;
    std::vector<size_t> source_offsets;
    std::wstring name_key;
};
State& primary_state() { static State s; return s; }
State& extra_state(unsigned slot){static State extra[2];return extra[slot-1];}
thread_local State* current_slot_state=nullptr;
State& state() {return current_slot_state?*current_slot_state:primary_state();}
struct SlotScope {State* previous;SlotScope(State& s):previous(current_slot_state){current_slot_state=&s;}~SlotScope(){current_slot_state=previous;}};
PanelGL& panel_state(){static PanelGL panel;return panel;}
void warmup(int,int,void*) {
    auto& s=state();bool ready;
    {std::lock_guard<std::mutex> lock(s.mutex);ready=s.renderer_ready=panel_state().init();}
    if(ready){std::fprintf(stderr,"STT_P1 core font renderer ready before dialogue\n");psx_host_overlay_set_draw_cb(nullptr,nullptr);}
}
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
uint64_t prefix(const std::vector<unsigned char>& raw) {
    uint64_t key=0;for(unsigned i=0;i<8;++i)key|=uint64_t(raw[i])<<(8*i);return key;
}
bool match_entry(uint32_t base,const Entry& e){
    if(base<128 || !match(base,e.raw))return false;
    for(const auto& context:e.contexts)if(match(base-uint32_t(context.size()),context))return true;
    return false;
}
bool fits(const std::wstring& text,int x) {
    HDC dc=CreateCompatibleDC(nullptr);
    HFONT font=CreateFontW(-28,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Microsoft JhengHei");
    auto old=SelectObject(dc,font);bool ok=true;size_t start=0;
    while(start<text.size()){size_t end=text.find(L'\n',start);if(end==std::wstring::npos)end=text.size();SIZE size{};
        if(!GetTextExtentPoint32W(dc,text.data()+start,static_cast<int>(end-start),&size)||size.cx>(304-x)*2)ok=false;start=end+1;}
    SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);return ok;
}
bool layout_page(const std::wstring& text,int x,unsigned lines,std::wstring& output){
    HDC dc=CreateCompatibleDC(nullptr);HFONT font=CreateFontW(-28,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Microsoft JhengHei");
    auto old=SelectObject(dc,font);
    bool result=stt_caption::reflow(text,(304-x)*2,lines,[&](const std::wstring& t){SIZE size{};return GetTextExtentPoint32W(dc,t.data(),int(t.size()),&size)?size.cx:INT_MAX;},output);
    SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);return result;
}
void load(State& s) {
    s.loaded = true;
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr,exe,MAX_PATH);
    std::wstring dir(exe);dir.resize(dir.find_last_of(L"\\/")+1);
    auto batch_path=std::filesystem::path(dir+L"pc-data-v5\\dialogues.bin");
    if(!std::filesystem::exists(batch_path))batch_path=std::filesystem::path(dir+L"p1-data\\dialogues.bin");
    std::ifstream batch(batch_path,std::ios::binary);
    if(batch) {
        char magic[8];uint32_t count=0;batch.read(magic,8);batch.read(reinterpret_cast<char*>(&count),4);
        if(!batch || std::string(magic,8)!="STTB0005"||count>50000){std::fprintf(stderr,"STT_P1 invalid batch header\n");return;}
        std::ifstream names(batch_path.parent_path()/L"name-glyphs.bin",std::ios::binary);uint32_t name_count=0;names.read(reinterpret_cast<char*>(&name_count),4);
        if(name_count<=65536)for(uint32_t j=0;j<name_count;++j){uint16_t code=0;uint32_t cp=0;names.read(reinterpret_cast<char*>(&code),2);names.read(reinterpret_cast<char*>(&cp),4);if(!names||cp>65535)break;s.name_glyphs[code]=wchar_t(cp);}
        for(uint32_t i=0;i<count;++i){uint32_t n=0,r=0,t=0;
            batch.read(reinterpret_cast<char*>(&n),4);batch.read(reinterpret_cast<char*>(&r),4);batch.read(reinterpret_cast<char*>(&t),4);
            if(!batch||n>32||r<8||r>2048||r%2||t>16384){s.entries.clear();s.index.clear();return;}
            Entry e;e.uid.resize(n);e.raw.resize(r);std::string utf8(t,'\0');
            batch.read(e.uid.data(),n);batch.read(reinterpret_cast<char*>(e.raw.data()),r);batch.read(utf8.data(),t);
            uint32_t contexts=0;batch.read(reinterpret_cast<char*>(&e.header_words),4);batch.read(reinterpret_cast<char*>(&contexts),4);
            if(!batch||contexts==0||contexts>50000){s.entries.clear();s.index.clear();return;}
            for(uint32_t j=0;j<contexts;++j){std::vector<unsigned char> c(128);batch.read(reinterpret_cast<char*>(c.data()),128);e.contexts.push_back(std::move(c));}
            int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),t,nullptr,0);
            if(!batch||size<=0){s.entries.clear();s.index.clear();return;}
            e.text.resize(size);MultiByteToWideChar(CP_UTF8,0,utf8.data(),t,e.text.data(),size);
            try{e.pages=stt_caption::pages(e.raw,e.text,e.header_words);}catch(...){s.entries.clear();s.index.clear();return;}
            if(e.pages.size()==1){try{e.timeline=stt_caption::timeline(e.raw,e.text,e.header_words);}catch(...){}}
            s.max_signature=std::max(s.max_signature,e.raw.size());s.index[prefix(e.raw)].push_back(s.entries.size());s.entries.push_back(std::move(e));
        }
        std::fprintf(stderr,"STT_P1 batch loaded records=%zu\n",s.entries.size());return;
    }
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
    int W=s.in_frame?(304-s.text_x)*2:640,H=s.in_frame?s.lines*32:176;
    s.raster_w=W;s.raster_h=H;
    HDC dc=CreateCompatibleDC(nullptr); BITMAPINFO bi{};
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=W;
    bi.bmiHeader.biHeight=-H;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    void* bits=nullptr; HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bm) {DeleteDC(dc);return;}
    auto old=SelectObject(dc,bm);
    RECT area{0,0,W,H};HBRUSH brush=CreateSolidBrush(s.in_frame?RGB(0,0,0):RGB(18,24,30));FillRect(dc,&area,brush);DeleteObject(brush);
    HFONT font=CreateFontW(-28,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Microsoft JhengHei");
    auto oldFont=SelectObject(dc,font);SetTextColor(dc,RGB(255,255,255));SetBkMode(dc,TRANSPARENT);
    if(s.in_frame) {
        auto text=s.caption.substr(0,s.visible);size_t start=0;int line=0;
        while(start<text.size()){size_t end=text.find(L'\n',start);if(end==std::wstring::npos)end=text.size();
            TextOutW(dc,0,line*32,text.data()+start,static_cast<int>(end-start));start=end+1;++line;}
    } else {
        RECT text{18,10,W-18,H-8};DrawTextW(dc,s.caption.c_str(),static_cast<int>(s.visible),&text,DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);
    }
    GdiFlush();s.pixels.resize(W*H*4);auto src=static_cast<unsigned char*>(bits);
    for(size_t i=0;i<s.pixels.size();i+=4){s.pixels[i]=src[i+2];s.pixels[i+1]=src[i+1];s.pixels[i+2]=src[i];s.pixels[i+3]=s.in_frame?std::max({src[i],src[i+1],src[i+2]}):255;}
    SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);
}
void draw_slot(int width,int height,void*) {
    auto& s=state();std::lock_guard<std::mutex> lock(s.mutex);
    auto& panel=panel_state();
    s.renderer_ready=panel.init();
    if(!s.renderer_ready)return;
    if(!s.active || !s.presentation_visible || (s.tint[0]==0 && s.tint[1]==0 && s.tint[2]==0)) return;
    if(s.pixels.empty()) raster(s);
    if(s.pixels.empty()) return;
    GLboolean incomingStencil=glIsEnabled(GL_STENCIL_TEST),incomingMask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK,incomingMask);
    if(s.in_frame) {
        auto r=stt_caption::fit_rect(width,height,s.text_x,s.text_y,304-s.text_x,s.lines*16);
        if(!panel.image(width,height,s.pixels.data(),s.raster_w,s.raster_h,r.x,r.y,r.w,r.h,true,s.tint[0],s.tint[1],s.tint[2]))return;
    } else if(!panel.image(width,height,s.pixels.data()))return;
    static bool reported=false;
    if(!reported) {std::fprintf(stderr,"STT_P1 panel submitted %dx%d incoming_stencil=%d mask=%d%d%d%d\n",width,height,
        incomingStencil,incomingMask[0],incomingMask[1],incomingMask[2],incomingMask[3]);reported=true;}
}
void draw(int width,int height,void*){
    {SlotScope scope(primary_state());draw_slot(width,height,nullptr);}
    for(unsigned slot=1;slot<3;++slot){SlotScope scope(extra_state(slot));draw_slot(width,height,nullptr);}
}
int refresh_slot(CPUState*) {
    auto& s=state();
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if(!s.loaded) load(s);
        if(s.signature.empty() && s.entries.empty()) return 0;
        bool found=false;
        for(unsigned slot=0;slot<3;++slot) {
            if(slot!=s.fixed_slot)continue;
            uint32_t table=0x800c6b88+slot*44,flags=0,modeWord=0,ptr=0;
            if(!peek(table,flags)||!(flags&1)||!peek(table+0x10,modeWord)||!peek(table+0x20,ptr))continue;
            unsigned mode=modeWord&255;
            if(mode!=0 && mode!=1 && mode!=4 && mode!=5)continue;
            auto name_pointer=stt_caption::name_pointer(peek,table,ptr);ptr=name_pointer.source;
            if(ptr<0x80010000 || ptr>0x80200000)continue;
            uint32_t base=0;
            size_t selected=SIZE_MAX;
            if(!s.entries.empty()) {
                if(s.active && s.owner_index==slot && s.entry_index<s.entries.size()){
                    const auto& e=s.entries[s.entry_index];
                    if(stt_caption::sample(s.base,e.raw.size(),flags,mode,ptr).visible && match_entry(s.base,e)){base=s.base;selected=s.entry_index;}
                }
                if(!base)for(size_t back=0;back<=s.max_signature && ptr>=back;back+=2){
                    uint32_t a=ptr-static_cast<uint32_t>(back);
                    // Source strings are halfword aligned; aligned word reads reconstruct the prefix.
                    std::vector<unsigned char> bytes(8);bool valid=true;
                    for(unsigned j=0;j<8;++j){uint32_t word=0;if(!peek((a+j)&~3u,word)){valid=false;break;}bytes[j]=(word>>(((a+j)&3)*8))&255;}
                    if(!valid)continue;
                    auto it=s.index.find(prefix(bytes));if(it==s.index.end())continue;
                    for(size_t k:it->second){const auto& e=s.entries[k];
                        if(stt_caption::sample(a,e.raw.size(),flags,mode,ptr).visible && match_entry(a,e)){base=a;selected=k;break;}}
                    if(base)break;
                }
            } else {
                if(s.base && ptr>=s.base && ptr-s.base<=s.signature.size() && match(s.base,s.signature))base=s.base;
                else for(size_t back=0;back<=s.signature.size();back+=2)
                    if(ptr>=back && match(ptr-static_cast<uint32_t>(back),s.signature)){base=ptr-static_cast<uint32_t>(back);break;}
            }
            if(!base)continue;
            const auto& raw=selected<s.entries.size()?s.entries[selected].raw:s.signature;
            auto sample=stt_caption::sample(base,raw.size(),flags,mode,ptr);
            if(!sample.visible)continue;
            uint32_t xy=0;peek(table+4,xy);int x=static_cast<int16_t>(xy>>16);
            peek(table+8,xy);int y=static_cast<int16_t>(xy&65535);
            size_t page=0,local_words=sample.words;bool named=false;
            stt_caption::ResolvedPage resolved;
            if(selected<s.entries.size()){
                const auto& e=s.entries[selected];page=stt_caption::page_index(e.pages,sample.words,mode);
                local_words=sample.words>e.pages[page].begin?sample.words-e.pages[page].begin:0;
                named=e.header_words&&page==0;if(named)y+=16;
                if(!stt_caption::resolve_page(e.pages[page],peek,s.name_glyphs,resolved)){
                    std::string key=e.uid+":name";if(s.rejected.size()<512&&s.rejected.insert(key).second)std::fprintf(stderr,"STT_P1 rejected %s reason=name_unavailable\n",e.uid.c_str());continue;
                }
            }
            bool changed=!s.active || s.base!=base || s.entry_index!=selected || s.owner_index!=slot || s.page_index!=page || s.text_x!=x || s.text_y!=y || s.name_key!=resolved.key;
            if(selected<s.entries.size() && (!s.renderer_ready || !stt_caption::valid_text_origin(x,y,1)))continue;
            if(changed) {
                if(selected<s.entries.size()){
                    const auto& e=s.entries[selected];std::wstring laid_out;
                    unsigned max_lines=std::min(unsigned(4-named),unsigned((240-y)/16));
                    std::string key=e.uid+":"+std::to_string(page)+":"+std::to_string(x)+":"+std::to_string(max_lines)+":"+std::to_string(std::hash<std::wstring>{}(resolved.key));
                    if(s.rejected.count(key))continue;
                    if(!layout_page(resolved.page.text,x,max_lines,laid_out)){
                        if(s.rejected.size()<512&&s.rejected.insert(key).second)std::fprintf(stderr,"STT_P1 rejected %s page=%zu reason=page_capacity x=%d y=%d lines=%u\n",e.uid.c_str(),page,x,y,max_lines);
                        continue;
                    }
                    s.signature=e.raw;s.caption=laid_out;s.timeline=stt_caption::page_timeline(resolved.page,laid_out);s.source_offsets=resolved.offsets;s.name_key=resolved.key;
                }
                s.pixels.clear();
                s.lines=1+int(std::count(s.caption.begin(),s.caption.end(),L'\n'));s.entry_index=selected;s.page_index=page;
                s.presentation_visible=true;
                s.in_frame=s.renderer_ready && stt_caption::valid_text_origin(x,y,s.lines);
                std::fprintf(stderr,"STT_P1 layout slot=%u x=%d y=%d in_frame=%d renderer=%d\n",slot,x,y,s.in_frame,s.renderer_ready);
                if(selected<s.entries.size())std::fprintf(stderr,"STT_P1 matched %s base=%08x page=%zu/%zu reflow=%d\n",s.entries[selected].uid.c_str(),base,page+1,s.entries[selected].pages.size(),s.caption!=s.entries[selected].pages[page].text);
            }
            if(selected<s.entries.size()){
                if(local_words>=s.source_offsets.size())continue;local_words=s.source_offsets[local_words];
                if(name_pointer.variable>=0 && sample.words>=s.entries[selected].pages[page].begin)local_words+=name_pointer.typed;
            }
            if(local_words>=s.timeline.size())continue;
            size_t visible=s.timeline[local_words];
            if(s.text_x!=x || s.text_y!=y)s.pixels.clear();s.text_x=x;s.text_y=y;
            if(!s.active || s.base!=base || s.visible!=visible || s.source_words!=sample.words)
                std::fprintf(stderr,"STT_P1 progress slot=%u mode=%u ptr=%08x words=%zu chars=%zu\n",slot,mode,ptr,sample.words,visible);
            if(s.visible!=visible || !s.active)s.pixels.clear();
            s.active=true;s.base=base;s.owner_index=slot;s.visible=visible;s.source_words=sample.words;found=true;break;
        }
        if(!found && s.active){std::fprintf(stderr,"STT_P1 cleared inactive/replaced dialogue\n");s.active=false;s.base=0;s.visible=0;s.pixels.clear();}
    }
    return 0; // Original guest function and its cycle accounting execute unchanged.
}
int refresh(CPUState* cpu){
    {SlotScope scope(primary_state());refresh_slot(cpu);}
    bool renderer_ready;{std::lock_guard<std::mutex> lock(primary_state().mutex);renderer_ready=primary_state().renderer_ready;}
    for(unsigned slot=1;slot<3;++slot){
        auto& extra=extra_state(slot);uint32_t flags=0;bool active;
        {std::lock_guard<std::mutex> lock(extra.mutex);active=extra.active;extra.fixed_slot=slot;extra.renderer_ready=renderer_ready;}
        if(active || (peek(0x800c6b88+slot*44,flags)&&(flags&1))){SlotScope scope(extra);refresh_slot(cpu);}
    }
    psx_host_set_hud_draw_cb(draw,nullptr);return 0;
}
int frame_sync(CPUState* cpu) {
    stt_pc_controls_frame();
    // This entry runs after scene updates, even when dialogue updates stop.
    refresh(cpu);
    auto& s=state();std::lock_guard<std::mutex> lock(s.mutex);
    uint32_t enabled=0,mode=0;const uint32_t gp=cpu->gpr[28];
    float tint[3]={1,1,1};
    // Mode 3 targets a draw-object interval, not the whole screen. Do not
    // extrapolate its color to a caption without knowing the target objects.
    if(peek(gp+0x2c0,enabled)&&enabled && peek((gp+0x2f6)&~3u,mode) && ((mode>>16)&65535)!=3) {
        for(unsigned i=0;i<3;++i){uint32_t a=gp+0x2b8+i*2,v=0;
            if(peek(a&~3u,v))tint[i]=((v>>((a&3)*8))&255)/128.0f;}
    }
    if(s.active && (s.tint[0]!=tint[0]||s.tint[1]!=tint[1]||s.tint[2]!=tint[2]))
        std::fprintf(stderr,"STT_P1 frame tint=%.3f,%.3f,%.3f\n",tint[0],tint[1],tint[2]);
    std::copy(tint,tint+3,s.tint);
    for(unsigned slot=1;slot<3;++slot){auto& extra=extra_state(slot);std::lock_guard<std::mutex> guard(extra.mutex);std::copy(tint,tint+3,extra.tint);}
    return 0;
}
int glyph(CPUState* cpu) {
    unsigned owner=cpu->gpr[21]&255;if(owner>=3)return 0;
    bool suppress;{auto& s=owner?extra_state(owner):primary_state();std::lock_guard<std::mutex> lock(s.mutex);
        suppress=s.active&&s.in_frame&&s.renderer_ready && (cpu->gpr[21]&255)==s.owner_index;
        if(suppress && s.entry_index<s.entries.size() && s.entries[s.entry_index].header_words){
            uint32_t ptr=0;peek(0x800c6b88+s.owner_index*44+0x20,ptr);
            ptr=stt_caption::name_pointer(peek,0x800c6b88+s.owner_index*44,ptr).source;
            suppress=ptr>=s.base+s.entries[s.entry_index].header_words*2;
        }}
    uint32_t primitive=cpu->gpr[4],offset=0;
    if(!suppress || cpu->gpr[31]!=0x8004bdb0 || !psx_ram_resolve(primitive,32,&offset))return 0;
    // Lookup and timing execute normally. Only sprite height is hidden; width
    // at +8 still drives the original cursor, so text pacing/layout is intact.
    func_override_call_original(cpu);
    psx_host_write_half(primitive+0xa,0);
    return 1;
}
int presentation(CPUState* cpu) {
    // Prologue script's original hide branch selects draw group 4 and skips
    // dialogue updates; its release branch selects group 1 and resumes them.
    bool hiding=cpu->gpr[31]==0x80021df8 && cpu->gpr[4]==4;
    bool showing=cpu->gpr[31]==0x80021e08 && cpu->gpr[4]==1;
    if(!hiding&&!showing)return 0;
    for(unsigned slot=0;slot<3;++slot){auto& s=slot?extra_state(slot):primary_state();std::lock_guard<std::mutex> lock(s.mutex);
        if(s.active && s.presentation_visible!=showing) {
            s.presentation_visible=showing;
            std::fprintf(stderr,"STT_P1 presentation visible=%d slot=%u chars=%zu\n",showing,slot,s.visible);
        }
    }
    return 0; // The original draw-list selection still executes.
}
struct Register {
    Register() {
        // P1 has no launcher/menu overlay provider. Warm once on the GL thread,
        // then release the provider slot; regular text stays in the HUD layer.
        psx_host_overlay_set_draw_cb(warmup,nullptr);
        int a=func_override_add("stt.p1.update",0x8004a1c4,refresh,0);
        int b=func_override_add("stt.p1.dialogue",0x8004b92c,refresh,0);
        int c=func_override_add("stt.p1.glyph",0x8004a7d8,glyph,FO_CREDIT_SELF);
        int d=func_override_add("stt.p1.presentation",0x80016de0,presentation,0);
        int e=func_override_add("stt.p1.frame",0x800150e8,frame_sync,0);
        std::fprintf(stderr,"STT_P1 hooks=%d,%d,%d,%d,%d\n",a,b,c,d,e);
    }
} registration;
}
