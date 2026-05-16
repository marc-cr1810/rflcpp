#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <rflcpp/rflcpp.hpp>

namespace {

enum class color { red, green, blue };

struct point {
    int x;
    int y;
};

struct profile {
    std::string                       name;
    int                               age;
    std::optional<std::string>        nickname;
    std::vector<std::string>          tags;
    color                             favorite;
};

} // namespace

TEST_CASE("Schema for a primitive type", "[schema][primitive]") {
    auto s = rflcpp::to_json_schema<int>();
    REQUIRE(s.find("\"type\":\"integer\"") != std::string::npos);
}

TEST_CASE("Schema for an enum lists every enumerator", "[schema][enum]") {
    auto s = rflcpp::to_json_schema<color>();
    REQUIRE(s.find("\"enum\"") != std::string::npos);
    REQUIRE(s.find("red")      != std::string::npos);
    REQUIRE(s.find("green")    != std::string::npos);
    REQUIRE(s.find("blue")     != std::string::npos);
}

TEST_CASE("Schema for an aggregate describes properties and required",
          "[schema][object]") {
    auto s = rflcpp::to_json_schema<profile>();

    REQUIRE(s.find("\"type\":\"object\"") != std::string::npos);
    REQUIRE(s.find("\"properties\"")      != std::string::npos);
    REQUIRE(s.find("\"name\"")            != std::string::npos);
    REQUIRE(s.find("\"age\"")             != std::string::npos);
    REQUIRE(s.find("\"required\"")        != std::string::npos);

    // `nickname` is optional, so it must NOT appear in "required".
    auto req_pos = s.find("\"required\"");
    REQUIRE(req_pos != std::string::npos);
    REQUIRE(s.substr(req_pos).find("nickname") == std::string::npos);
}
