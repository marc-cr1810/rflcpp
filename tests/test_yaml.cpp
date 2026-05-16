#include <catch2/catch_test_macros.hpp>
#include <rflcpp/yaml.hpp>

#ifdef RFLCPP_ENABLE_YAML
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
    rflcpp::attr<std::string, rflcpp::attrs::rename<"yaml_name">> name;
    rflcpp::attr<int, rflcpp::attrs::default_to<42>> age;
    rflcpp::attr<std::string, rflcpp::attrs::sensitive> secret;
};

using Var = std::variant<int, std::string, Inner>;

struct VarStruct {
    Var data;
};

} // namespace

TEST_CASE("YAML nested structures and sequences", "[yaml][nested]") {
    Outer o{"test", {{1}, {2}}, {{"key", "val"}}};
    auto yaml = rflcpp::to_yaml(o);
    
    auto back = rflcpp::from_yaml<Outer>(yaml);
    REQUIRE(back.has_value());
    REQUIRE(back->name == "test");
    REQUIRE(back->items.size() == 2);
    REQUIRE(back->items[0].val == 1);
    REQUIRE(back->metadata.at("key") == "val");
}

TEST_CASE("YAML variants", "[yaml][variant]") {
    VarStruct v{Inner{42}};
    auto yaml = rflcpp::to_yaml(v);
    
    auto back = rflcpp::from_yaml<VarStruct>(yaml);
    REQUIRE(back.has_value());
    REQUIRE(std::holds_alternative<Inner>(back->data));
    REQUIRE(std::get<Inner>(back->data).val == 42);
}

TEST_CASE("YAML attributes", "[yaml][attribute]") {
    SECTION("Renaming and Sensitive") {
        AttrStruct s;
        s.name.value = "Alice";
        s.secret.value = "password";
        
        auto yaml = rflcpp::to_yaml(s);
        REQUIRE(yaml.find("yaml_name") != std::string::npos);
        REQUIRE(yaml.find("***")       != std::string::npos);
        
        auto back = rflcpp::from_yaml<AttrStruct>(yaml);
        REQUIRE(back.has_value());
        REQUIRE(back->name.value == "Alice");
    }

    SECTION("Default values") {
        auto back = rflcpp::from_yaml<AttrStruct>("yaml_name: Bob\nsecret: password");
        REQUIRE(back.has_value());
        REQUIRE(back->age.value == 42);
    }
}

TEST_CASE("YAML enums", "[yaml][enum]") {
    Color c = Color::Green;
    auto yaml = rflcpp::to_yaml(c);
    REQUIRE(yaml.find("Green") != std::string::npos);
    
    auto back = rflcpp::from_yaml<Color>(yaml);
    REQUIRE(back.has_value());
    REQUIRE(*back == Color::Green);
}

TEST_CASE("YAML error handling", "[yaml][error]") {
    SECTION("Type mismatch") {
        auto res = rflcpp::from_yaml<int>("name: test");
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind() == rflcpp::error_kind::type_mismatch);
    }
}

#endif // RFLCPP_ENABLE_YAML
