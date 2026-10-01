#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}
#include "native_protocol.h"
#include <memory>
#include <vector>
namespace vxe {
struct Handle {
    HANDLE h;
    explicit Handle(HANDLE value):h(value){}
    ~Handle(){if(valid())CloseHandle(h);}
    Handle(const Handle&)=delete;
    bool valid() const{return h && h!=INVALID_HANDLE_VALUE;}
};
struct InfoSet {
    HDEVINFO h;
    explicit InfoSet(HDEVINFO value):h(value){}
    ~InfoSet(){if(h!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(h);}
};
struct QueryResult {
    int value=unavailable;
    unsigned matches=0;
    DWORD written=0,read=0,error=0;
    bool timed_out=false,stop=false;
    Packet reply{};
};
struct IoContext {
    OVERLAPPED ov{};
    Packet buffer{};
    IoContext(){ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);}
    ~IoContext(){if(ov.hEvent)CloseHandle(ov.hEvent);}
};
inline void cancel(HANDLE device,std::unique_ptr<IoContext>& io,QueryResult& result){
    CancelIoEx(device,&io->ov);
    if(WaitForSingleObject(io->ov.hEvent,100)!=WAIT_OBJECT_0){result.stop=true;io.release();}
}
// Retain event/report/OVERLAPPED until completion. A driver that refuses
// cancellation causes one quarantined allocation and a stopped worker, not a
// dangling kernel buffer, UI hang or an unbounded retry/allocation loop.
inline bool transfer(HANDLE device,bool writing,Packet& packet,QueryResult& result){
    auto io=std::make_unique<IoContext>();
    if(!io->ov.hEvent){result.error=GetLastError();return false;}
    if(writing)io->buffer=packet;
    DWORD count=0;
    BOOL ok=writing ? WriteFile(device,io->buffer.data(),17,&count,&io->ov)
                    : ReadFile(device,io->buffer.data(),17,&count,&io->ov);
    if(!ok){
        result.error=GetLastError();
        if(result.error!=ERROR_IO_PENDING)return false;
        DWORD wait=WaitForSingleObject(io->ov.hEvent,500);
        if(wait!=WAIT_OBJECT_0){
            result.timed_out=wait==WAIT_TIMEOUT;
            result.error=result.timed_out?ERROR_TIMEOUT:GetLastError();
            cancel(device,io,result);
            return false;
        }
        if(!GetOverlappedResult(device,&io->ov,&count,FALSE)){result.error=GetLastError();return false;}
    }
    result.error=0;
    if(writing)result.written=count;
    else{result.read=count;packet=io->buffer;}
    return count==17;
}
inline QueryResult query() noexcept {
    QueryResult result;
    try{
        Handle mutex(CreateMutexW(nullptr,FALSE,L"Local\\VXE_R1_Battery_Read_373B_10C9"));
        if(!mutex.valid()){result.error=GetLastError();return result;}
        DWORD owned=WaitForSingleObject(mutex.h,0);
        if(owned!=WAIT_OBJECT_0 && owned!=WAIT_ABANDONED){result.error=ERROR_BUSY;return result;}
        struct Unlock{HANDLE h;~Unlock(){ReleaseMutex(h);}} unlock{mutex.h};
        GUID guid;HidD_GetHidGuid(&guid);
        InfoSet devices(SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE));
        if(devices.h==INVALID_HANDLE_VALUE){result.error=GetLastError();return result;}
        std::wstring selected;
        for(DWORD i=0;;++i){
            SP_DEVICE_INTERFACE_DATA entry{};entry.cbSize=sizeof(entry);
            if(!SetupDiEnumDeviceInterfaces(devices.h,nullptr,&guid,i,&entry)){
                if(GetLastError()!=ERROR_NO_MORE_ITEMS){result.error=GetLastError();return result;}
                break;
            }
            DWORD required=0;
            SetupDiGetDeviceInterfaceDetailW(devices.h,&entry,nullptr,0,&required,nullptr);
            if(required<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || required>65536)continue;
            std::vector<BYTE> bytes(required);
            auto detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(bytes.data());detail->cbSize=sizeof(*detail);
            if(!SetupDiGetDeviceInterfaceDetailW(devices.h,&entry,detail,required,nullptr,nullptr))continue;
            if(!target_path(detail->DevicePath))continue;
            Handle probe(CreateFileW(detail->DevicePath,0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));
            if(!probe.valid()){result.error=GetLastError();return result;}
            HIDD_ATTRIBUTES attributes{};attributes.Size=sizeof(attributes);
            if(!HidD_GetAttributes(probe.h,&attributes)){result.error=GetLastError();return result;}
            PHIDP_PREPARSED_DATA pp=nullptr;
            if(!HidD_GetPreparsedData(probe.h,&pp)){result.error=GetLastError();return result;}
            HIDP_CAPS caps{};auto status=HidP_GetCaps(pp,&caps);HidD_FreePreparsedData(pp);
            if(status!=HIDP_STATUS_SUCCESS || !target_caps(attributes.VendorID,attributes.ProductID,caps.UsagePage,caps.Usage,caps.InputReportByteLength,caps.OutputReportByteLength))continue;
            ++result.matches;selected=detail->DevicePath;
        }
        if(!result.matches){result.value=offline;return result;}
        if(result.matches!=1){result.error=ERROR_DUPLICATE_TAG;return result;}
        Handle device(CreateFileW(selected.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
        if(!device.valid()){result.error=GetLastError();return result;}
        // Same per-handle input ring size used by verified hidapi 0.15.0.
        if(!HidD_SetNumInputBuffers(device.h,64)){result.error=GetLastError();return result;}
        if(!HidD_FlushQueue(device.h)){result.error=GetLastError();return result;}
        // Arm input before sending the command: a fast receiver can reply
        // immediately, and its USB input path must already have a pending IRP.
        auto reader=std::make_unique<IoContext>();
        if(!reader->ov.hEvent){result.error=GetLastError();return result;}
        DWORD received=0;
        BOOL immediate=ReadFile(device.h,reader->buffer.data(),17,&received,&reader->ov);
        if(!immediate && GetLastError()!=ERROR_IO_PENDING){result.error=GetLastError();return result;}
        Packet command=request();
        if(!transfer(device.h,true,command,result)){
            if(!immediate)cancel(device.h,reader,result);
            return result;
        }
        if(!immediate){
            DWORD wait=WaitForSingleObject(reader->ov.hEvent,500);
            if(wait!=WAIT_OBJECT_0){
                result.timed_out=wait==WAIT_TIMEOUT;
                result.error=result.timed_out?ERROR_TIMEOUT:GetLastError();
                cancel(device.h,reader,result);
                if(result.timed_out && !result.stop)result.value=offline;
                return result;
            }
            if(!GetOverlappedResult(device.h,&reader->ov,&received,FALSE)){
                result.error=GetLastError();
                if(result.error==ERROR_DEVICE_NOT_CONNECTED || result.error==ERROR_OPERATION_ABORTED)result.value=offline;
                return result;
            }
        }
        result.read=received;result.reply=reader->buffer;
        if(received!=17){
            return result;
        }
        result.value=parse(result.reply.data(),result.read);
    }catch(...){result.value=unavailable;}
    return result;
}
}
