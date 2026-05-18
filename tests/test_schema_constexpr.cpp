#include <catch2/catch_test_macros.hpp>
#include <rflcpp/rflcpp.hpp>
#include <string_view>

namespace {

struct ConstexprUser {
    int id;
    std::string name;
    std::optional<std::string> nickname;
};

} // namespace

TEST_CASE("Compile-time JSON Schema generation") {
    // 1. Validate that the schema can be fully resolved inside a static_assert!
    static_assert(rflcpp::to_json_schema_view<bool>() == "{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\",\"title\":\"bool\",\"$id\":\"rflcpp://bool\",\"type\":\"boolean\"}");
    static_assert(rflcpp::to_json_schema_view<int>() == "{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\",\"title\":\"int\",\"$id\":\"rflcpp://int\",\"type\":\"integer\"}");

    // 2. Validate that the schema string is correct at runtime as well
    std::string schema = rflcpp::to_json_schema<ConstexprUser>();
    REQUIRE(schema.find("\"type\":\"object\"") != std::string::npos);
    REQUIRE(schema.find("\"properties\":{") != std::string::npos);
    REQUIRE(schema.find("\"id\":{\"type\":\"integer\"}") != std::string::npos);
    REQUIRE(schema.find("\"name\":{\"type\":\"string\"}") != std::string::npos);
    REQUIRE(schema.find("\"nickname\":{\"anyOf\":[{\"type\":\"string\"},{\"type\":\"null\"}]}") != std::string::npos);
    REQUIRE(schema.find("\"required\":[\"id\",\"name\"]") != std::string::npos);
}
