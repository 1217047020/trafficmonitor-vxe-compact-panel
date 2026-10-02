#include "battery_query.h"
#include <cstdio>
int main() {
    auto reading=pudding::query();
    printf("Pudding read-only probe: matches=%u status=%d error=%d device_status=%d\n",
        reading.matches,int(reading.status),reading.error,reading.device_status);
    printf("hardware battery validation: %s\n",pudding::pudding_battery_validated
        ? "two battery states compared; valid numeric output enabled; charging encoding pending"
        : "pending; numeric output disabled");
    // No Bluetooth address, challenge/proof, device serial or raw TargetInfo log.
    printf("negotiated GAIA API=%u transport=%u stage=%u\n",reading.api_version,reading.protocol_version,reading.stage);
    if(reading.status==pudding::Status::ok)printf("GAIA readout: left=%d right=%d case=%d\n",
        reading.batteries[0].percent,reading.batteries[1].percent,reading.batteries[2].percent);
    if(!reading.battery_wire.empty()){printf("battery wire: ");for(auto byte:reading.battery_wire)printf("%02X ",byte);puts("");}
    return reading.status==pudding::Status::ok ? 0 : 2;
}
