#include <catch2/catch_test_macros.hpp>
#include <rflcpp/xml.hpp>

#ifdef RFLCPP_ENABLE_XML
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
    std::optional<double> weight;
};

enum class Color { Red, Green, Blue };

struct AttrStruct {
    rflcpp::attr<std::string, rflcpp::attrs::rename<"xml_name">> name;
    rflcpp::attr<int, rflcpp::attrs::default_to<42>> age;
    rflcpp::attr<std::string, rflcpp::attrs::sensitive> secret;
};

using Var = std::variant<int, std::string, Inner>;

struct VarStruct {
    Var data;
};

} // namespace

TEST_CASE("XML nested structures and sequences", "[xml][nested]") {
    Outer o{"test", {{1}, {2}}, 75.5};
    auto xml = rflcpp::to_xml(o);
    
    auto back = rflcpp::from_xml<Outer>(xml);
    REQUIRE(back.has_value());
    REQUIRE(back->name == "test");
    REQUIRE(back->items.size() == 2);
    REQUIRE(back->items[0].val == 1);
    REQUIRE(back->weight == 75.5);
}

TEST_CASE("XML variants (external tagging)", "[xml][variant]") {
    VarStruct v{Inner{42}};
    auto xml = rflcpp::to_xml(v);
    
    // External variant in XML looks like <data><Inner><val>42</val></Inner></data>
    REQUIRE(xml.find("<Inner>") != std::string::npos);
    
    auto back = rflcpp::from_xml<VarStruct>(xml);
    REQUIRE(back.has_value());
    REQUIRE(std::holds_alternative<Inner>(back->data));
    REQUIRE(std::get<Inner>(back->data).val == 42);
}

TEST_CASE("XML attributes (rename, default, sensitive)", "[xml][attribute]") {
    SECTION("Renaming and Sensitive") {
        AttrStruct s;
        s.name.value = "Alice";
        s.secret.value = "password";
        
        auto xml = rflcpp::to_xml(s);
        REQUIRE(xml.find("<xml_name>Alice</xml_name>") != std::string::npos);
        REQUIRE(xml.find("***") != std::string::npos); // sensitive
        
        auto back = rflcpp::from_xml<AttrStruct>(xml);
        REQUIRE(back.has_value());
        REQUIRE(back->name.value == "Alice");
    }

    SECTION("Default values") {
        auto back = rflcpp::from_xml<AttrStruct>("<root><xml_name>Bob</xml_name><secret>...</secret></root>");
        REQUIRE(back.has_value());
        REQUIRE(back->age.value == 42);
    }
}

TEST_CASE("XML enums", "[xml][enum]") {
    Color c = Color::Green;
    auto xml = rflcpp::to_xml(c);
    REQUIRE(xml.find("Green") != std::string::npos);
    
    auto back = rflcpp::from_xml<Color>(xml);
    REQUIRE(back.has_value());
    REQUIRE(*back == Color::Green);
}

TEST_CASE("XML maps", "[xml][map]") {
    std::map<std::string, int> m = {{"a", 1}, {"b", 2}};
    auto xml = rflcpp::to_xml(m);
    
    auto back = rflcpp::from_xml<std::map<std::string, int>>(xml);
    REQUIRE(back.has_value());
    REQUIRE(back->at("a") == 1);
    REQUIRE(back->at("b") == 2);
}

TEST_CASE("XML error handling", "[xml][error]") {
    SECTION("Malformed XML") {
        auto res = rflcpp::from_xml<Outer>("<root><name>test"); // No closing tags
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind() == rflcpp::error_kind::parse_error);
    }
    SECTION("Missing field") {
        auto res = rflcpp::from_xml<Outer>("<root><weight>10</weight></root>");
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind() == rflcpp::error_kind::missing_field);
    }
}

#endif // RFLCPP_ENABLE_XML
