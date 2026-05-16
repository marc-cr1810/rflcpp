#include <string>

#include <catch2/catch_test_macros.hpp>

#include <rflcpp/rflcpp.hpp>

using rflcpp::validated;
namespace rules = rflcpp::rules;

namespace {
using Percent  = validated<int, rules::min_value<0>, rules::max_value<100>>;
using NonEmpty = validated<std::string, rules::non_empty>;
using Username = validated<std::string, rules::non_empty, rules::max_length<8>>;
} // namespace

TEST_CASE("validated::make accepts values that satisfy every rule",
          "[validation][make][happy]") {
    auto p = Percent::make(50);
    REQUIRE(p.has_value());
    REQUIRE(p->get() == 50);

    auto s = NonEmpty::make("hello");
    REQUIRE(s.has_value());
}

TEST_CASE("validated::make rejects values that violate a rule",
          "[validation][make][failure]") {
    SECTION("min_value violated") {
        auto r = Percent::make(-1);
        REQUIRE(!r.has_value());
        REQUIRE(r.error().kind() == rflcpp::error_kind::validation_failed);
    }
    SECTION("max_value violated") {
        auto r = Percent::make(101);
        REQUIRE(!r.has_value());
        REQUIRE(r.error().kind() == rflcpp::error_kind::validation_failed);
    }
    SECTION("non_empty violated") {
        REQUIRE(!NonEmpty::make("").has_value());
    }
    SECTION("max_length violated") {
        REQUIRE(!Username::make("way-too-long-name").has_value());
    }
}

TEST_CASE("validated throwing constructor throws rflcpp_error on failure",
          "[validation][throwing]") {
    REQUIRE_THROWS_AS(Percent{1000},      rflcpp::rflcpp_error);
    REQUIRE_NOTHROW (Percent{42});
}
