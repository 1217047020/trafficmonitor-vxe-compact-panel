#include "../tools/controller_battery_model.h"
#include <iostream>
#include <stdexcept>
using namespace controller_battery;
void check(bool value) { if(!value) throw std::runtime_error("controller battery model assertion failed"); }
int main() {
    check(classify(1167,0,2,3).quality == Quality::offline);
    check(classify(5,0,2,3).quality == Quality::unavailable);
    check(classify(0,1167,2,3).quality == Quality::unavailable);
    check(classify(0,0,0,3).quality == Quality::unavailable);
    check(classify(0,0,1,3).quality == Quality::non_battery);
    check(classify(0,0,255,3).quality == Quality::unknown_type);
    check(classify(0,0,2,4).quality == Quality::unavailable);
    for(unsigned type : {2u,3u}) for(unsigned level=0;level<4;++level) {
        auto sample=classify(0,0,type,level);
        check(sample.quality==Quality::ordinal && sample.level==static_cast<int>(level));
    }
    check(!percentage(false,50,100));
    check(!percentage(true,std::nullopt,100));
    check(!percentage(true,50,std::nullopt));
    check(!percentage(true,0,0));
    check(!percentage(true,-1,100));
    check(!percentage(true,101,100));
    check(percentage(true,0,100)==0);
    check(percentage(true,100,100)==100);
    check(percentage(true,1,3)==33);
    check(percentage(true,2,3)==67);
    check(percentage(true,2147483647,2147483647)==100);
    std::cout << "controller battery model: PASS (no device access)\n";
}
