#include "native_protocol.h"
#include <stdexcept>
#include <iostream>
int main(){
    int count=0;auto check=[&](bool ok){++count;if(!ok)throw std::runtime_error("native test "+std::to_string(count));};
    auto req=vxe::request();check(req[0]==8 && req[1]==4 && req[16]==0x49);
    unsigned sum=0;for(unsigned i=1;i<17;++i)sum+=req[i];check((sum&255)==0x4D);
    vxe::Packet real{8,4,0,0,0,2,0x55,0,0x0f,0xa8,0,0,0,0,0,0,0x3b};
    check(vxe::parse(real.data(),17)==85);check(vxe::parse(nullptr,17)==vxe::unavailable);
    for(auto n:{0,16,18})check(vxe::parse(real.data(),n)==vxe::unavailable);
    for(unsigned i:{0,1,2,16}){auto b=real;b[i]^=1;check(vxe::parse(b.data(),17)==vxe::unavailable);}
    for(unsigned p:{0,100,101,255}){auto b=real;b[6]=static_cast<std::uint8_t>(p);unsigned s=0;for(unsigned i=1;i<16;++i)s+=b[i];b[16]=static_cast<std::uint8_t>(0x4D-s);check(vxe::parse(b.data(),17)==(p<=100?int(p):vxe::unavailable));}
    check(vxe::target_path(L"\\\\?\\hid#vid_373b&pid_10c9&mi_01&col05#test"));check(vxe::target_path(L"\\\\?\\HID#VID_373B&PID_10C9&MI_01&COL05#test"));
    check(!vxe::target_path(L"\\\\?\\hid#vid_373b&pid_10c9&mi_01&col04#test"));check(!vxe::target_path(L"\\\\?\\hid#vid_373b&pid_10c9&mi_01&col050#test"));
    check(vxe::target_caps(0x373B,0x10C9,0xFF02,2,17,17));
    for(unsigned i=0;i<6;++i){unsigned f[]{0x373B,0x10C9,0xFF02,2,17,17};++f[i];check(!vxe::target_caps(f[0],f[1],f[2],f[3],f[4],f[5]));}
    std::cout<<count<<" native tests passed\n";
}
