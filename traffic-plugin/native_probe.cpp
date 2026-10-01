#include "native_hid.h"
#include <iostream>
#include <iomanip>
int main(){
    auto start=GetTickCount64();auto r=vxe::query();
    std::cout<<"{\"percentage\":"<<r.value<<",\"matches\":"<<r.matches<<",\"written\":"<<r.written<<",\"read\":"<<r.read<<",\"error\":"<<r.error<<",\"timeout\":"<<(r.timed_out?"true":"false")<<",\"workerStopped\":"<<(r.stop?"true":"false")<<",\"durationMs\":"<<(GetTickCount64()-start)<<",\"responseHex\":\"";
    if(r.read==17)for(unsigned i=0;i<17;++i){if(i)std::cout<<' ';std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(r.reply[i]);}
    std::cout<<"\"}"<<std::endl;return r.value>=0 ? 0 : 1;
}
