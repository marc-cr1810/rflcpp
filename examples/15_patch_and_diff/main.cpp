#include <iostream>
#include <rflcpp/reflect.hpp>
#include <rflcpp/json.hpp>
#include <rflcpp/patch.hpp>

struct User {
    std::string name;
    int age;
    std::optional<std::string> email;
};

int main() {
    std::cout << "=== rflcpp C++26 Reflection-Native Patch & Diff Example ===" << std::endl;
    
    User alice{"Alice", 30, "alice@example.com"};
    std::cout << "Original Alice: name=" << alice.name << ", age=" << alice.age 
              << ", email=" << (alice.email ? *alice.email : "null") << std::endl;
              
    // 1. Parse a partial update patch payload
    std::string patch_json = R"({"age": 31, "email": null})";
    auto parsed_patch = rflcpp::from_json<rflcpp::patch_type<User>>(patch_json);
    if (!parsed_patch) {
        std::cerr << "Failed to parse patch: " << parsed_patch.error().message() << std::endl;
        return 1;
    }
    
    // Apply patch
    rflcpp::patch(alice, *parsed_patch);
    std::cout << "Patched Alice:  name=" << alice.name << ", age=" << alice.age 
              << ", email=" << (alice.email ? *alice.email : "null") << std::endl;
              
    // 2. Diffing objects
    User bob{"Bob", 25, "bob@example.com"};
    User bob_changed{"Bob", 26, std::nullopt};
    
    auto bob_diff = rflcpp::diff(bob, bob_changed);
    std::string diff_json = rflcpp::to_json(bob_diff);
    std::cout << "Bob Diff JSON: " << diff_json << std::endl;
    
    return 0;
}
