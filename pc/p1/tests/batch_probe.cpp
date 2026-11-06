// Exercise the actual batch loader and dialogue observer against synthetic RAM.
#include "../native_caption.cpp"
#include <cstring>
std::vector<unsigned char> ram(0x200000);
extern "C" {
uint32_t g_psx_ram_mask=0x1fffff,g_psx_ram_size=0x200000;
int psx_peek_word(uint32_t a,uint32_t* v){uint32_t off=a&0x1fffff;if(off>ram.size()-4)return 0;std::memcpy(v,ram.data()+off,4);return 1;}
int func_override_add(const char*,uint32_t,FuncOverrideFn,int32_t){return 0;}
void psx_host_set_hud_draw_cb(PsxHostOverlayDrawFn,void*){}
void psx_host_overlay_set_draw_cb(PsxHostOverlayDrawFn,void*){}
void func_override_call_original(CPUState*){}
void psx_host_write_half(uint32_t,uint16_t){}
}
void word(uint32_t a,uint32_t v){std::memcpy(ram.data()+(a&0x1fffff),&v,4);}
int main(){
    auto& s=state();load(s);if(s.entries.size()<100)return 1;
    for(const auto& e:s.entries){if(e.timeline.size()!=e.raw.size()/2+1||e.timeline.back()!=e.text.size())return 2;
        if(!std::is_sorted(e.timeline.begin(),e.timeline.end()))return 3;}
    size_t a=SIZE_MAX,b=SIZE_MAX;
    for(size_t i=0;i<s.entries.size();++i){if(s.entries[i].uid=="U00208")a=i;if(s.entries[i].uid=="U00209")b=i;}
    if(a==SIZE_MAX||b==SIZE_MAX)return 4;
    s.renderer_ready=true;
    uint32_t base=0x800e4aae,slot=0x800c6b88;
    auto place=[&](size_t index,unsigned mode,size_t progress){std::fill(ram.begin(),ram.end(),0);
        const auto& e=s.entries[index];std::memcpy(ram.data()+(base&0x1fffff),e.raw.data(),e.raw.size());
        std::memcpy(ram.data()+((base-16)&0x1fffff),e.contexts[0].data(),16);
        word(slot,1);word(slot+4,60u<<16);word(slot+8,24);word(slot+0x10,mode);word(slot+0x20,base+static_cast<uint32_t>(progress*2));refresh(nullptr);};
    place(a,0,0);if(!s.active||s.entry_index!=a||s.visible!=0)return 5;
    place(a,0,10);if(s.visible==0)return 6;
    place(a,4,s.entries[a].raw.size()/2);if(!s.active||s.visible!=s.caption.size())return 7;
    place(b,0,8);if(!s.active||s.entry_index!=b||s.caption!=s.entries[b].text)return 8;
    place(a,0,4);if(!s.active||s.entry_index!=a)return 9;
    word(slot,0);refresh(nullptr);if(s.active)return 10;
    // Recover a mid-dialogue slot with no prior host state (save/load scenario).
    place(b,0,10);if(!s.active||s.entry_index!=b||s.visible==0)return 11;
    ram[(base-16)&0x1fffff]^=255;refresh(nullptr);if(s.active)return 15;
    size_t terminal=SIZE_MAX;for(size_t i=0;i<s.entries.size();++i)if(s.entries[i].raw.back()==255 && s.entries[i].raw[s.entries[i].raw.size()-2]==254 && fits(s.entries[i].text,60)){terminal=i;break;}
    if(terminal==SIZE_MAX)return 16;
    place(terminal,5,s.entries[terminal].raw.size()/2);if(!s.active||s.visible!=s.caption.size())return 17;
    std::fill(ram.begin(),ram.end(),0);word(slot,1);word(slot+0x20,base);refresh(nullptr);if(s.active)return 12;
    if(fits(std::wstring(200,L'漢'),60))return 13;
    s.active=true;s.in_frame=true;s.owner_index=1;CPUState cpu{};cpu.gpr[4]=0x80010000;cpu.gpr[31]=0x8004bdb0;cpu.gpr[21]=0;
    if(glyph(&cpu)!=0)return 14;
    place(a,4,s.entries[a].raw.size()/2);cpu.gpr[28]=0x800d0000;
    word(cpu.gpr[28]+0x2c0,1);word(cpu.gpr[28]+0x2f4,2u<<16);
    word(cpu.gpr[28]+0x2b8,64u|(64u<<16));word(cpu.gpr[28]+0x2bc,64);
    frame_sync(&cpu);if(!s.active||s.tint[0]!=0.5f||s.tint[1]!=0.5f||s.tint[2]!=0.5f)return 18;
    word(cpu.gpr[28]+0x2b8,0);word(cpu.gpr[28]+0x2bc,0);frame_sync(&cpu);
    if(s.tint[0]!=0||s.tint[1]!=0||s.tint[2]!=0)return 19;
    word(cpu.gpr[28]+0x2f4,3u<<16);frame_sync(&cpu);if(s.tint[0]!=1)return 20;
    // The scene closes the slot without another dialogue update.
    word(slot+0x10,6);frame_sync(&cpu);if(s.active)return 21;
    std::puts("frame_sync: half/black fade, selective-mode isolation and scene close without dialogue updates passed");
    std::printf("batch_probe: %zu records validated; replacement, backward seek, wait, clear, mid-dialogue recovery, width rejection, slot isolation passed\n",s.entries.size());return 0;
}
