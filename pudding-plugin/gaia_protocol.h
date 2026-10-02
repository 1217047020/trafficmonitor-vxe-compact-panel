#pragma once
#include "battery_state.h"

namespace pudding {
struct GaiaFrame {
    uint8_t version=0,flags=0;
    uint16_t vendor=0,command=0;
    Bytes payload,wire;
};
inline bool gaia_matches(const GaiaFrame& frame,uint16_t vendor,uint16_t command) {
    return frame.vendor==vendor && frame.command==command;
}
// Transport version != API major version. This target starts with version 1
// GET_API_VERSION and replies with transport 4 / GAIA API 3.
inline Bytes gaia_request(uint8_t version,uint16_t vendor,uint16_t command,const Bytes& payload={}) {
    if((version!=1 && version!=4) || payload.size()>255)return {};
    Bytes out={0xff,version,0,static_cast<uint8_t>(payload.size()),
        static_cast<uint8_t>(vendor>>8),static_cast<uint8_t>(vendor),
        static_cast<uint8_t>(command>>8),static_cast<uint8_t>(command)};
    out.insert(out.end(),payload.begin(),payload.end());return out;
}
class GaiaFramer {
    Bytes buffer;
public:
    bool invalid=false;
    size_t pending()const{return buffer.size();}
    std::vector<GaiaFrame> feed(const uint8_t* bytes,size_t count) {
        std::vector<GaiaFrame> frames;
        if(count>4096 || buffer.size()+count>8192){invalid=true;buffer.clear();return frames;}
        if(count)buffer.insert(buffer.end(),bytes,bytes+count);
        while(buffer.size()>=4) {
            auto version=buffer[1],flags=buffer[2];
            if(buffer[0]!=0xff || version<1 || version>4 || (flags&~3) || ((flags&2)&&version<4)) {
                invalid=true;buffer.erase(buffer.begin());continue;
            }
            size_t header=(flags&2)?5:4;if(buffer.size()<header)break;
            size_t length=(flags&2)?(size_t(buffer[3])<<8)|buffer[4]:buffer[3];
            if(length>1024){invalid=true;buffer.erase(buffer.begin());continue;}
            size_t total=header+4+length+(flags&1);if(buffer.size()<total)break;
            if(flags&1) {
                uint8_t checksum=0;for(size_t i=0;i<total;++i)checksum^=buffer[i];
                if(checksum){invalid=true;buffer.erase(buffer.begin(),buffer.begin()+total);continue;}
            }
            uint16_t vendor=static_cast<uint16_t>((uint16_t(buffer[header])<<8)|buffer[header+1]);
            uint16_t command=static_cast<uint16_t>((uint16_t(buffer[header+2])<<8)|buffer[header+3]);
            frames.push_back({version,flags,vendor,command,
                Bytes(buffer.begin()+header+4,buffer.begin()+header+4+length),Bytes(buffer.begin(),buffer.begin()+total)});
            buffer.erase(buffer.begin(),buffer.begin()+total);
        }
        return frames;
    }
};
inline bool gaia_supported_batteries(const Bytes& payload) {
    if(payload.empty() || payload.size()>3)return false;
    bool seen[4]{};
    for(auto id:payload){if(id<1 || id>3 || seen[id])return false;seen[id]=true;}
    // Pudding lists left/right but automatically includes case in its level reply.
    return seen[1] && seen[2];
}
inline Status decode_gaia_battery(const Bytes& payload,std::array<Battery,3>& out) {
    if(payload.empty() || payload.size()%2 || payload.size()>6)return Status::malformed;
    bool seen[4]{};std::array<Battery,3> decoded{};
    for(size_t i=0;i<payload.size();i+=2) {
        auto id=payload[i],value=payload[i+1];
        if(id<1 || id>3 || seen[id] || (value>100 && value!=0xff))return Status::malformed;
        seen[id]=true;
        // Actual Windows frame also returned case FF. Unknown, never zero or
        // 127%, and not a charging flag to strip from a percentage.
        if(value!=0xff)decoded[id-1]={value,false}; // No charging flag here.
    }
    if(!seen[1] || !seen[2] || !seen[3] || decoded[2].percent<0)return Status::missing_case;
    out=decoded;return Status::ok;
}
}
