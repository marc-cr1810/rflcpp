#include <iostream>
#include <rflcpp/schema.hpp>
#include <rflcpp/validation.hpp>
#include <rflcpp/attributes.hpp>

using namespace rflcpp;
using namespace rflcpp::rules;
using namespace rflcpp::attrs;

struct User {
    attr<std::string, 
         description<"The user's unique login name">,
         title<"Username">> 
    name;

    validated<int, min_value<18>, max_value<120>> age;

    attr<std::string, 
         description<"Optional bio for the user">> 
    bio;
};

int main() {
    std::cout << to_json_schema<User>() << std::endl;
    return 0;
}
