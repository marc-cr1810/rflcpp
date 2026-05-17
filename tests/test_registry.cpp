#include <catch2/catch_test_macros.hpp>
#include <rflcpp/reflect.hpp>
#include <rflcpp/json.hpp>
#include <rflcpp/registry.hpp>
#include <rflcpp/yaml.hpp>

#ifdef RFLCPP_ENABLE_XML
#include <rflcpp/xml.hpp>
#endif

#ifdef RFLCPP_ENABLE_TOML
#include <rflcpp/toml.hpp>
#endif

#ifdef RFLCPP_ENABLE_MSGPACK
#include <rflcpp/msgpack.hpp>
#endif

using namespace rflcpp;

struct Circle {
    double radius;
};

struct Square {
    double side;
};

using ShapeRegistry = type_registry<Circle, Square>;
using RegAny = registered_any<ShapeRegistry, "shape_type">;

TEST_CASE("Compile-time type registry round-trip", "[registry][json]") {
    SECTION("JSON serialization attaches the correct tag") {
        RegAny r = Circle{4.5};
        auto j = to_json(r);
        REQUIRE(j == "{\"radius\":4.5,\"shape_type\":\"Circle\"}");
    }
    
    SECTION("JSON deserialization matches tag to correct type") {
        std::string json_input = "{\"radius\":5.5,\"shape_type\":\"Circle\"}";
        auto parsed = from_json<RegAny>(json_input);
        REQUIRE(parsed);
        REQUIRE(parsed->value.type_name() == "Circle");
        
        auto* c = parsed->value.cast<Circle>();
        REQUIRE(c != nullptr);
        REQUIRE(c->radius == 5.5);
    }
    
    SECTION("deserialization with missing tag fails") {
        std::string json_input = "{\"radius\":5.5}";
        auto parsed = from_json<RegAny>(json_input);
        REQUIRE(!parsed);
        REQUIRE(parsed.error().kind() == error_kind::missing_field);
    }
    
    SECTION("deserialization with unknown tag fails") {
        std::string json_input = "{\"radius\":5.5,\"shape_type\":\"Triangle\"}";
        auto parsed = from_json<RegAny>(json_input);
        REQUIRE(!parsed);
        REQUIRE(parsed.error().kind() == error_kind::type_mismatch);
    }
}

#ifdef RFLCPP_ENABLE_YAML
TEST_CASE("Compile-time type registry YAML round-trip", "[registry][yaml]") {
    SECTION("YAML deserialization routes correctly") {
        std::string yaml_input = "side: 10.0\nshape_type: Square";
        auto parsed = from_yaml<RegAny>(yaml_input);
        REQUIRE(parsed);
        REQUIRE(parsed->value.type_name() == "Square");
        
        auto* s = parsed->value.cast<Square>();
        REQUIRE(s != nullptr);
        REQUIRE(s->side == 10.0);
    }
    
    SECTION("YAML serialization routes correctly") {
        RegAny r = Square{12.5};
        auto yaml_str = to_yaml(r);
        REQUIRE(yaml_str.find("side: 12.5") != std::string::npos);
        REQUIRE(yaml_str.find("shape_type: Square") != std::string::npos);
    }
}
#endif

#ifdef RFLCPP_ENABLE_XML
TEST_CASE("Compile-time type registry XML round-trip", "[registry][xml]") {
    SECTION("XML round-trip routes correctly") {
        std::string xml_input = "<root><radius>5.5</radius><shape_type>Circle</shape_type></root>";
        auto parsed = from_xml<RegAny>(xml_input);
        REQUIRE(parsed);
        REQUIRE(parsed->value.type_name() == "Circle");
        
        auto* c = parsed->value.cast<Circle>();
        REQUIRE(c != nullptr);
        REQUIRE(c->radius == 5.5);
        
        auto serialized = to_xml(*parsed);
        REQUIRE(serialized.find("<radius>5.5</radius>") != std::string::npos);
        REQUIRE(serialized.find("<shape_type>Circle</shape_type>") != std::string::npos);
    }
}
#endif

#ifdef RFLCPP_ENABLE_TOML
TEST_CASE("Compile-time type registry TOML round-trip", "[registry][toml]") {
    SECTION("TOML round-trip routes correctly") {
        std::string toml_input = "side = 7.5\nshape_type = \"Square\"\n";
        auto parsed = from_toml<RegAny>(toml_input);
        REQUIRE(parsed);
        REQUIRE(parsed->value.type_name() == "Square");
        
        auto* s = parsed->value.cast<Square>();
        REQUIRE(s != nullptr);
        REQUIRE(s->side == 7.5);
        auto serialized = to_toml(*parsed);
        REQUIRE(serialized.find("side = 7.5") != std::string::npos);
        REQUIRE(serialized.find("shape_type") != std::string::npos);
        REQUIRE(serialized.find("Square") != std::string::npos);
    }
}
#endif

#ifdef RFLCPP_ENABLE_MSGPACK
TEST_CASE("Compile-time type registry MsgPack round-trip", "[registry][msgpack]") {
    SECTION("MsgPack round-trip routes correctly") {
        RegAny original = Square{22.0};
        auto bytes = to_msgpack(original);
        REQUIRE(!bytes.empty());
        
        auto parsed = from_msgpack<RegAny>(bytes);
        REQUIRE(parsed);
        REQUIRE(parsed->value.type_name() == "Square");
        
        auto* s = parsed->value.cast<Square>();
        REQUIRE(s != nullptr);
        REQUIRE(s->side == 22.0);
    }
}
#endif
