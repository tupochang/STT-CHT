#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <cstdio>
#include <algorithm>
#include "psx_sdl.h"
extern "C" {
#include "host_overlay.h"
#include "psx_memory.h"
#include "func_override.h"
#include "cpu_state.h"
int psx_presentation_fast_forward(void);
}
#include "core_panel.h"
#include "cheat_engine.h"
namespace {
struct Controls {std::mutex mutex;bool open=false,loaded=false;size_t selected=0;std::vector<stt_cheats::Cheat> cheats;};
Controls& controls(){static Controls c;return c;}
void load(Controls& c){if(c.loaded)return;c.loaded=true;wchar_t path[MAX_PATH];GetModuleFileNameW(nullptr,path,MAX_PATH);
    std::ifstream f(std::filesystem::path(path).parent_path()/L"stt2001.cht");c.cheats=stt_cheats::parse(f);
    std::fprintf(stderr,"STT_PC cheats loaded=%zu (default off)\n",c.cheats.size());}
std::wstring wide(const std::string& t){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,t.data(),int(t.size()),nullptr,0);
    std::wstring w(n,L' ');if(n)MultiByteToWideChar(CP_UTF8,0,t.data(),int(t.size()),w.data(),n);return w;}
void draw_menu(int width,int height,void*){
    auto& c=controls();std::lock_guard<std::mutex> lock(c.mutex);if(!c.open)return;
    constexpr int W=800,H=620;HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth=W;bi.bmiHeader.biHeight=-H;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void* bits=nullptr;
    HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!bitmap){DeleteDC(dc);return;}
    auto old=SelectObject(dc,bitmap);RECT rect{0,0,W,H};HBRUSH bg=CreateSolidBrush(RGB(16,24,34));FillRect(dc,&rect,bg);DeleteObject(bg);
    HFONT font=CreateFontW(-25,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Microsoft JhengHei");auto oldFont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);
    auto line=[&](int y,const std::wstring& text,COLORREF color){SetTextColor(dc,color);TextOutW(dc,22,y,text.data(),int(text.size()));};
    line(15,L"STT2001 功能選單 · F1 / Esc 返回",RGB(255,222,110));
    line(53,psx_presentation_fast_forward()?L"加速：開　｜　返回遊戲後按空白鍵切換":L"加速：關　｜　返回遊戲後按空白鍵切換",RGB(220,230,240));
    line(92,L"金手指　↑↓ 選擇，Enter 切換",RGB(180,210,250));
    for(size_t i=0;i<c.cheats.size()&&i<11;++i){const auto& item=c.cheats[i];
        std::wstring label=(i==c.selected?L"> ":L"  ")+std::wstring(!item.supported?L"[不支援] ":item.enabled?L"[開] ":L"[關] ")+wide(item.name);
        line(135+int(i)*32,label,i==c.selected?RGB(255,230,100):item.supported?RGB(240,240,240):RGB(140,150,160));}
    line(510,L"金手指預設關閉；本次開關不會寫入設定。",RGB(170,185,200));
    line(544,L"關閉會停止持續寫入，已改變的數值仍保留。",RGB(170,185,200));
    line(578,L"部分原版程式碼型金手指暫不支援。",RGB(170,185,200));
    GdiFlush();std::vector<unsigned char> pixels(W*H*4);auto src=static_cast<unsigned char*>(bits);
    for(size_t i=0;i<pixels.size();i+=4){pixels[i]=src[i+2];pixels[i+1]=src[i+1];pixels[i+2]=src[i];pixels[i+3]=255;}
    SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
    static PanelGL panel;int w=std::min(width,static_cast<int>(height*W/double(H))),h=w*H/W;
    panel.image(width,height,pixels.data(),W,H,(width-w)/2,(height-h)/2,w,h);
}
int input(const void* data,void*){
    const auto& e=*static_cast<const SDL_Event*>(data);auto& c=controls();
    if(e.type==SDL_KEYDOWN && (e.key.key==SDLK_F1 || (e.key.key==SDLK_ESCAPE&&c.open))){
        if(e.key.repeat)return 1;
        if(c.open){ {std::lock_guard<std::mutex> lock(c.mutex);c.open=false;}psx_host_overlay_set_draw_cb(nullptr,nullptr);psx_host_set_ui_capture(0);psx_host_pause_pop();}
        else if(psx_host_pause_push("STT2001 function menu")){ {std::lock_guard<std::mutex> lock(c.mutex);load(c);c.open=true;}psx_host_set_ui_capture(1);psx_host_overlay_set_draw_cb(draw_menu,nullptr);}
        return 1;
    }
    if(!c.open)return 0;
    if(e.type==SDL_KEYDOWN){std::lock_guard<std::mutex> lock(c.mutex);if(!c.cheats.empty()){
        if(e.key.key==SDLK_DOWN)c.selected=(c.selected+1)%c.cheats.size();
        if(e.key.key==SDLK_UP)c.selected=(c.selected+c.cheats.size()-1)%c.cheats.size();
        if(e.key.key==SDLK_RETURN&&!e.key.repeat&&c.cheats[c.selected].supported)c.cheats[c.selected].enabled=!c.cheats[c.selected].enabled;}}
    // Window and quit events still reach the runtime.
    return e.type==SDL_KEYDOWN||e.type==SDL_KEYUP||e.type==SDL_CONTROLLERBUTTONDOWN||e.type==SDL_CONTROLLERBUTTONUP;
}
int apply(CPUState*){
    auto& c=controls();std::lock_guard<std::mutex> lock(c.mutex);if(!c.loaded)return 0;
    stt_cheats::apply(c.cheats,[](uint32_t a,uint16_t& v){uint32_t w=0;if(!psx_peek_word(a&~3u,&w))return false;v=uint16_t(w>>((a&3)*8));return true;},
        [](uint32_t a,uint32_t v,unsigned bytes){if(bytes==4)psx_host_write_word(a,v);else if(bytes==2)psx_host_write_half(a,uint16_t(v));else psx_host_write_byte(a,uint8_t(v));});return 0;
}
struct Register {Register(){psx_host_set_input_sink(input,nullptr);}} registration;
}
extern "C" void stt_pc_controls_frame(){apply(nullptr);}
