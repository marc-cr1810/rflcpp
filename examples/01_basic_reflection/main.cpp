// Walk a struct's fields with `for_each_field` and `to_tuple`.

#include <iostream>
#include <string>

#include <rflcpp/rflcpp.hpp>

struct Person {
    std::string name;
    int         age;
    double      height_m;
};

int main() {
    Person p{"Ada Lovelace", 36, 1.65};

    std::cout << "rflcpp v" << RFLCPP_VERSION_STRING << "\n";
    std::cout << "Type has " << rflcpp::field_count_of<Person>() << " fields.\n";

    constexpr auto names = rflcpp::field_names_of<Person>();
    std::cout << "Field names:";
    for (auto n : names) std::cout << " " << n;
    std::cout << "\n\nfor_each_field:\n";

    rflcpp::for_each_field(p, [](std::string_view name, auto const& value) {
        std::cout << "  " << name << " = " << value << "\n";
    });

    auto t = rflcpp::to_tuple(p);
    std::get<1>(t) = 37;
    std::cout << "\nAfter mutation through to_tuple: age = " << p.age << "\n";
}
