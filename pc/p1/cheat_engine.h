#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <cstdint>
namespace stt_cheats {
struct Code {uint32_t address,value;unsigned type;};
struct Cheat {std::string name;std::vector<Code> codes;bool enabled=false,supported=true;};
inline std::vector<Cheat> parse(std::istream& input){
    std::vector<Cheat> out;std::string line;
    while(std::getline(input,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty()||line[0]==';'||line[0]=='#')continue;
        if(line.front()=='['&&line.back()==']'){out.push_back({line.substr(1,line.size()-2),{}});continue;}
        if(out.empty()||line.find('=')!=std::string::npos)continue;
        uint64_t code=0,value=0;std::istringstream row(line);row>>std::hex>>code>>value;
        if(!row||code>0xffffffff||value>0xffffffff){out.back().supported=false;continue;}
        unsigned type=unsigned(code>>24);uint32_t address=0x80000000u|(uint32_t(code)&0xffffff);
        unsigned bytes=type==0x30?1:type==0x90?4:2;
        if((type!=0x80&&type!=0x90&&type!=0x30&&type!=0xd0&&type!=0xd1)||
           address<0x800bd800||uint64_t(address)+bytes>0x80200000||address%bytes||
           (bytes<4&&value>=(uint64_t(1)<<(bytes*8))))out.back().supported=false;
        out.back().codes.push_back({address,uint32_t(value),type});
    }
    for(auto& c:out)if(c.codes.empty()||c.codes.back().type==0xd0||c.codes.back().type==0xd1)c.supported=false;
    return out;
}
template<class Read,class Write>void apply(const std::vector<Cheat>& cheats,Read read,Write write){
    for(const auto& cheat:cheats)if(cheat.enabled&&cheat.supported){
        bool skip=false;
        for(const auto& c:cheat.codes){if(skip){skip=false;continue;}
            if(c.type==0xd0||c.type==0xd1){uint16_t current=0;if(!read(c.address,current))skip=true;else skip=c.type==0xd0?current!=c.value:current==c.value;}
            else write(c.address,c.value,c.type==0x90?4:c.type==0x30?1:2);
        }
    }
}
}
