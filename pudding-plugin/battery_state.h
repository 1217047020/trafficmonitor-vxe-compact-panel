#pragma once
#include <array>
#include <cstdint>
#include <string>
#include "rcsp_protocol.h"

namespace pudding {
enum class Status {
    waiting, ok, offline, ambiguous, enumeration_error, winsock_error,
    connect_error, auth_error, timeout, transport_error, unsupported,
    device_error, malformed, missing_case, negotiation_error
};
struct Battery { int percent=-1; bool charging=false; };
struct Reading {
    Status status=Status::waiting;
    // RCSP order: left, right, case. No value is populated from GetTargetInfo.
    std::array<Battery,3> batteries{};
    uint64_t timestamp=0;
    int error=0;
    int device_status=-1;
    unsigned matches=0;
    unsigned api_version=0,protocol_version=0;
    unsigned stage=0; // 0 target/connect, 1 API, 2 supported list, 3 levels.
    Bytes battery_wire; // Only the non-identifying battery frame, never TargetInfo.
};

// Candidate standard RCSP ADV battery TLV. NOT yet observed on this Pudding.
// Missing, duplicate, truncated or out-of-range case data never becomes 0%.
inline Status decode_adv(const Bytes& payload,std::array<Battery,3>& out) {
    std::vector<std::pair<uint8_t,Bytes>> attrs;
    if(!attributes(payload,attrs))return Status::malformed;
    bool found=false;
    std::array<Battery,3> decoded{};
    for(const auto& attr:attrs) {
        if(attr.first!=0)continue;
        if(found || attr.second.size()!=3)return Status::malformed;
        found=true;
        for(size_t i=0;i<3;++i) {
            auto raw=attr.second[i];int value=raw&0x7f;
            if(value<=100)decoded[i]={value,(raw&0x80)!=0};
        }
    }
    if(!found || decoded[2].percent<0)return Status::missing_case;
    out=decoded;return Status::ok;
}
inline const wchar_t* status_text(Status status) {
    switch(status) {
    case Status::waiting:return L"等待采集";
    case Status::ok:return L"GAIA 电量已采集";
    case Status::offline:return L"布丁未连接或没有已配对设备";
    case Status::ambiguous:return L"发现多台同名设备，停止选择";
    case Status::enumeration_error:return L"蓝牙设备枚举失败";
    case Status::winsock_error:return L"Winsock 初始化失败";
    case Status::connect_error:return L"串口连接失败";
    case Status::auth_error:return L"杰理双向认证失败";
    case Status::timeout:return L"读取超时";
    case Status::transport_error:return L"蓝牙连接中断";
    case Status::unsupported:return L"设备不支持所需 GAIA 电量命令";
    case Status::device_error:return L"设备拒绝查询";
    case Status::malformed:return L"收到无效协议数据";
    case Status::missing_case:return L"响应中没有有效仓电量";
    case Status::negotiation_error:return L"GAIA 版本协商失败或不是布丁支持的版本";
    }
    return L"不可用";
}
// Actual Windows / App comparisons passed at case23 and case84 (2026-10-02).
// Only valid 0..100 values are enabled; charging high-bit encoding is unverified.
// Unit fixtures alone must never count as hardware proof.
inline constexpr bool pudding_battery_validated=true;
inline std::wstring display(const Reading& reading,size_t index,uint64_t now,bool enabled=pudding_battery_validated) {
    if(index>=3)return L"不可用";
    if(reading.status==Status::offline)return L"离线";
    if(reading.status==Status::waiting)return L"等待";
    if(reading.status==Status::unsupported)return L"待适配";
    if(reading.status!=Status::ok || !reading.timestamp || now<reading.timestamp || now-reading.timestamp>90000)return L"不可用";
    if(!enabled)return L"待适配";
    auto percent=reading.batteries[index].percent;
    return percent>=0 && percent<=100 ? std::to_wstring(percent)+L"%" : L"不可用";
}
}
