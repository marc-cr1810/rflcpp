// `rflcpp::field<"name", T>` assigns an explicit JSON key,
// decoupling the wire name from the C++ identifier.

#include <iostream>
#include <string>

#include <rflcpp/rflcpp.hpp>

struct PersonA {
    std::string first_name;
    std::string last_name;
};

struct PersonB {
    rflcpp::field<"first_name", std::string> firstName;
    rflcpp::field<"last_name",  std::string> lastName;
};

int main() {
    PersonA a{"Ada", "Lovelace"};
    PersonB b{ {"Ada"}, {"Lovelace"} };

    std::cout << "PersonA: " << rflcpp::to_json(a) << "\n";
    std::cout << "PersonB: " << rflcpp::to_json(b) << "\n";

    auto parsed = rflcpp::from_json<PersonB>(
        R"({"first_name":"Grace","last_name":"Hopper"})");
    if (parsed) std::cout << "parsed firstName = " << *parsed->firstName << "\n";
}
