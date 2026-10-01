#include "battery_state.h"
#include <iostream>
#include <stdexcept>
using nlohmann::json;
int main() {
    int tests = 0;
    auto d = json{{"id", battery::device_id}, {"percentage",85}, {"connected",true}, {"stale",false}, {"lastSeen",1000000}};
    auto response = [&] (json devices) { return json{{"jsonrpc","2.0"},{"id",1},{"result",{{"devices",devices}}}}.dump(); };
    auto check = [&] (std::string s, int expected) { ++tests; if (battery::parse(s,1000010) != expected) throw std::runtime_error("state test failed " + std::to_string(tests)); };
    check(response(json::array({d})),85);
    for (int p : {0,100}) { auto x=d; x["percentage"]=p; check(response(json::array({x})),p); }
    for (json p : {json(-1),json(101),json(nullptr),json(85.5),json("85"),json(true)}) {
        auto x=d; x["percentage"]=p; check(response(json::array({x})),battery::unavailable);
    }
    auto x=d; x["connected"]=false; check(response(json::array({x})),battery::offline);
    x=d; x["stale"]=true; check(response(json::array({x})),battery::offline);
    x=d; x["lastSeen"]=900000; check(response(json::array({x})),battery::unavailable);
    x=d; x["lastSeen"]=1010000; check(response(json::array({x})),battery::unavailable);
    x=d; x["id"]="other"; check(response(json::array({x})),battery::offline);
    check(response(json::array()),battery::offline);
    check(response(json::array({d,d})),battery::unavailable);
    check("invalid json",battery::unavailable);
    check(R"({"jsonrpc":"2.0","id":1,"error":{"code":-32001}})",battery::unavailable);
    check(R"({"jsonrpc":"2.0","id":2,"result":{"devices":[]}})",battery::unavailable);
    x=d; x.erase("stale"); check(response(json::array({x})),battery::unavailable);
    x=d; x["lastSeen"]=0;
    auto j=json::parse(response(json::array({x})));
    check(j.dump(),battery::unavailable);
    j["result"]["updatedAt"]=1000000; check(j.dump(),85);
    j["result"]["updatedAt"]=900000; check(j.dump(),battery::unavailable);
    j["result"]["updatedAt"]=nullptr; check(j.dump(),battery::unavailable);
    std::cout << tests << " state tests passed\n";
}
