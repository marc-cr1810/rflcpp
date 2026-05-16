// Tour of the wrapper-type attributes: skip, skip_on_*, rename, aliases,
// default_to, sensitive, flatten, and skip_if_*.

#include <iostream>
#include <optional>
#include <string>

#include <rflcpp/rflcpp.hpp>

namespace at = rflcpp::attrs;

struct address {
    std::string street;
    std::string city;
};

struct user {
    rflcpp::attr<std::string,
        at::rename<"user_name">,
        at::aliases<"name", "userName">>            username;

    int                                              age;

    rflcpp::attr<std::string, at::sensitive>         password;

    rflcpp::attr<int, at::default_to<3>>             retries;

    rflcpp::attr<std::optional<std::string>,
        at::skip_if_null>                            nickname;

    rflcpp::attr<std::string, at::write_only>        computed_etag;

    rflcpp::attr<int, at::skip>                      internal_id;

    rflcpp::attr<address, at::flatten>               where;
};

int main() {
    user u{
        .username = {"ada"},
        .age = 36,
        .password = {"hunter2"},
        .retries = {5},
        .nickname = {std::nullopt},
        .computed_etag = {"abc-123"},
        .internal_id = {99},
        .where = {{"10 Downing St", "London"}},
    };

    // Expect: nickname omitted, password redacted, internal_id absent,
    // address fields flattened into the surrounding object.
    std::cout << "Serialized:\n";
    std::cout << rflcpp::to_json(u, {.indent = 2}) << "\n\n";

    auto parsed = rflcpp::from_json<user>(R"({
        "name": "grace",
        "age": 85,
        "password": "secret",
        "street": "100 Avenue", "city": "DC"
    })");

    if (parsed)
        std::cout << "parsed name    = " << parsed->username.value << "\n"
                  << "parsed retries = " << parsed->retries.value
                                          << " (default kicked in)\n"
                  << "parsed city    = " << parsed->where.value.city << "\n";
}
