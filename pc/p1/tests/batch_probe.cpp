// Exercise the actual batch loader and dialogue observer against synthetic RAM.
#include "../native_caption.cpp"
#include <cstring>
std::vector<unsigned char> ram(0x200000);
extern "C" {
void stt_pc_controls_frame(){}
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
    for(const auto& e:s.entries)for(const auto& page:e.pages){auto map=stt_caption::page_timeline(page,page.text);
        if(map.size()!=page.raw.size()/2+1||map.back()!=page.text.size())return 2;
        if(!std::is_sorted(map.begin(),map.end()))return 3;}
    size_t a=SIZE_MAX,b=SIZE_MAX;
    for(size_t i=0;i<s.entries.size();++i){if(s.entries[i].uid=="U00208")a=i;if(s.entries[i].uid=="U00209")b=i;}
    if(a==SIZE_MAX||b==SIZE_MAX)return 4;
    s.renderer_ready=true;
    uint32_t base=0x800e4aae,slot=0x800c6b88;
    auto place=[&](size_t index,unsigned mode,size_t progress,int x=60,int y=24){std::fill(ram.begin(),ram.end(),0);
        const auto& e=s.entries[index];std::memcpy(ram.data()+(base&0x1fffff),e.raw.data(),e.raw.size());
        std::memcpy(ram.data()+((base-e.contexts[0].size())&0x1fffff),e.contexts[0].data(),e.contexts[0].size());
        word(slot,1);word(slot+4,uint32_t(x)<<16);word(slot+8,y);word(slot+0x10,mode);word(slot+0x20,base+static_cast<uint32_t>(progress*2));refresh(nullptr);};
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
    size_t named=SIZE_MAX;for(size_t i=0;i<s.entries.size();++i)if(s.entries[i].uid=="U00213"){named=i;break;}
    if(named==SIZE_MAX||!s.entries[named].header_words)return 22;
    place(named,0,2);if(!s.active||s.visible!=0||s.text_y!=40)return 23;
    cpu.gpr[21]=s.owner_index;cpu.gpr[4]=0x80010000;cpu.gpr[31]=0x8004bdb0;
    if(glyph(&cpu)!=0)return 24; // Original speaker glyph must survive.
    place(named,0,s.entries[named].header_words+10);if(!s.active||!s.visible)return 25;
    std::puts("named dialogue: U00213 header retained, body shifted one line, source progress includes header passed");
    auto find=[&](const char* uid){for(size_t i=0;i<s.entries.size();++i)if(s.entries[i].uid==uid)return i;return SIZE_MAX;};
    for(const char* uid:{"U00211","U00215"}){auto index=find(uid);if(index==SIZE_MAX)return 26;int x=uid==std::string("U00215")?72:52;place(index,0,s.entries[index].header_words+1,x,uid==std::string("U00215")?154:22);
        if(!s.active||s.entry_index!=index||!fits(s.caption,x))return 27;}
    size_t paged=find("U00221");if(paged==SIZE_MAX||s.entries[paged].pages.size()!=2)return 28;
    size_t boundary=s.entries[paged].pages[0].end;
    place(paged,1,boundary);if(!s.active||s.page_index!=0||s.visible!=s.caption.size())return 29;
    place(paged,2,boundary);if(s.active)return 30;
    place(paged,0,boundary);if(!s.active||s.page_index!=1||s.visible!=0||s.text_y!=24)return 31;
    place(paged,0,boundary+10);if(!s.active||s.page_index!=1||s.visible==0)return 32;
    place(paged,5,s.entries[paged].raw.size()/2);if(!s.active||s.visible!=s.caption.size())return 33;
    place(paged,1,boundary);if(!s.active||s.page_index!=0)return 34;
    std::puts("pages: actual U00211/U00215 reflow, U00221 wait, scroll, next-page reset, progressive reveal, final wait and backward restore passed");
    for (const auto& fixture : {std::pair{"U00211", L"簡稱"}, std::pair{"U00230", L"痕跡"},
                                std::pair{"U00230", L"Number Ten"}}) {
        const auto index = find(fixture.first);
        std::wstring layout;
        if (index == SIZE_MAX || !layout_page(s.entries[index].pages[0].text, 52, 4, layout) ||
            layout.find(fixture.second) == std::wstring::npos) {
            std::fprintf(stderr, "Protected phrase layout failed for %s\n", fixture.first);
            return 48;
        }
    }
    std::ofstream audit("work/p0/early-pages-layout.tsv");audit<<"uid\tpage\tx\tcapacity\tfits\tlines\n";size_t early_ok=0,early_bad=0;
    for(const auto& e:s.entries)if(e.uid>="U00208"&&e.uid<="U00242")for(size_t p=0;p<e.pages.size();++p){
        bool named=e.header_words&&p==0;int x=e.header_words?72:52;std::wstring layout;bool ok=layout_page(e.pages[p].text,x,4-named,layout);
        audit<<e.uid<<'\t'<<p+1<<'\t'<<x<<'\t'<<4-named<<'\t'<<ok<<'\t'<<1+std::count(layout.begin(),layout.end(),L'\n')<<'\n';
        if(ok)++early_ok;else{++early_bad;std::printf("early layout capacity exceeded %s page=%zu\n",e.uid.c_str(),p+1);}}
    std::printf("early original-page layout audit: fits=%zu rejected=%zu (simulated original dialogue widths)\n",early_ok,early_bad);
    size_t win=find("U00243"),lose=find("U00246");if(win==SIZE_MAX||lose==SIZE_MAX)return 35;
    std::fill(ram.begin(),ram.end(),0);extra_state(1).renderer_ready=true;
    for(unsigned n=0;n<2;++n){size_t index=n?lose:win;uint32_t address=0x800e6000+n*0x1000,table=0x800c6b88+n*44;const auto& e=s.entries[index];
        std::memcpy(ram.data()+(address&0x1fffff),e.raw.data(),e.raw.size());std::memcpy(ram.data()+((address-e.contexts[0].size())&0x1fffff),e.contexts[0].data(),e.contexts[0].size());
        word(table,1);word(table+4,32u<<16);word(table+8,n?142:36);word(table+0x10,5);word(table+0x20,address+e.raw.size());}
    refresh(nullptr);if(!s.active||!extra_state(1).active||s.entries[s.entry_index].uid!="U00243"||extra_state(1).entries[extra_state(1).entry_index].uid!="U00246")return 36;
    word(0x800c6b88,0);refresh(nullptr);if(s.active||!extra_state(1).active)return 37;
    word(0x800c6b88+44,0);refresh(nullptr);if(extra_state(1).active)return 38;
    std::puts("parallel slots: actual victory/loss signatures displayed and cleared independently passed");
    size_t variable=find("U00103");if(variable==SIZE_MAX)return 39;
    place(variable,0,s.entries[variable].header_words+1,72,154);
    uint32_t config=0x80130000;word(0x800bd350,config);
    auto write_half=[&](uint32_t a,uint16_t v){std::memcpy(ram.data()+(a&0x1fffff),&v,2);};
    write_half(config+0x272,111);write_half(config+0x274,103);write_half(config+0x276,126);write_half(config+0x278,0xff05);
    refresh(nullptr);if(!s.active||s.caption.find(L"タクマ")==std::wstring::npos||s.caption.find(L"[HERO")!=std::wstring::npos)return 40;
    const auto& entry=s.entries[variable];size_t var_word=0;for(size_t j=entry.header_words;j<entry.raw.size()/2;++j)if((entry.raw[j*2]|entry.raw[j*2+1]<<8)==0xff02){var_word=j;break;}
    if(!var_word)return 41;word(slot+0x24,base+var_word*2);word(slot+0x20,config+0x272);refresh(nullptr);size_t first=s.visible;
    word(slot+0x20,config+0x278);refresh(nullptr);if(!s.active||s.visible<=first||s.base!=base)return 42;
    write_half(config+0x272,106);write_half(config+0x274,102);write_half(config+0x276,0xff05);word(slot+0x20,base+(var_word+1)*2);refresh(nullptr);
    if(!s.active||s.caption.find(L"サキ")==std::wstring::npos||s.caption.find(L"タクマ")!=std::wstring::npos)return 43;
    write_half(config+0x276,0);write_half(config+0x278,0);write_half(config+0x27a,0);write_half(config+0x27c,0xffff);refresh(nullptr);
    if(!s.active||s.caption.find(L"サキ")==std::wstring::npos||s.caption.find(L"サキ  ")!=std::wstring::npos)return 46;
    size_t local_var=var_word-entry.pages[0].begin;if(s.source_offsets[local_var+1]-s.source_offsets[local_var]!=5)return 47;
    std::puts("variables: U00103 live name, name-buffer pointer detour/progress, restored script pointer and changed player name passed");
    size_t brain=find("U00161");if(brain==SIZE_MAX)return 44;
    place(brain,0,s.entries[brain].pages[0].end-1,72,154);
    if(!s.active||s.entry_index!=brain||s.caption.find(L"腦波")==std::wstring::npos||s.caption.find(L"[HERO")!=std::wstring::npos)return 45;
    std::puts("U00161 brain-wave first page is translatable even though a later page contains a hero variable");
    std::printf("batch_probe: %zu records validated; replacement, backward seek, wait, clear, mid-dialogue recovery, width rejection, slot isolation passed\n",s.entries.size());return 0;
}
