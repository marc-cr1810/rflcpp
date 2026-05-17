#include <catch2/catch_test_macros.hpp>
#include <rflcpp/reflect.hpp>
#include <rflcpp/json.hpp>
#include <rflcpp/registry.hpp>
#include <rflcpp/yaml.hpp>

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
}
#endif
