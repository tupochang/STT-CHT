#include "../pc_controls.cpp"
#include <tuple>
int paused=0,captured=0;PsxHostInputSinkFn sink=nullptr;PsxHostOverlayDrawFn drawer=nullptr;
std::vector<std::tuple<uint32_t,uint32_t,unsigned>> writes;
extern "C" {
int psx_host_pause_push(const char*){++paused;return 1;}void psx_host_pause_pop(){--paused;}
void psx_host_set_ui_capture(int on){captured=on;}
void psx_host_set_input_sink(PsxHostInputSinkFn fn,void*){sink=fn;}
void psx_host_overlay_set_draw_cb(PsxHostOverlayDrawFn fn,void*){drawer=fn;}
int psx_presentation_fast_forward(){return 0;}
int psx_peek_word(uint32_t,uint32_t* v){*v=0;return 1;}
void psx_host_write_word(uint32_t a,uint32_t v){writes.emplace_back(a,v,4);}
void psx_host_write_half(uint32_t a,uint16_t v){writes.emplace_back(a,v,2);}
void psx_host_write_byte(uint32_t a,uint8_t v){writes.emplace_back(a,v,1);}
}
int main(){
    if(!sink)return 1;SDL_Event e{};e.type=SDL_KEYDOWN;e.key.key=SDLK_SPACE;
    if(sink(&e,nullptr)!=0)return 2; // Native TurboToggle must receive Space.
    e.key.key=SDLK_F1;if(!sink(&e,nullptr)||paused!=1||!captured||!drawer)return 3;
    auto& c=controls();size_t supported=0;for(auto& a:c.cheats){if(a.enabled)return 4;supported+=a.supported;}
    if(c.cheats.size()!=9||supported!=6)return 5;
    HWND window=CreateWindowW(L"STATIC",L"Menu probe",WS_POPUP,0,0,800,620,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);HDC dc=GetDC(window);
    PIXELFORMATDESCRIPTOR fmt{};fmt.nSize=sizeof(fmt);fmt.nVersion=1;fmt.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;fmt.iPixelType=PFD_TYPE_RGBA;fmt.cColorBits=32;
    if(!SetPixelFormat(dc,ChoosePixelFormat(dc,&fmt),&fmt))return 16;
    HGLRC bootstrap=wglCreateContext(dc);wglMakeCurrent(dc,bootstrap);
    using CreateContext=HGLRC(WINAPI*)(HDC,HGLRC,const int*);auto createContext=(CreateContext)wglGetProcAddress("wglCreateContextAttribsARB");
    const int attrs[]={0x2091,3,0x2092,3,0x9126,1,0};if(!createContext)return 17;
    HGLRC context=createContext(dc,nullptr,attrs);if(!context||!wglMakeCurrent(dc,context))return 18;wglDeleteContext(bootstrap);
    glViewport(0,0,800,620);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);drawer(800,620,nullptr);glFinish();
    std::vector<unsigned char> pixels(800*620*4);glReadPixels(0,0,800,620,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());unsigned yellow=0,white=0;
    for(size_t i=0;i<pixels.size();i+=4){if(pixels[i]>220&&pixels[i+1]>190&&pixels[i+2]<130)++yellow;if(pixels[i]>200&&pixels[i+1]>200&&pixels[i+2]>200)++white;std::swap(pixels[i],pixels[i+2]);}
    if(yellow<100||white<1000||glGetError()!=GL_NO_ERROR)return 19;
    BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+pixels.size();
    BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=800;info.biHeight=620;info.biPlanes=1;info.biBitCount=32;
    std::ofstream bmp("work/p0/controls-menu.bmp",std::ios::binary);bmp.write((char*)&file,sizeof(file));bmp.write((char*)&info,sizeof(info));bmp.write((char*)pixels.data(),pixels.size());bmp.close();
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context);ReleaseDC(window,dc);DestroyWindow(window);
    std::printf("menu core rendering: highlighted pixels=%u text pixels=%u GL_error=0\n",yellow,white);
    stt_pc_controls_frame();if(!writes.empty())return 6;
    e.key.repeat=true;sink(&e,nullptr);if(!c.open||paused!=1)return 7;e.key.repeat=false;
    e.key.key=SDLK_RETURN;sink(&e,nullptr);stt_pc_controls_frame();if(writes.size()!=3)return 8;
    for(auto [a,v,n]:writes)if(a<0x800bd800||a>=0x80200000||n!=2||v!=999)return 9;
    sink(&e,nullptr);writes.clear();stt_pc_controls_frame();if(!writes.empty())return 10;
    e.key.key=SDLK_ESCAPE;sink(&e,nullptr);if(c.open||paused||captured||drawer)return 11;
    e.key.key=SDLK_SPACE;if(sink(&e,nullptr)!=0)return 12;
    std::istringstream bad("[patch]\n800556D6 1400\n[unaligned]\n9013022D 1\n[conditional]\nD013022C 0001\n80130326 0009\n");
    auto codes=stt_cheats::parse(bad);if(codes[0].supported||codes[1].supported||!codes[2].supported)return 13;
    codes[2].enabled=true;unsigned count=0;
    stt_cheats::apply(codes,[](uint32_t,uint16_t& v){v=0;return true;},[&](uint32_t,uint32_t,unsigned){++count;});if(count)return 14;
    stt_cheats::apply(codes,[](uint32_t,uint16_t& v){v=1;return true;},[&](uint32_t,uint32_t,unsigned){++count;});if(count!=1)return 15;
    std::puts("controls_probe: F1 pause/capture, repeat guard, Space passthrough, six supported/all off, toggle writes, disable, close, patch rejection and conditionals passed");return 0;
}
