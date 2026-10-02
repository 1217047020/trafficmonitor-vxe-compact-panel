#pragma once
#include "bluetooth_transport.h"
#include "gaia_protocol.h"

namespace pudding {
inline Status link_failure(const Link& link,Status otherwise) {
    return link.timed_out ? Status::timeout : otherwise;
}
inline Status gaia_exchange(Link& link,GaiaFramer& framer,const Bytes& packet,
        uint16_t vendor,uint16_t expected,uint16_t error_command,GaiaFrame& out) {
    auto deadline=GetTickCount64()+4000;
    if(!link.send_all(packet.data(),packet.size(),deadline))return link_failure(link,Status::transport_error);
    uint8_t bytes[1024];
    while(GetTickCount64()<deadline) {
        int count=link.receive(bytes,sizeof(bytes),deadline);
        if(count<=0)return link_failure(link,Status::transport_error);
        auto frames=framer.feed(bytes,static_cast<size_t>(count));
        if(framer.invalid)return Status::malformed;
        for(auto& frame:frames) {
            if(error_command && gaia_matches(frame,vendor,error_command)){out=std::move(frame);return Status::unsupported;}
            if(!gaia_matches(frame,vendor,expected))continue; // Notification cannot fulfil a read.
            out=std::move(frame);return Status::ok;
        }
    }
    return Status::timeout;
}
inline Reading query() {
    Reading result;
    auto finish=[&](Status status,int error=0) {
        result.status=status;result.error=error;result.timestamp=GetTickCount64();return result;
    };
    Winsock winsock;
    if(winsock.error)return finish(Status::winsock_error,winsock.error);
    auto target=find_target();result.matches=target.matches;
    if(target.error)return finish(Status::enumeration_error,static_cast<int>(target.error));
    if(target.matches!=1)return finish(target.matches ? Status::ambiguous : Status::offline);
    if(!target.connected)return finish(Status::offline);
    Link link;
    if(!link.open(target.address,GetTickCount64()+8000))return finish(link_failure(link,Status::connect_error),link.error);
    // HCI evidence: do NOT enter JL OTA auth. Negotiate GAIA using version 1
    // GET_API_VERSION before the version-4 battery reads.
    GaiaFramer framer;GaiaFrame frame;
    result.stage=1;
    auto status=gaia_exchange(link,framer,gaia_request(1,0x000a,0x0300),0x000a,0x8300,0,frame);
    if(status!=Status::ok)return finish(status,link.error);
    if(frame.payload.size()!=4 || frame.payload[0]!=0)return finish(Status::negotiation_error,ERROR_INVALID_DATA);
    result.protocol_version=frame.payload[1];result.api_version=frame.payload[2];
    if(result.protocol_version!=4 || result.api_version!=3)return finish(Status::negotiation_error,ERROR_NOT_SUPPORTED);
    result.stage=2;
    status=gaia_exchange(link,framer,gaia_request(4,0x001d,0x1a00),0x001d,0x1b00,0x1b80,frame);
    if(status!=Status::ok)return finish(status,link.error);
    if(frame.version!=4)return finish(Status::malformed,ERROR_INVALID_DATA);
    if(!gaia_supported_batteries(frame.payload))return finish(Status::missing_case,ERROR_INVALID_DATA);
    // Actual support list [1,2]; level response additionally contains case ID 3.
    auto supported=frame.payload;
    result.stage=3;
    status=gaia_exchange(link,framer,gaia_request(4,0x001d,0x1a01,supported),0x001d,0x1b01,0x1b81,frame);
    if(status!=Status::ok)return finish(status,link.error);
    if(frame.version!=4)return finish(Status::malformed,ERROR_INVALID_DATA);
    result.battery_wire=std::move(frame.wire);
    status=decode_gaia_battery(frame.payload,result.batteries);
    return finish(status);
}
}
