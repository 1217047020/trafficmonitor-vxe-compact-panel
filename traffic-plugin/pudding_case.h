#pragma once
// Include Winsock2 before the Windows headers from the host interface.
#include "../pudding-plugin/battery_query.h"
#include "compact_model.h"

namespace compact {
inline void update_pudding_case(SharedState& state, const pudding::Reading& reading) {
    BatterySample sample;
    sample.completed = reading.timestamp;
    sample.error = static_cast<unsigned>(reading.error);
    sample.matches = reading.matches;
    sample.timeout = reading.status == pudding::Status::timeout;
    sample.stopped = reading.status == pudding::Status::unsupported || reading.status == pudding::Status::ambiguous;
    // Independent Bluetooth worker: don't stretch the HID cadence/stale limit.
    sample.valid_for_ms = 90000;
    if (reading.status == pudding::Status::offline) sample.value = vxe::offline;
    else if (pudding::pudding_battery_validated && reading.status == pudding::Status::ok &&
             reading.batteries[2].percent >= 0 && reading.batteries[2].percent <= 100)
        sample.value = reading.batteries[2].percent; // Case ID3 only; never ear ID1/2.
    std::wstring detail = pudding::status_text(reading.status);
    if (reading.api_version) detail += L"；GAIA API " + std::to_wstring(reading.api_version) +
        L" / transport " + std::to_wstring(reading.protocol_version);
    state.earcase(sample, detail);
}
}
