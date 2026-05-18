// 18_direct_json_serialization/main.cpp - Zero-Copy JSON Serializer Example
// SPDX-License-Identifier: MIT

#include <array>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include <rflcpp/rflcpp.hpp>

struct SubRecord {
    double value;
    bool active;
};

struct DataRecord {
    int id;
    std::string name;
    std::vector<int> codes;
    SubRecord sub;
};

int main() {
    DataRecord record{
        .id = 42,
        .name = "Zero-Copy JSON Demonstration",
        .codes = {100, 200, 300},
        .sub = {.value = 3.14159, .active = true}
    };

    // --- Scenario A: Zero-Allocation Writing to a Stack Buffer ---
    std::array<char, 512> buffer{};
    
    // Serialize directly to the stack buffer using write_to_span
    auto result = rflcpp::json::direct::write_to_span(std::span{buffer}, record);
    if (result) {
        std::cout << "Successfully serialized to stack buffer (0 allocations):\n";
        std::cout << std::string_view{buffer.data(), *result} << "\n\n";
    } else {
        std::cerr << "Serialization failed: buffer size too small.\n";
    }

    // --- Scenario B: Direct Fast JSON String Writing ---
    // Serializes directly, skipping the nlohmann JSON AST creation entirely!
    std::string fast_json = rflcpp::json::direct::write(record);
    std::cout << "Directly serialized JSON string (extremely fast, no AST):\n";
    std::cout << fast_json << "\n";
}
