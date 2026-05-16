// Reflection-derived `==`, `<=>`, `hash`, `format`, plus JSON Schema export.

#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <rflcpp/rflcpp.hpp>

enum class status { ok, warn, error };

struct point  { int x; int y; int z; };

struct report {
    std::string                name;
    status                     state;
    std::vector<std::string>   tags;
    std::optional<int>         level;
};

int main() {
    point a{1, 2, 3}, b{1, 2, 3}, c{1, 2, 4};

    std::cout << "a == b:      " << rflcpp::derive::equal(a, b) << "\n";
    std::cout << "a == c:      " << rflcpp::derive::equal(a, c) << "\n";
    std::cout << "a <=> c < 0: " << (rflcpp::derive::compare(a, c) < 0) << "\n";
    std::cout << "hash(a):     " << rflcpp::derive::hash(a)  << "\n";
    std::cout << "hash(b):     " << rflcpp::derive::hash(b)  << "  (same as a)\n";
    std::cout << "format(a):   " << rflcpp::derive::format(a) << "\n\n";

    std::cout << "=== JSON Schema for `report` ===\n";
    std::cout << rflcpp::to_json_schema<report>() << "\n";
}
