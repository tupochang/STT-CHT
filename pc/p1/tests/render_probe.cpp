// Exercise the actual caption renderer in a hidden OpenGL window, with a
// foreign shader already bound. No game or guest RAM runs in this probe.
#include "../native_caption.cpp"
extern "C" {
void stt_pc_controls_frame(){}
unsigned probeOriginalCalls=0;uint16_t probeWidth=0,probeHeight=0;
uint32_t g_psx_ram_mask=0x1fffff, g_psx_ram_size=0x200000;
int psx_peek_word(uint32_t,uint32_t*) {return 0;}
int func_override_add(const char*,uint32_t,FuncOverrideFn,int32_t) {return 0;}
void psx_host_set_hud_draw_cb(PsxHostOverlayDrawFn,void*) {}
void psx_host_overlay_set_draw_cb(PsxHostOverlayDrawFn,void*) {}
void func_override_call_original(CPUState* cpu){++probeOriginalCalls;probeWidth=12;probeHeight=16;cpu->gpr[2]=16;}
void psx_host_write_half(uint32_t address,uint16_t value){if(address==0x8001000a)probeHeight=value;}
}
int main() {
    HWND win=CreateWindowW(L"STATIC",L"Caption render probe",WS_POPUP,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    HDC dc=GetDC(win);
    PIXELFORMATDESCRIPTOR fmt{};fmt.nSize=sizeof(fmt);fmt.nVersion=1;
    fmt.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;fmt.iPixelType=PFD_TYPE_RGBA;fmt.cColorBits=32;fmt.cStencilBits=8;
    if(!SetPixelFormat(dc,ChoosePixelFormat(dc,&fmt),&fmt)) return 2;
    HGLRC bootstrap=wglCreateContext(dc);if(!wglMakeCurrent(dc,bootstrap)) return 3;
    using CreateContext=HGLRC(WINAPI*)(HDC,HGLRC,const int*);
    auto createContext=(CreateContext)wglGetProcAddress("wglCreateContextAttribsARB");
    if(!createContext)return 6;
    const int attributes[]={0x2091,3,0x2092,3,0x9126,1,0};
    HGLRC context=createContext(dc,nullptr,attributes);
    if(!context || !wglMakeCurrent(dc,context))return 7;
    wglDeleteContext(bootstrap);
    GLint profile=0;glGetIntegerv(0x9126,&profile);if(!(profile&1))return 8;
    using CreateShader=GLuint(APIENTRY*)(GLenum);
    using ShaderSource=void(APIENTRY*)(GLuint,GLsizei,const char*const*,const GLint*);
    using Compile=void(APIENTRY*)(GLuint);
    using CreateProgram=GLuint(APIENTRY*)();
    using Attach=void(APIENTRY*)(GLuint,GLuint);
    using Link=void(APIENTRY*)(GLuint);
    using Use=void(APIENTRY*)(GLuint);
    auto createShader=(CreateShader)wglGetProcAddress("glCreateShader");
    auto source=(ShaderSource)wglGetProcAddress("glShaderSource");
    auto compile=(Compile)wglGetProcAddress("glCompileShader");
    auto createProgram=(CreateProgram)wglGetProcAddress("glCreateProgram");
    auto attach=(Attach)wglGetProcAddress("glAttachShader");
    auto link=(Link)wglGetProcAddress("glLinkProgram");
    auto use=(Use)wglGetProcAddress("glUseProgram");
    if(!createShader||!source||!compile||!createProgram||!attach||!link||!use) return 4;
    const char* vs="#version 330 core\nvoid main(){gl_Position=vec4(0,0,0,1);}";
    const char* fs="#version 330 core\nout vec4 color;void main(){color=vec4(1,0,0,1);}";
    GLuint v=createShader(0x8B31),f=createShader(0x8B30);source(v,1,&vs,nullptr);source(f,1,&fs,nullptr);compile(v);compile(f);
    GLuint program=createProgram();attach(program,v);attach(program,f);link(program);use(program);
    glViewport(0,0,640,480);glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);
    // The PS1 mask-bit renderer can leave stencil rejection and color masks
    // active. The HUD must draw independently, then restore those states.
    glEnable(GL_STENCIL_TEST);glStencilFunc(GL_NEVER,0,~0u);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
    warmup(640,480,nullptr);if(!state().renderer_ready)return 12;
    auto& s=state();load(s);
    for(const auto& e:s.entries)if(e.uid=="U00208"){s.signature=e.raw;s.caption=e.text;s.timeline=e.timeline;break;}
    s.active=true;s.visible=s.caption.size();draw(640,480,nullptr);glFinish();
    s.in_frame=true;CPUState cpu{};cpu.gpr[4]=0x80010000;cpu.gpr[31]=0x8004bdb0;
    if(glyph(&cpu)!=1 || probeOriginalCalls!=1 || probeWidth!=12 || probeHeight!=0 || cpu.gpr[2]!=16)return 10;
    cpu.gpr[31]=0x80000000;if(glyph(&cpu)!=0 || probeOriginalCalls!=1)return 11;s.in_frame=false;
    GLint restored=0;glGetIntegerv(0x8B8D,&restored);
    GLboolean mask[4];glGetBooleanv(GL_COLOR_WRITEMASK,mask);
    bool stateRestored=glIsEnabled(GL_STENCIL_TEST)&&!mask[0]&&!mask[1]&&!mask[2]&&!mask[3];
    GLenum error=glGetError();
    std::vector<unsigned char> rgba(640*480*4);glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    size_t white=0,dark=0;
    for(size_t i=0;i<640*176*4;i+=4) {
        if(rgba[i]>200&&rgba[i+1]>200&&rgba[i+2]>200)++white;
        if(rgba[i]==18&&rgba[i+1]==24&&rgba[i+2]==30)++dark;
    }
    for(size_t i=0;i<rgba.size();i+=4)std::swap(rgba[i],rgba[i+2]);
    BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+rgba.size();
    BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=640;info.biHeight=480;info.biPlanes=1;info.biBitCount=32;
    std::ofstream bmp("work/p0/p1-native-panel.bmp",std::ios::binary);bmp.write((char*)&file,sizeof(file));bmp.write((char*)&info,sizeof(info));bmp.write((char*)rgba.data(),rgba.size());bmp.close();
    s.active=false;glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDisable(GL_STENCIL_TEST);glClear(GL_COLOR_BUFFER_BIT);draw(640,480,nullptr);unsigned char pixel[4];glReadPixels(20,20,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    bool clear=pixel[0]==0&&pixel[1]==0&&pixel[2]==255;
    s.active=true;s.in_frame=true;s.text_x=60;s.text_y=24;s.pixels.clear();draw(640,480,nullptr);glFinish();
    glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());size_t inWhite=0,inBlue=0,outWhite=0;
    for(int y=0;y<480;++y)for(int x=0;x<640;++x){size_t i=(y*640+x)*4;
        bool inside=x>=120&&x<608&&y>=304&&y<432;
        bool isWhite=rgba[i]>200&&rgba[i+1]>200&&rgba[i+2]>200;
        if(inside&&isWhite)++inWhite;if(!inside&&isWhite)++outWhite;
        if(inside&&rgba[i]==0&&rgba[i+1]==0&&rgba[i+2]==255)++inBlue;}
    for(size_t i=0;i<rgba.size();i+=4)std::swap(rgba[i],rgba[i+2]);
    std::ofstream frame("work/p0/p1-inframe.bmp",std::ios::binary);frame.write((char*)&file,sizeof(file));frame.write((char*)&info,sizeof(info));frame.write((char*)rgba.data(),rgba.size());frame.close();
    auto wide=stt_caption::fit_rect(960,480,60,24,244,64);
    bool layoutOK=wide.x==280&&wide.y==304&&wide.w==488&&wide.h==128;
    size_t kept=s.visible;
    cpu.gpr[31]=0x80021df8;cpu.gpr[4]=4;
    if(presentation(&cpu)!=0 || s.presentation_visible || !s.active || s.visible!=kept)return 13;
    glClear(GL_COLOR_BUFFER_BIT);draw(640,480,nullptr);
    glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    size_t hiddenWhite=0;for(size_t i=0;i<rgba.size();i+=4)if(rgba[i]>200&&rgba[i+1]>200&&rgba[i+2]>200)++hiddenWhite;
    cpu.gpr[31]=0x80021e08;cpu.gpr[4]=1;
    if(presentation(&cpu)!=0 || !s.presentation_visible || s.visible!=kept)return 14;
    draw(640,480,nullptr);glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    size_t resumedWhite=0;for(size_t i=0;i<rgba.size();i+=4)if(rgba[i]>200&&rgba[i+1]>200&&rgba[i+2]>200)++resumedWhite;
    cpu.gpr[31]=0;cpu.gpr[4]=4;presentation(&cpu);if(!s.presentation_visible)return 15;
    s.tint[0]=s.tint[1]=s.tint[2]=0.5f;glClear(GL_COLOR_BUFFER_BIT);draw(640,480,nullptr);
    glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());size_t gray=0;
    for(size_t i=0;i<rgba.size();i+=4)if(rgba[i]>=125&&rgba[i]<=130&&rgba[i+1]>=125&&rgba[i+1]<=130&&rgba[i+2]>=125&&rgba[i+2]<=130)++gray;
    if(gray<100)return 16;
    s.tint[0]=s.tint[1]=s.tint[2]=0;glClear(GL_COLOR_BUFFER_BIT);draw(640,480,nullptr);
    glReadPixels(20,20,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);if(pixel[0]!=0||pixel[1]!=0||pixel[2]!=255)return 17;
    s.tint[0]=s.tint[1]=s.tint[2]=1;
    std::printf("fade render half-bright pixels=%zu black fade skipped\n",gray);
    std::printf("hide/release hidden_white=%zu restored_white=%zu progress_preserved=%d\n",hiddenWhite,resumedWhite,s.visible==kept);
    std::printf("inframe white=%zu transparent_background=%zu outside_white=%zu layout=%d glyph_width_preserved=%d\n",inWhite,inBlue,outWhite,layoutOK,probeWidth==12);
    std::printf("render_probe core_profile=%d white=%zu dark=%zu shader_restored=%d mask_stencil_restored=%d clear=%d GL_error=%u\n",profile,white,dark,restored==(GLint)program,stateRestored,clear,error);
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context);ReleaseDC(win,dc);DestroyWindow(win);
    return white>100&&dark>10000&&restored==(GLint)program&&stateRestored&&clear&&error==GL_NO_ERROR&&inWhite>100&&inBlue>10000&&outWhite==0&&layoutOK&&hiddenWhite==0&&resumedWhite==inWhite?0:5;
}
