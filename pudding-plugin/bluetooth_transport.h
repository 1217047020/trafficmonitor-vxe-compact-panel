#pragma once
// Only the already-paired, exact-name Pudding. No discovery inquiry or pairing.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2bth.h>
#include <windows.h>
#include <bluetoothapis.h>
#include <bcrypt.h>
#include <array>
#include <cwchar>
#include <cstring>
#include "jl_auth.h"

namespace pudding {
constexpr wchar_t target_name[]=L"MOONDROP Pudding";
constexpr GUID spp={0x00001101,0,0x1000,{0x80,0,0,0x80,0x5f,0x9b,0x34,0xfb}};
struct Winsock {
    int error; WSADATA data{};
    Winsock():error(WSAStartup(MAKEWORD(2,2),&data)){}
    ~Winsock(){if(!error)WSACleanup();}
    Winsock(const Winsock&)=delete;
    Winsock& operator=(const Winsock&)=delete;
};
struct Target { BTH_ADDR address=0; unsigned matches=0; bool connected=false; DWORD error=0; };
inline Target find_target(){
    Target result;
    BLUETOOTH_DEVICE_SEARCH_PARAMS search{};search.dwSize=sizeof(search);
    search.fReturnAuthenticated=TRUE;search.fReturnRemembered=TRUE;search.fReturnConnected=TRUE;
    BLUETOOTH_DEVICE_INFO info{};info.dwSize=sizeof(info);
    auto handle=BluetoothFindFirstDevice(&search,&info);
    if(!handle){auto err=GetLastError();if(err!=ERROR_NO_MORE_ITEMS)result.error=err;return result;}
    do{
        if(_wcsicmp(info.szName,target_name)==0 && info.fAuthenticated){
            ++result.matches;result.address=info.Address.ullLong;result.connected=info.fConnected!=FALSE;
        }
    }while(BluetoothFindNextDevice(handle,&info));
    auto err=GetLastError();BluetoothFindDeviceClose(handle);
    if(err!=ERROR_NO_MORE_ITEMS)result.error=err;
    return result;
}
class Link {
    SOCKET socket_=INVALID_SOCKET;
public:
    int error=0;bool timed_out=false;
    Link()=default;Link(const Link&)=delete;
    Link& operator=(const Link&)=delete;
    ~Link(){if(socket_!=INVALID_SOCKET)closesocket(socket_);}
    bool ready(bool writing,ULONGLONG deadline){
        for(;;){
            auto now=GetTickCount64();if(now>=deadline)break;
            auto ms=deadline-now;if(ms>250)ms=250;
            timeval timeout{static_cast<long>(ms/1000),static_cast<long>((ms%1000)*1000)};
            fd_set io,errors;FD_ZERO(&io);FD_ZERO(&errors);FD_SET(socket_,&io);FD_SET(socket_,&errors);
            int rc=select(0,writing?nullptr:&io,writing?&io:nullptr,&errors,&timeout);
            if(rc==SOCKET_ERROR){error=WSAGetLastError();return false;}
            if(rc==0)continue;
            if(FD_ISSET(socket_,&errors)){
                int len=sizeof(error);if(getsockopt(socket_,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&len))error=WSAGetLastError();
                if(!error)error=WSAECONNABORTED;return false;
            }
            return true;
        }
        error=WSAETIMEDOUT;timed_out=true;return false;
    }
    bool open(BTH_ADDR address,ULONGLONG deadline){
        if(socket_!=INVALID_SOCKET){error=WSAEALREADY;return false;}
        socket_=socket(AF_BTH,SOCK_STREAM,BTHPROTO_RFCOMM);
        if(socket_==INVALID_SOCKET){error=WSAGetLastError();return false;}
        u_long mode=1;if(ioctlsocket(socket_,FIONBIO,&mode)){error=WSAGetLastError();return false;}
        SOCKADDR_BTH remote{};remote.addressFamily=AF_BTH;remote.btAddr=address;remote.serviceClassId=spp;
        if(connect(socket_,reinterpret_cast<sockaddr*>(&remote),sizeof(remote))==0)return true;
        error=WSAGetLastError();if(error!=WSAEWOULDBLOCK)return false;
        error=0;if(!ready(true,deadline))return false;
        int len=sizeof(error);if(getsockopt(socket_,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&len)){error=WSAGetLastError();return false;}
        return error==0;
    }
    bool send_all(const uint8_t* data,size_t size,ULONGLONG deadline){
        size_t pos=0;while(pos<size){if(!ready(true,deadline))return false;
            int rc=send(socket_,reinterpret_cast<const char*>(data+pos),static_cast<int>(size-pos),0);
            if(rc==SOCKET_ERROR){error=WSAGetLastError();if(error==WSAEWOULDBLOCK){error=0;continue;}return false;}
            if(rc==0){error=WSAECONNRESET;return false;}pos+=rc;
        }return true;
    }
    int receive(uint8_t* data,size_t capacity,ULONGLONG deadline){
        for(;;){if(!ready(false,deadline))return -1;
            int rc=recv(socket_,reinterpret_cast<char*>(data),static_cast<int>(capacity),0);
            if(rc==SOCKET_ERROR){error=WSAGetLastError();if(error==WSAEWOULDBLOCK){error=0;continue;}return -1;}
            if(rc==0)error=WSAECONNRESET;return rc;
        }
    }
    bool exact(uint8_t* data,size_t size,ULONGLONG deadline){
        size_t pos=0;while(pos<size){int rc=receive(data+pos,size-pos,deadline);if(rc<=0)return false;pos+=rc;}return true;
    }
    bool authenticate(ULONGLONG deadline){
        std::array<uint8_t,17> challenge{},proof{};
        if(BCryptGenRandom(nullptr,challenge.data()+1,16,BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0){error=ERROR_GEN_FAILURE;return false;}
        if(!send_all(challenge.data(),challenge.size(),deadline)||!exact(proof.data(),proof.size(),deadline))return false;
        jl_auth::Block input{};memcpy(input.data(),challenge.data()+1,16);auto expected=jl_auth::encrypt(input);
        if(proof[0]!=1 || memcmp(proof.data()+1,expected.data(),16)){error=ERROR_INVALID_DATA;return false;}
        constexpr std::array<uint8_t,5> pass={2,'p','a','s','s'};
        if(!send_all(pass.data(),pass.size(),deadline)||!exact(challenge.data(),challenge.size(),deadline))return false;
        if(challenge[0]!=0){error=ERROR_INVALID_DATA;return false;}
        memcpy(input.data(),challenge.data()+1,16);auto encrypted=jl_auth::encrypt(input);
        proof[0]=1;memcpy(proof.data()+1,encrypted.data(),16);
        std::array<uint8_t,5> ack{};
        if(!send_all(proof.data(),proof.size(),deadline)||!exact(ack.data(),ack.size(),deadline))return false;
        if(ack!=pass){error=ERROR_INVALID_DATA;return false;}return true;
    }
};
}
