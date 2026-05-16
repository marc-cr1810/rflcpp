#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <rflcpp/rflcpp.hpp>

namespace {

struct Inner {
    int    n;
    double d;
};

struct Outer {
    std::string                 name;
    std::optional<std::string>  alias;
    std::vector<Inner>          items;
    bool                        active;
};

} // namespace

TEST_CASE("Primitive types round-trip through JSON", "[json][primitives]") {
    SECTION("integers") {
        auto js = rflcpp::to_json(42);
        REQUIRE(js == "42");
        auto v = rflcpp::from_json<int>(js);
        REQUIRE(v.has_value());
        REQUIRE(*v == 42);
    }
    SECTION("booleans") {
        REQUIRE(rflcpp::to_json(true)  == "true");
        REQUIRE(rflcpp::to_json(false) == "false");
        auto v = rflcpp::from_json<bool>("true");
        REQUIRE(v.has_value());
        REQUIRE(*v == true);
    }
    SECTION("strings") {
        auto js = rflcpp::to_json(std::string{"hello \"world\""});
        REQUIRE(js == R"("hello \"world\"")");
    }
}

TEST_CASE("std::optional encodes null and round-trips", "[json][optional]") {
    SECTION("nullopt becomes null") {
        std::optional<int> none{};
        REQUIRE(rflcpp::to_json(none) == "null");
        auto parsed = rflcpp::from_json<std::optional<int>>("null");
        REQUIRE(parsed.has_value());
        REQUIRE(!parsed->has_value());
    }
    SECTION("populated optional encodes the inner value") {
        std::optional<int> some{7};
        REQUIRE(rflcpp::to_json(some) == "7");
        auto parsed = rflcpp::from_json<std::optional<int>>("7");
        REQUIRE(parsed.has_value());
        REQUIRE(parsed->has_value());
        REQUIRE(**parsed == 7);
    }
}

TEST_CASE("Aggregates round-trip through JSON", "[json][aggregate]") {
    Outer o{"hello", "alias", { {1, 1.5}, {2, 2.5} }, true};
    auto js = rflcpp::to_json(o);

    auto parsed = rflcpp::from_json<Outer>(js);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->name   == "hello");
    REQUIRE(parsed->alias  == "alias");
    REQUIRE(parsed->active == true);
    REQUIRE(parsed->items.size() == 2u);
    REQUIRE(parsed->items[0].n == 1);
    REQUIRE(parsed->items[1].d == 2.5);
}

TEST_CASE("Missing required fields surface as missing_field errors",
          "[json][error][missing]") {
    auto r = rflcpp::from_json<Outer>(R"({"name":"x"})");
    REQUIRE(!r.has_value());
    REQUIRE(r.error().kind() == rflcpp::error_kind::missing_field);
}

TEST_CASE("Type mismatches surface as type_mismatch errors",
          "[json][error][type]") {
    auto r = rflcpp::from_json<int>(R"("not a number")");
    REQUIRE(!r.has_value());
    REQUIRE(r.error().kind() == rflcpp::error_kind::type_mismatch);
}

TEST_CASE("Malformed JSON surfaces as a parse_error", "[json][error][parse]") {
    auto r = rflcpp::from_json<int>("{ this is not json");
    REQUIRE(!r.has_value());
    REQUIRE(r.error().kind() == rflcpp::error_kind::parse_error);
}

TEST_CASE("Pretty printing produces newlines and indentation",
          "[json][pretty]") {
    Outer o{"hello", std::nullopt, {}, false};
    auto js = rflcpp::to_json(o, {.indent = 2});
    REQUIRE(js.find('\n')   != std::string::npos);
    REQUIRE(js.find("  ")   != std::string::npos);
}
