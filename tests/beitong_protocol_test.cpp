#include "../traffic-plugin/beitong_protocol.h"
#include <iostream>
#include <stdexcept>
void check(bool condition) {
    if (!condition) throw std::runtime_error("Beitong protocol assertion failed");
}
int main() {
    auto packet = beitong::status_request();
    check(packet.size() == 64 && packet[0] == 2 && packet[1] == 0x15);
    for (std::size_t i = 2; i < packet.size(); ++i) check(packet[i] == 0);
    // Prefix of the actual receiver report; all remaining bytes were zero.
    packet[2] = 0x3f; packet[4] = 0x50; packet[5] = 1;
    auto status = beitong::parse_status(packet.data(), packet.size());
    check(status.percentage_candidate == 63 && status.charge_state_raw == 0);
    packet[4] = 0x51;
    check(beitong::parse_status(packet.data(), 64).charge_state_raw == 1);
    check(beitong::parse_status(nullptr, 64).percentage_candidate == -1);
    check(beitong::parse_status(packet.data(), 63).percentage_candidate == -1);
    check(beitong::parse_status(packet.data(), 65).percentage_candidate == -1);
    for (unsigned percent : {0u, 1u, 50u, 100u}) {
        packet[2] = static_cast<std::uint8_t>(percent);
        check(beitong::parse_status(packet.data(), 64).percentage_candidate == static_cast<int>(percent));
    }
    for (unsigned percent : {101u, 255u}) {
        packet[2] = static_cast<std::uint8_t>(percent);
        check(beitong::parse_status(packet.data(), 64).percentage_candidate == -1);
    }
    packet[2] = 63;
    packet[0] = 1;
    check(beitong::parse_status(packet.data(), 64).percentage_candidate == -1);
    packet[0] = 2; packet[1] = 0x55;
    check(beitong::parse_status(packet.data(), 64).percentage_candidate == -1);
    check(beitong::target_path(L"\\\\?\\HID#VID_20BC&PID_5159&MI_01&COL01#test"));
    check(!beitong::target_path(L"\\\\?\\HID#VID_20BC&PID_5159&MI_01&COL02#test"));
    check(beitong::target_caps(0x20BC, 0x5159, 0xFF, 3, 64, 64, 0));
    check(!beitong::target_caps(0x20DD, 0x5159, 0xFF, 3, 64, 64, 0));
    check(!beitong::target_caps(0x20BC, 0x5159, 1, 6, 64, 64, 0));
    check(!beitong::target_caps(0x20BC, 0x5159, 0xFF, 3, 64, 64, 64));
    std::cout << "Beitong protocol tests passed\n";
}
