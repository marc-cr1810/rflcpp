#include <catch2/catch_test_macros.hpp>
#include <rflcpp/toml.hpp>

#ifdef RFLCPP_ENABLE_TOML
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

struct Inner {
    int val;
    bool operator==(const Inner&) const = default;
};

struct Outer {
    std::string name;
    std::vector<Inner> items;
    std::map<std::string, std::string> metadata;
};

enum class Color { Red, Green, Blue };

struct AttrStruct {
    rflcpp::attr<std::string, rflcpp::attrs::rename<"toml_name">> name;
    rflcpp::attr<int, rflcpp::attrs::default_to<42>> age;
    rflcpp::attr<std::string, rflcpp::attrs::sensitive> secret;
};

using Var = std::variant<int, std::string, Inner>;

struct VarStruct {
    Var data;
};

} // namespace

TEST_CASE("TOML nested tables and arrays", "[toml][nested]") {
    Outer o{"test", {{1}, {2}}, {{"key", "val"}}};
    auto toml = rflcpp::to_toml(o);
    
    auto back = rflcpp::from_toml<Outer>(toml);
    REQUIRE(back.has_value());
    REQUIRE(back->name == "test");
    REQUIRE(back->items.size() == 2);
    REQUIRE(back->items[0].val == 1);
    REQUIRE(back->metadata.at("key") == "val");
}

TEST_CASE("TOML variants", "[toml][variant]") {
    VarStruct v{Inner{42}};
    auto toml = rflcpp::to_toml(v);
    
    auto back = rflcpp::from_toml<VarStruct>(toml);
    REQUIRE(back.has_value());
    REQUIRE(std::holds_alternative<Inner>(back->data));
    REQUIRE(std::get<Inner>(back->data).val == 42);
}

TEST_CASE("TOML attributes", "[toml][attribute]") {
    SECTION("Renaming and Sensitive") {
        AttrStruct s;
        s.name.value = "Alice";
        s.secret.value = "password";
        
        auto toml = rflcpp::to_toml(s);
        REQUIRE(toml.find("toml_name") != std::string::npos);
        REQUIRE(toml.find("***")       != std::string::npos);
        
        auto back = rflcpp::from_toml<AttrStruct>(toml);
        REQUIRE(back.has_value());
        REQUIRE(back->name.value == "Alice");
    }

    SECTION("Default values") {
        // Test that default_to works when field is missing
        auto back = rflcpp::from_toml<AttrStruct>("toml_name = 'Bob'\nsecret = '...'");
        REQUIRE(back.has_value());
        REQUIRE(back->age.value == 42);
    }
}

TEST_CASE("TOML enums", "[toml][enum]") {
    Color c = Color::Green;
    auto toml = rflcpp::to_toml(c);
    REQUIRE(toml.find("Green") != std::string::npos);
    
    auto back = rflcpp::from_toml<Color>(toml);
    REQUIRE(back.has_value());
    REQUIRE(*back == Color::Green);
}

TEST_CASE("TOML error handling", "[toml][error]") {
    SECTION("Parse error") {
        auto res = rflcpp::from_toml<Outer>("this is [not toml");
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind() == rflcpp::error_kind::parse_error);
    }
    SECTION("Type mismatch") {
        auto res = rflcpp::from_toml<int>("value = 'test'");
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind() == rflcpp::error_kind::type_mismatch);
    }
}

#endif // RFLCPP_ENABLE_TOML
