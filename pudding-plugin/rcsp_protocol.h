#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include <utility>
namespace pudding {
using Bytes=std::vector<uint8_t>;
struct Frame {uint8_t flags=0,opcode=0;Bytes body;};
inline Bytes request(uint8_t opcode,uint8_t sequence,const Bytes& params){
    if(params.size()>1023)return {};
    size_t size=params.size()+1;
    Bytes out={0xfe,0xdc,0xba,0xc0,opcode,static_cast<uint8_t>(size>>8),static_cast<uint8_t>(size),sequence};
    out.insert(out.end(),params.begin(),params.end());out.push_back(0xef);return out;
}
// RFCOMM is a byte stream, not a message transport. Bound memory and frame size.
class Framer {
    Bytes buffer;
public:
    bool invalid=false;
    size_t pending()const{return buffer.size();}
    std::vector<Frame> feed(const uint8_t* data,size_t size){
        std::vector<Frame> result;
        if(size>4096 || buffer.size()+size>8192){invalid=true;buffer.clear();return result;}
        if(size)buffer.insert(buffer.end(),data,data+size);
        while(buffer.size()>=3){
            if(buffer[0]!=0xfe||buffer[1]!=0xdc||buffer[2]!=0xba){invalid=true;buffer.erase(buffer.begin());continue;}
            if(buffer.size()<7)break;
            size_t length=(size_t(buffer[5])<<8)|buffer[6];
            if(length>1024){invalid=true;buffer.erase(buffer.begin());continue;}
            if(buffer.size()<length+8)break;
            if(buffer[length+7]!=0xef){invalid=true;buffer.erase(buffer.begin());continue;}
            result.push_back({buffer[3],buffer[4],Bytes(buffer.begin()+7,buffer.begin()+7+length)});
            buffer.erase(buffer.begin(),buffer.begin()+length+8);
        }
        return result;
    }
};
inline bool response(const Frame& f,uint8_t opcode,uint8_t sequence){
    return f.flags==0 && f.opcode==opcode && f.body.size()>=2 && f.body[1]==sequence;
}
// TLV length includes the type byte, but not the length byte itself.
inline bool attributes(const Bytes& data,std::vector<std::pair<uint8_t,Bytes>>& out){
    std::vector<std::pair<uint8_t,Bytes>> parsed;
    for(size_t i=0;i<data.size();){
        size_t length=data[i];if(length<1 || length+1>data.size()-i)return false;
        parsed.emplace_back(data[i+1],Bytes(data.begin()+i+2,data.begin()+i+length+1));i+=length+1;
    }
    out=std::move(parsed);return true;
}
}
