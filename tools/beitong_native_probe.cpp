#include "../traffic-plugin/beitong_hid.h"
#include "../traffic-plugin/vendor/json.hpp"
#include <cstring>
#include <iostream>
int main(int argc, char** argv) {
    if (argc != 2 || std::strcmp(argv[1], "--query") != 0) {
        std::cerr << "Usage: beitong_native_probe.exe --query (one read-only status request)\n";
        return 2;
    }
    auto r = beitong::query_status();
    using nlohmann::json;
    json output = {{"percentageCandidate", r.status.percentage_candidate >= 0 ? json(r.status.percentage_candidate) : json(nullptr)},
        {"chargeStateRaw", r.status.charge_state_raw >= 0 ? json(r.status.charge_state_raw) : json(nullptr)},
        {"matches", r.matches}, {"written", r.written}, {"read", r.read}, {"reports", r.reports},
        {"error", r.error}, {"timedOut", r.timed_out}, {"stopped", r.stop},
        {"hardwareComparisonPassed", false}};
    std::cout << output.dump(2) << '\n';
    return r.status.percentage_candidate >= 0 || (r.matches == 0 && !r.error) ? 0 : 1;
}
