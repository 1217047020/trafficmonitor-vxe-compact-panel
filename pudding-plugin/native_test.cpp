#include "jl_auth.h"
#include "gaia_protocol.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
unsigned checks=0;
void check(bool condition,const char* label) {
    ++checks;if(!condition){printf("FAIL: %s\n",label);exit(1);}
}
void protocol_tests() {
    using namespace pudding;
    auto packet=request(0xc1,3,{0,0,0,1});
    check(packet==Bytes({0xfe,0xdc,0xba,0xc0,0xc1,0,5,3,0,0,0,1,0xef}),"request wire format");
    check(request(3,1,Bytes(1024)).empty(),"oversized request rejected");
    Bytes reply={0xfe,0xdc,0xba,0,0xc1,0,2,2,3,0xef};
    Framer split;std::vector<Frame> frames;
    for(auto byte:reply) {
        auto batch=split.feed(&byte,1);frames.insert(frames.end(),batch.begin(),batch.end());
    }
    check(frames.size()==1 && split.pending()==0 && !split.invalid,"one-byte fragmented reply");
    check(response(frames[0],0xc1,3) && frames[0].body[0]==2,"live unsupported reply");
    check(!response(frames[0],0xc1,4),"wrong sequence ignored");
    check(!response(frames[0],3,3),"wrong opcode ignored");
    auto notification=frames[0];notification.flags=0x40;
    check(!response(notification,0xc1,3),"notification not a query response");
    Framer coalesced;auto both=reply;both.insert(both.end(),reply.begin(),reply.end());
    check(coalesced.feed(both.data(),both.size()).size()==2,"coalesced replies");
    Framer partial;partial.feed(reply.data(),5);
    check(partial.pending()==5,"partial header retained");
    check(partial.feed(reply.data()+5,reply.size()-5).size()==1,"partial header completed");
    Framer bad_end;auto bad=reply;bad.back()=0;
    check(bad_end.feed(bad.data(),bad.size()).empty() && bad_end.invalid,"bad footer rejected");
    Framer huge;Bytes oversized(4097);
    check(huge.feed(oversized.data(),oversized.size()).empty() && huge.invalid && huge.pending()==0,"bounded input memory");
    Framer bad_length;Bytes length={0xfe,0xdc,0xba,0,3,0xff,0xff};
    check(bad_length.feed(length.data(),length.size()).empty() && bad_length.invalid,"bounded frame length");
    std::vector<std::pair<uint8_t,Bytes>> attrs={{7,{1}}};
    check(!attributes({0},attrs) && attrs[0].first==7,"TLV length zero fails atomically");
    check(!attributes({4,0,100},attrs),"truncated TLV fails");
    check(attributes({1,7},attrs) && attrs.size()==1 && attrs[0].second.empty(),"empty attribute allowed");

    std::array<Battery,3> values{};
    // Synthetic parser fixture ONLY, not a hardware capture or verification.
    check(decode_adv({4,0,100,100,32},values)==Status::ok && values[2].percent==32,"candidate three-battery parser");
    check(decode_adv({4,0,0x80,0xe4,0xa0},values)==Status::ok && values[0].percent==0 && values[2].charging,"charge flag and valid zero");
    auto saved=values;
    check(decode_adv({4,0,100,100,0xff},values)==Status::missing_case && values[2].percent==saved[2].percent,"unknown case not zero");
    check(decode_adv({4,0,100,100,101},values)==Status::missing_case,"out-of-range case rejected");
    check(decode_adv({3,0,100,100},values)==Status::malformed,"two-ear data not case data");
    check(decode_adv({4,0,100,100,32,4,0,100,100,33},values)==Status::malformed,"duplicate case block rejected");
    check(decode_adv({1,8},values)==Status::missing_case,"absent battery block");
    check(decode_adv({},values)==Status::missing_case,"empty battery response");
    Reading reading;
    check(display(reading,2,1)==L"等待","initial wait display");
    reading.status=Status::offline;
    check(display(reading,2,1)==L"离线","offline display");
    reading.status=Status::unsupported;
    check(display(reading,2,1)==L"待适配","unsupported display");
    reading.status=Status::ok;reading.timestamp=1;reading.batteries[2]={32,false};
    check(display(reading,2,2,false)==L"待适配","disabled hardware gate ignores synthetic success");
    check(display(reading,2,std::numeric_limits<uint64_t>::max(),true)!=L"32%","stale sample never percentage");
    check(display(reading,3,2)==L"不可用","invalid item index");
}
void gaia_tests() {
    using namespace pudding;
    // Actual target-filtered phone HCI frames, 2026-10-02. No peer address or key.
    Bytes api_tx={0xff,1,0,0,0,0x0a,3,0};
    Bytes api_rx={0xff,4,0,4,0,0x0a,0x83,0,0,4,3,1};
    Bytes supported_rx={0xff,4,0,2,0,0x1d,0x1b,0,1,2};
    Bytes battery_rx={0xff,4,0,6,0,0x1d,0x1b,1,1,100,2,100,3,23};
    check(gaia_request(1,0x0a,0x0300)==api_tx,"captured API init request");
    check(gaia_request(4,0x1d,0x1a01,{1,2})==Bytes({0xff,4,0,2,0,0x1d,0x1a,1,1,2}),"captured battery request");
    check(gaia_request(3,0x1d,0x1a01).empty(),"unsupported outgoing version");
    check(gaia_request(4,0x1d,0x1a01,Bytes(256)).empty(),"oversized outgoing GAIA");
    GaiaFramer api;auto frames=api.feed(api_rx.data(),api_rx.size());
    check(frames.size()==1 && gaia_matches(frames[0],0x0a,0x8300) && frames[0].payload==Bytes({0,4,3,1}),"actual API response");
    for(size_t cut=0;cut<=battery_rx.size();++cut) {
        GaiaFramer parser;auto a=parser.feed(battery_rx.data(),cut);
        auto b=parser.feed(battery_rx.data()+cut,battery_rx.size()-cut);a.insert(a.end(),b.begin(),b.end());
        check(a.size()==1 && parser.pending()==0 && !parser.invalid,"all GAIA split boundaries");
        check(a[0].wire==battery_rx && a[0].payload==Bytes({1,100,2,100,3,23}),"split capture roundtrip");
    }
    GaiaFramer joined;auto both=supported_rx;both.insert(both.end(),battery_rx.begin(),battery_rx.end());
    auto pairs=joined.feed(both.data(),both.size());
    check(pairs.size()==2,"coalesced support and level frames");
    check(gaia_supported_batteries(pairs[0].payload),"support list may omit case ID3");
    check(!gaia_supported_batteries({1,2,2}) && !gaia_supported_batteries({1}) && !gaia_supported_batteries({1,2,4}),"invalid supported lists");
    std::array<Battery,3> values{};
    check(decode_gaia_battery(pairs[1].payload,values)==Status::ok && values[0].percent==100 && values[1].percent==100 && values[2].percent==23,"phone capture battery triplet");
    auto saved=values;
    check(decode_gaia_battery({1,100,2,100},values)==Status::missing_case && values[2].percent==saved[2].percent,"missing case fails atomically");
    check(decode_gaia_battery({1,100,2,100,3,255},values)==Status::missing_case,"actual GAIA FF means unavailable case");
    check(decode_gaia_battery({1,255,2,100,3,23},values)==Status::ok && values[0].percent==-1 && values[2].percent==23,"unknown ear not invented zero");
    check(decode_gaia_battery({1,100,2,100,3,0x97},values)==Status::malformed,"do not assume high bit is GAIA charge flag");
    saved=values;
    check(decode_gaia_battery({1,100,2,100,3,0xde},values)==Status::malformed && values[2].percent==saved[2].percent,"live charging DE rejected atomically, not interpreted as 94 percent");
    check(decode_gaia_battery({1,100,2,100,3,0xdf},values)==Status::malformed && values[2].percent==saved[2].percent,"live charging DF rejected atomically, not interpreted as 95 percent");
    check(decode_gaia_battery({1,100,2,100,3,101},values)==Status::malformed,"GAIA 101 rejected");
    check(decode_gaia_battery({1,100,2,100,3},values)==Status::malformed,"GAIA partial pair");
    check(decode_gaia_battery({1,100,3,23,3,24},values)==Status::malformed,"GAIA duplicate ID rejected");
    check(decode_gaia_battery({1,0,2,100,3,0},values)==Status::ok && values[2].percent==0,"synthetic zero valid");
    auto notification=pairs[1];notification.command=0x1a81;
    check(!gaia_matches(notification,0x1d,0x1b01),"notification cannot masquerade as response");
    check(!gaia_matches(pairs[1],0x0a,0x1b01),"wrong vendor ignored");
    GaiaFramer checksum;auto checked=battery_rx;checked[2]=1;
    uint8_t sum=0;for(auto b:checked)sum^=b;checked.push_back(sum);
    check(checksum.feed(checked.data(),checked.size()).size()==1 && !checksum.invalid,"optional XOR checksum");
    checked.back()^=1;GaiaFramer corrupt;
    check(corrupt.feed(checked.data(),checked.size()).empty() && corrupt.invalid,"corrupt XOR rejected");
    auto extended=battery_rx;extended[2]=2;extended.insert(extended.begin()+3,0);
    GaiaFramer extension;
    check(extension.feed(extended.data(),extended.size()).size()==1,"version4 length extension");
    extended[1]=1;GaiaFramer wrong_extension;
    check(wrong_extension.feed(extended.data(),extended.size()).empty() && wrong_extension.invalid,"legacy length extension rejected");
    GaiaFramer oversized;Bytes huge(4097);
    check(oversized.feed(huge.data(),huge.size()).empty() && oversized.invalid,"GAIA bounded input");
    Reading reading;reading.status=Status::ok;reading.batteries=values;reading.timestamp=100;
    check(display(reading,2,99)==L"不可用","backward clock fails closed");
    check(display(reading,2,90101)==L"不可用","stale GAIA reading fails closed");
    check(display(reading,2,100,true)==L"0%","enabled display valid zero");
    reading.batteries[2].percent=23;
    check(display(reading,2,100,true)==L"23%","enabled display captured case value");
    std::array<Battery,3> unplugged{};
    check(decode_gaia_battery({1,100,2,100,3,97},unplugged)==Status::ok && unplugged[2].percent==97,"live unplugged case97 decoded without charge masking");
    reading.batteries[2].percent=97;
    check(display(reading,2,100,true)==L"97%","enabled display new Windows case state");
    std::array<Battery,3> second_state{};
    check(decode_gaia_battery({1,100,2,100,3,84},second_state)==Status::ok && second_state[2].percent==84,"second Windows state matched live phone App84");
    auto verified=reading;verified.batteries=second_state;
    check(display(verified,2,100)==L"84%","production gate enabled only after two actual App comparisons");
    check(display(reading,0,100,true)==L"0%" && display(reading,1,100,true)==L"100%","distinct ear values");
    reading.batteries[2].percent=101;
    check(display(reading,2,100,true)==L"不可用","enabled display rejects invalid case");
    reading.status=Status::offline;
    check(display(reading,2,100,true)==L"离线","enabled offline clears numeric display");
    reading.status=Status::transport_error;
    check(display(reading,2,100,true)==L"不可用","enabled transport error clears old value");
}
}
int main() {
    // Published cipher test vector; unrelated to any local device identity.
    jl_auth::Block input={0xb6,0xe0,0x80,0xec,0xaf,0xf3,0x22,0x91,0x6d,0x88,0xfa,0xd5,0xaa,0x34,0xc2,0xac};
    jl_auth::Block output={0x1d,0x88,0x97,0xac,0x46,0x04,0xd3,0x32,0xe8,0x17,0x5e,0x81,0xbb,0x29,0x25,0x24};
    check(jl_auth::encrypt(input)==output,"JL cipher known-answer vector");
    protocol_tests();gaia_tests();printf("PASS: %u checks (offline; no Bluetooth calls)\n",checks);return 0;
}
