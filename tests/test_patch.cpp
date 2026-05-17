#include <catch2/catch_test_macros.hpp>
#include <rflcpp/reflect.hpp>
#include <rflcpp/json.hpp>
#include <rflcpp/patch.hpp>
#include <rflcpp/yaml.hpp>

using namespace rflcpp;

struct TestUser {
    std::string name;
    int age;
    std::optional<std::string> email;
};

TEST_CASE("Patch and diff basic operations", "[patch]") {
    TestUser u{"Alice", 20, "alice@example.com"};
    
    SECTION("diffing identical objects yields empty patch") {
        TestUser u2 = u;
        auto p = diff(u, u2);
        
        // No fields should have a value
        REQUIRE(!std::get<0>(p.values).has_value());
        REQUIRE(!std::get<1>(p.values).has_value());
        REQUIRE(!std::get<2>(p.values).has_value());
    }
    
    SECTION("diffing changed objects sets only modified fields") {
        TestUser u2{"Alice", 21, std::nullopt};
        auto p = diff(u, u2);
        
        REQUIRE(!std::get<0>(p.values).has_value()); // name unchanged
        
        REQUIRE(std::get<1>(p.values).has_value()); // age changed
        REQUIRE(*std::get<1>(p.values) == 21);
        
        REQUIRE(std::get<2>(p.values).has_value()); // email changed to nullopt
        REQUIRE(!std::get<2>(p.values)->has_value());
    }
    
    SECTION("applying a patch updates only target fields") {
        auto p_tuple = std::make_tuple(
            std::optional<std::string>(std::nullopt), // name unchanged
            std::optional<int>(25),                  // age changed
            std::optional<std::optional<std::string>>(std::optional<std::string>("new@example.com")) // email changed
        );
        patch_type<TestUser> p(p_tuple);
        
        patch(u, p);
        REQUIRE(u.name == "Alice");
        REQUIRE(u.age == 25);
        REQUIRE(u.email == "new@example.com");
    }
}

TEST_CASE("Patch serialization and deserialization", "[patch][json][yaml]") {
    SECTION("JSON Merge Patch round-trip") {
        std::string json_str = R"({"age":30,"email":null})";
        auto parsed = from_json<patch_type<TestUser>>(json_str);
        REQUIRE(parsed);
        
        // name is missing
        REQUIRE(!std::get<0>(parsed->values).has_value());
        // age is present with 30
        REQUIRE(std::get<1>(parsed->values).has_value());
        REQUIRE(*std::get<1>(parsed->values) == 30);
        // email is present with nullopt (null)
        REQUIRE(std::get<2>(parsed->values).has_value());
        REQUIRE(!std::get<2>(parsed->values)->has_value());
        
        auto serialized = to_json(*parsed);
        REQUIRE(serialized == "{\"age\":30,\"email\":null}");
    }
    
#ifdef RFLCPP_ENABLE_YAML
    SECTION("YAML Patch round-trip") {
        std::string yaml_str = "age: 30\nemail: null";
        auto parsed = from_yaml<patch_type<TestUser>>(yaml_str);
        REQUIRE(parsed);
        
        REQUIRE(!std::get<0>(parsed->values).has_value());
        REQUIRE(std::get<1>(parsed->values).has_value());
        REQUIRE(*std::get<1>(parsed->values) == 30);
        REQUIRE(std::get<2>(parsed->values).has_value());
        REQUIRE(!std::get<2>(parsed->values)->has_value());
    }
#endif
}
