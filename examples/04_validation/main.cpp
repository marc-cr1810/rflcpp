// Attach compile-time validation rules to runtime values.

#include <iostream>
#include <string>

#include <rflcpp/rflcpp.hpp>

using rflcpp::validated;
namespace rules = rflcpp::rules;

using Age      = validated<int, rules::min_value<0>, rules::max_value<130>>;
using Username = validated<std::string, rules::non_empty, rules::max_length<32>>;

int main() {
    try {
        Age      a{42};
        Username u{"ada"};
        std::cout << "Created age=" << a.get() << " user=" << u.get() << "\n";
    } catch (const rflcpp::rflcpp_error& e) {
        std::cerr << "unexpected: " << e.what() << "\n";
        return 1;
    }

    if (auto bad = Age::make(999); !bad)
        std::cout << "Rejected as expected: " << bad.error().what() << "\n";

    if (auto bad = Username::make(""); !bad)
        std::cout << "Rejected as expected: " << bad.error().what() << "\n";
}
