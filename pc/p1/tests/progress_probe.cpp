#include "../caption_progress.h"
#include <cstdio>
int main() {
    std::vector<unsigned char> raw;
    for(unsigned c:{1u,2u,3u,4u,0xfffbu,0u,1u,2u,0xfffdu}){raw.push_back(c&255);raw.push_back(c>>8);}
    auto map=stt_caption::timeline(raw,L"AB\n CDE");
    if(map.size()!=10 || map[0]!=0 || map[2]!=1 || map[4]!=2 || map[5]!=3 || map[8]!=7 || map[9]!=7)return 1;
    for(size_t i=1;i<map.size();++i)if(map[i]<map[i-1])return 2;
    // FFFD has advanced the pointer to the next string but the old window waits.
    if(!stt_caption::sample(0x800e1000,raw.size(),1,4,0x800e1012).visible)return 3;
    if(stt_caption::sample(0x800e1000,raw.size(),1,0,0x800e1012).visible)return 4;
    if(stt_caption::sample(0x800e1000,raw.size(),0,4,0x800e1012).visible)return 5;
    if(stt_caption::sample(0x800e1000,raw.size(),1,6,0x800e1008).visible)return 6;
    if(stt_caption::sample(0x800e1000,raw.size(),1,0,0x800e1003).visible)return 7;
    auto before=stt_caption::sample(0x800e1000,raw.size(),1,0,0x800e1004);
    auto after=stt_caption::sample(0x800e1000,raw.size(),1,0,0x800e1008);
    if(!before.visible || !after.visible || map[before.words]>=map[after.words])return 8;
    bool rejected=false;try{stt_caption::timeline(raw,L"wrong line count");}catch(...){rejected=true;}
    if(!rejected)return 9;
    std::puts("progress_probe: line mapping, FFFD wait, close, replacement, rollback and unsupported layout checks passed");return 0;
}
