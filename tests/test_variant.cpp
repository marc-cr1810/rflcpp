// `variant_policy<V>` is keyed on the variant type, so each tagging strategy
// gets its own variant type with distinct alternative types -- otherwise the
// policy specializations would collide.

#include <string>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include <rflcpp/rflcpp.hpp>

namespace external_test  { struct click { int x; int y; }; struct scroll { int dy; }; }
namespace internal_test  { struct click { int x; int y; }; struct scroll { int dy; }; }
namespace adjacent_test  { struct click { int x; int y; }; struct scroll { int dy; }; }
namespace untagged_test  { struct one { int a; }; struct two { std::string s; }; }

using ext_event = std::variant<external_test::click, external_test::scroll>;
using int_event = std::variant<internal_test::click, internal_test::scroll>;
using adj_event = std::variant<adjacent_test::click, adjacent_test::scroll>;
using unt_event = std::variant<untagged_test::one, untagged_test::two>;

template <>
struct rflcpp::variant_policy<int_event> {
    static constexpr auto tagging = variant_tagging::internal;
    static constexpr std::string_view tag_field = "type";
    static constexpr std::string_view content_field = "content";
};

template <>
struct rflcpp::variant_policy<adj_event> {
    static constexpr auto tagging = variant_tagging::adjacent;
    static constexpr std::string_view tag_field = "kind";
    static constexpr std::string_view content_field = "data";
};

template <>
struct rflcpp::variant_policy<unt_event> {
    static constexpr auto tagging = variant_tagging::untagged;
    static constexpr std::string_view tag_field = "type";
    static constexpr std::string_view content_field = "content";
};

TEST_CASE("Externally tagged variant - default", "[variant][external]") {
    ext_event e = external_test::click{1, 2};
    auto js = rflcpp::to_json(e);
    REQUIRE(js.find("click") != std::string::npos);

    auto back = rflcpp::from_json<ext_event>(js);
    REQUIRE(back.has_value());
    REQUIRE(std::holds_alternative<external_test::click>(*back));
    REQUIRE(std::get<external_test::click>(*back).x == 1);
}

TEST_CASE("Internally tagged variant", "[variant][internal]") {
    int_event e = internal_test::scroll{42};
    auto js = rflcpp::to_json(e);
    REQUIRE(js.find("\"type\":\"scroll\"") != std::string::npos);
    REQUIRE(js.find("\"dy\":42")           != std::string::npos);

    auto back = rflcpp::from_json<int_event>(js);
    REQUIRE(back.has_value());
    REQUIRE(std::holds_alternative<internal_test::scroll>(*back));
    REQUIRE(std::get<internal_test::scroll>(*back).dy == 42);
}

TEST_CASE("Adjacently tagged variant", "[variant][adjacent]") {
    adj_event e = adjacent_test::scroll{10};
    auto js = rflcpp::to_json(e);
    REQUIRE(js.find("\"kind\":\"scroll\"") != std::string::npos);
    REQUIRE(js.find("\"data\"")            != std::string::npos);

    auto back = rflcpp::from_json<adj_event>(js);
    REQUIRE(back.has_value());
    REQUIRE(std::holds_alternative<adjacent_test::scroll>(*back));
}

TEST_CASE("Untagged variant tries alternatives in order", "[variant][untagged]") {
    unt_event e = untagged_test::two{"hello"};
    auto js = rflcpp::to_json(e);
    // `two` has a string field s, so should look like {"s":"hello"}
    REQUIRE(js.find("hello") != std::string::npos);

    auto back = rflcpp::from_json<unt_event>(js);
    REQUIRE(back.has_value());
    REQUIRE(std::holds_alternative<untagged_test::two>(*back));
}

TEST_CASE("Unknown variant tag surfaces an error",
          "[variant][error]") {
    auto back = rflcpp::from_json<int_event>(R"({"type":"unknown"})");
    REQUIRE(!back.has_value());
}
