#include <string>

#include <catch2/catch_test_macros.hpp>

#include <rflcpp/rflcpp.hpp>

namespace {
struct point { int x; int y; };
struct named  { std::string name; int value; };
} // namespace

TEST_CASE("derive::equal walks members", "[derive][equal]") {
    REQUIRE( rflcpp::derive::equal(point{1, 2}, point{1, 2}));
    REQUIRE(!rflcpp::derive::equal(point{1, 2}, point{1, 3}));
    REQUIRE( rflcpp::derive::equal(named{"a", 1}, named{"a", 1}));
    REQUIRE(!rflcpp::derive::equal(named{"a", 1}, named{"b", 1}));
}

TEST_CASE("derive::compare gives a three-way ordering", "[derive][compare]") {
    REQUIRE(rflcpp::derive::compare(point{1, 2}, point{1, 2}) == 0);
    REQUIRE(rflcpp::derive::compare(point{1, 1}, point{1, 2}) <  0);
    REQUIRE(rflcpp::derive::compare(point{2, 0}, point{1, 9}) >  0);
}

TEST_CASE("derive::hash is reasonable and stable", "[derive][hash]") {
    auto h1 = rflcpp::derive::hash(point{1, 2});
    auto h2 = rflcpp::derive::hash(point{1, 2});
    auto h3 = rflcpp::derive::hash(point{1, 3});
    REQUIRE(h1 == h2);
    REQUIRE(h1 != h3);
}

TEST_CASE("derive::format produces a readable representation",
          "[derive][format]") {
    auto s = rflcpp::derive::format(named{"ada", 7});
    REQUIRE(s.find("ada") != std::string::npos);
    REQUIRE(s.find("name=") != std::string::npos);
    REQUIRE(s.find("value=7") != std::string::npos);
}
