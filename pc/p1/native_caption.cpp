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
extern "C" {
#include "cpu_state.h"
#include "psx_memory.h"
#include "func_override.h"
#include "host_overlay.h"
}

namespace {
struct State {
    std::mutex mutex;
    std::vector<unsigned char> signature, pixels;
    std::wstring caption;
    uint32_t base = 0;
    bool active = false;
    bool owner_general = false;
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
    RECT text{18,10,W-18,H-8};DrawTextW(dc,s.caption.c_str(),static_cast<int>(s.caption.size()),&text,DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);
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
    using UseProgram = void (APIENTRY*)(GLuint);
    using ActiveTexture = void (APIENTRY*)(GLenum);
    auto useProgram=reinterpret_cast<UseProgram>(wglGetProcAddress("glUseProgram"));
    auto activeTexture=reinterpret_cast<ActiveTexture>(wglGetProcAddress("glActiveTexture"));
    GLint oldProgram=0,oldActiveTexture=0;
    if(useProgram) {glGetIntegerv(0x8B8D,&oldProgram);useProgram(0);}
    if(activeTexture) {glGetIntegerv(0x84E0,&oldActiveTexture);activeTexture(0x84C0);}
    // Presentation-only proof: a full caption panel, not final typewriter/layout.
    glPushAttrib(GL_ALL_ATTRIB_BITS);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);glDisable(GL_COLOR_LOGIC_OP);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_CULL_FACE);glDisable(GL_ALPHA_TEST);glViewport(0,0,width,height);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    GLint mode;glGetIntegerv(GL_MATRIX_MODE,&mode);glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
    glOrtho(0,width,height,0,-1,1);glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    GLuint tex;glGenTextures(1,&tex);glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,640,176,0,GL_RGBA,GL_UNSIGNED_BYTE,s.pixels.data());
    glColor4f(1,1,1,1);float top=height-height*(176.0f/480.0f);
    glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(0,top);glTexCoord2f(1,0);glVertex2f(width,top);
    glTexCoord2f(1,1);glVertex2f(width,height);glTexCoord2f(0,1);glVertex2f(0,height);glEnd();
    glDeleteTextures(1,&tex);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(mode);glPopAttrib();
    if(activeTexture) activeTexture(oldActiveTexture);
    if(useProgram) useProgram(oldProgram);
    static bool reported=false;
    if(!reported) {std::fprintf(stderr,"STT_P1 panel submitted %dx%d incoming_stencil=%d mask=%d%d%d%d\n",width,height,
        incomingStencil,incomingMask[0],incomingMask[1],incomingMask[2],incomingMask[3]);reported=true;}
}
int observe(CPUState* cpu, bool is_general) {
    auto& s=state();
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if(!s.loaded) load(s);
        if(s.signature.empty()) return 0;
        uint32_t ptr=0;
        if(is_general) peek(cpu->gpr[29]+0x10,ptr);
        else if(cpu->gpr[4]<256) peek(0x800c6b88+cpu->gpr[4]*44+0x20,ptr);
        static unsigned traces=0;
        if(traces++<16) std::fprintf(stderr,"STT_P1 observe kind=%s ptr=%08x ra=%08x\n",is_general?"general":"dialogue",ptr,cpu->gpr[31]);
        if(match(ptr,s.signature)) {
            if(s.base!=ptr) std::fprintf(stderr,"STT_P1 matched U00208 ptr=%08x ra=%08x\n",ptr,cpu->gpr[31]);
            s.base=ptr;s.active=true;s.owner_general=is_general;s.owner_index=cpu->gpr[4];
        } else if(s.base && ptr>=s.base && ptr<s.base+s.signature.size() && match(s.base,s.signature)) s.active=true;
        else if(s.active && s.owner_general==is_general && (is_general || s.owner_index==cpu->gpr[4]) && ptr>=0x800e0000 && ptr<0x80100000) {
            std::fprintf(stderr,"STT_P1 cleared next-source ptr=%08x\n",ptr);s.active=false;s.base=0;
        }
    }
    psx_host_set_hud_draw_cb(draw,nullptr);
    return 0; // Original guest function and its cycle accounting execute unchanged.
}
int general(CPUState* c) { return observe(c,true); }
int dialogue(CPUState* c) { return observe(c,false); }
struct Register {
    Register() {
        int a=func_override_add("stt.p1.general",0x8004a954,general,0);
        int b=func_override_add("stt.p1.dialogue",0x8004b92c,dialogue,0);
        std::fprintf(stderr,"STT_P1 hooks=%d,%d\n",a,b);
    }
} registration;
}
