#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <cwctype>
// Port of verified local JS. Command source: noosxe/atk-tool (MIT).
namespace vxe {
constexpr int unavailable=-1, offline=-2;
using Packet=std::array<std::uint8_t,17>;
constexpr Packet request(){return {8,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x49};}
inline int parse(const std::uint8_t* b,std::size_t n) noexcept {
    if(!b || n!=17 || b[0]!=8 || b[1]!=4 || b[2]!=0) return unavailable;
    unsigned sum=0;for(std::size_t i=1;i<n;++i)sum+=b[i];
    return (sum&255)==0x4D && b[6]<=100 ? b[6] : unavailable;
}
inline bool target_path(std::wstring path){
    for(auto& c:path)c=static_cast<wchar_t>(std::towlower(c));
    return path.find(L"vid_373b&pid_10c9&mi_01&col05#")!=std::wstring::npos;
}
constexpr bool target_caps(unsigned vid,unsigned pid,unsigned page,unsigned usage,unsigned in,unsigned out){
    return vid==0x373B && pid==0x10C9 && page==0xFF02 && usage==2 && in==17 && out==17;
}
}
