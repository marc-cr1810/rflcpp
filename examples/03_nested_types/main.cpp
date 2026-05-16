// Nested structs, vectors of structs, and string-keyed maps.

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <rflcpp/rflcpp.hpp>

struct Coordinate {
    double lat;
    double lon;
};

struct Stop {
    std::string  name;
    Coordinate   position;
};

struct Route {
    std::string                         id;
    std::vector<Stop>                   stops;
    std::map<std::string, std::string>  metadata;
};

int main() {
    Route r{
        .id    = "R-42",
        .stops = {
            {"Start",  {37.7749, -122.4194}},
            {"Middle", {36.7783, -119.4179}},
            {"End",    {34.0522, -118.2437}},
        },
        .metadata = {
            {"operator", "ACME Transit"},
            {"color",    "#FF8800"},
        },
    };

    std::cout << rflcpp::to_json(r, {.indent = 2}) << "\n";

    auto js = rflcpp::to_json(r);
    auto r2 = rflcpp::from_json<Route>(js);
    if (!r2) { std::cerr << r2.error().what() << "\n"; return 1; }

    std::cout << "Round-trip ok; first stop = " << r2->stops.front().name << "\n";
}
