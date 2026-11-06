// OpenGL 3.3 core: no fixed-function calls or attribute/matrix stacks.
#pragma once
namespace {
struct PanelGL {
    HGLRC context=nullptr;
    GLuint program=0,vao=0,texture=0;
    GLuint (APIENTRY *CreateShader)(GLenum)=nullptr;
    void (APIENTRY *ShaderSource)(GLuint,GLsizei,const char*const*,const GLint*)=nullptr;
    void (APIENTRY *CompileShader)(GLuint)=nullptr;
    void (APIENTRY *GetShaderiv)(GLuint,GLenum,GLint*)=nullptr;
    void (APIENTRY *GetShaderInfoLog)(GLuint,GLsizei,GLsizei*,char*)=nullptr;
    GLuint (APIENTRY *CreateProgram)()=nullptr;
    void (APIENTRY *AttachShader)(GLuint,GLuint)=nullptr;
    void (APIENTRY *LinkProgram)(GLuint)=nullptr;
    void (APIENTRY *GetProgramiv)(GLuint,GLenum,GLint*)=nullptr;
    void (APIENTRY *DeleteShader)(GLuint)=nullptr;
    void (APIENTRY *UseProgram)(GLuint)=nullptr;
    void (APIENTRY *ActiveTexture)(GLenum)=nullptr;
    void (APIENTRY *GenVertexArrays)(GLsizei,GLuint*)=nullptr;
    void (APIENTRY *BindVertexArray)(GLuint)=nullptr;
    void (APIENTRY *BindBuffer)(GLenum,GLuint)=nullptr;
    void (APIENTRY *BindSampler)(GLuint,GLuint)=nullptr;
    GLint (APIENTRY *GetUniformLocation)(GLuint,const char*)=nullptr;
    void (APIENTRY *Uniform1i)(GLint,GLint)=nullptr;
    bool init() {
        HGLRC now=wglGetCurrentContext();
        if(now==context && program) return true;
        if(!now) return false;
#define PANEL_PROC(name) name=reinterpret_cast<decltype(name)>(wglGetProcAddress("gl" #name)); if(!name)return false
        PANEL_PROC(CreateShader);PANEL_PROC(ShaderSource);PANEL_PROC(CompileShader);PANEL_PROC(GetShaderiv);
        PANEL_PROC(GetShaderInfoLog);PANEL_PROC(CreateProgram);PANEL_PROC(AttachShader);PANEL_PROC(LinkProgram);
        PANEL_PROC(GetProgramiv);PANEL_PROC(DeleteShader);PANEL_PROC(UseProgram);PANEL_PROC(ActiveTexture);
        PANEL_PROC(GenVertexArrays);PANEL_PROC(BindVertexArray);PANEL_PROC(BindBuffer);PANEL_PROC(BindSampler);
        PANEL_PROC(GetUniformLocation);PANEL_PROC(Uniform1i);
#undef PANEL_PROC
        const char* vs="#version 330 core\nout vec2 uv;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=vec2(p.x,1.0-p.y);gl_Position=vec4(p*2.0-1.0,0,1);}";
        const char* fs="#version 330 core\nin vec2 uv;uniform sampler2D caption;out vec4 color;void main(){color=texture(caption,uv);}";
        GLuint shaders[2]={CreateShader(0x8B31),CreateShader(0x8B30)};
        const char* sources[2]={vs,fs};
        for(int i=0;i<2;++i) {
            ShaderSource(shaders[i],1,&sources[i],nullptr);CompileShader(shaders[i]);GLint ok=0;
            GetShaderiv(shaders[i],0x8B81,&ok);
            if(!ok){char error[1024]{};GetShaderInfoLog(shaders[i],sizeof(error),nullptr,error);
                std::fprintf(stderr,"STT_P1 shader failed: %s\n",error);DeleteShader(shaders[0]);DeleteShader(shaders[1]);return false;}
        }
        program=CreateProgram();AttachShader(program,shaders[0]);AttachShader(program,shaders[1]);LinkProgram(program);
        DeleteShader(shaders[0]);DeleteShader(shaders[1]);GLint ok=0;GetProgramiv(program,0x8B82,&ok);
        if(!ok){std::fprintf(stderr,"STT_P1 shader link failed\n");program=0;return false;}
        GenVertexArrays(1,&vao);glGenTextures(1,&texture);context=now;
        return true;
    }
    bool image(int width,int height,const unsigned char* rgba) {
        if(!init())return false;
        GLint oldProgram,oldVAO,oldActive,oldTexture,oldSampler,oldUnpack,oldAlignment,viewport[4],polygon[2];
        GLboolean mask[4];
        glGetIntegerv(0x8B8D,&oldProgram);glGetIntegerv(0x85B5,&oldVAO);glGetIntegerv(0x84E0,&oldActive);
        ActiveTexture(0x84C0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldTexture);glGetIntegerv(0x8919,&oldSampler);
        glGetIntegerv(0x88EF,&oldUnpack);glGetIntegerv(GL_UNPACK_ALIGNMENT,&oldAlignment);
        glGetIntegerv(GL_VIEWPORT,viewport);glGetBooleanv(GL_COLOR_WRITEMASK,mask);glGetIntegerv(GL_POLYGON_MODE,polygon);
        const GLenum gates[]={GL_DEPTH_TEST,GL_BLEND,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_CULL_FACE,GL_COLOR_LOGIC_OP,0x8C89};
        GLboolean enabled[7];for(int i=0;i<7;++i){enabled[i]=glIsEnabled(gates[i]);glDisable(gates[i]);}
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        BindBuffer(0x88EC,0);glPixelStorei(GL_UNPACK_ALIGNMENT,4);BindSampler(0,0);
        glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,640,176,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
        glViewport(0,0,width,height*176/480);UseProgram(program);Uniform1i(GetUniformLocation(program,"caption"),0);
        BindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,3);
        BindVertexArray(oldVAO);UseProgram(oldProgram);glBindTexture(GL_TEXTURE_2D,oldTexture);BindSampler(0,oldSampler);
        BindBuffer(0x88EC,oldUnpack);glPixelStorei(GL_UNPACK_ALIGNMENT,oldAlignment);ActiveTexture(oldActive);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glColorMask(mask[0],mask[1],mask[2],mask[3]);
        glPolygonMode(GL_FRONT_AND_BACK,polygon[0]);
        for(int i=0;i<7;++i)if(enabled[i])glEnable(gates[i]);
        return true;
    }
};
}
