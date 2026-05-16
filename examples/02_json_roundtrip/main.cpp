// Round-trip a struct through JSON with optional and container fields.

#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <rflcpp/rflcpp.hpp>

struct Address {
    std::string street;
    std::string city;
    std::string country;
};

struct User {
    std::string                  username;
    int                          age;
    std::optional<std::string>   email;
    std::vector<std::string>     roles;
    Address                      address;
};

int main() {
    User u{
        .username = "ada",
        .age      = 36,
        .email    = "ada@example.com",
        .roles    = {"admin", "engineer"},
        .address  = {"10 Downing St", "London", "UK"},
    };

    std::cout << "Pretty JSON:\n"  << rflcpp::to_json(u, {.indent = 2}) << "\n\n";
    std::string compact = rflcpp::to_json(u);
    std::cout << "Compact JSON:\n" << compact << "\n\n";

    auto parsed = rflcpp::from_json<User>(compact);
    if (!parsed) {
        std::cerr << "parse error: " << parsed.error().what() << "\n";
        return 1;
    }

    std::cout << "Round-tripped user.username = " << parsed->username << "\n";
    std::cout << "Re-serialized              = " << rflcpp::to_json(*parsed) << "\n";
}
