#include <catch2/catch_test_macros.hpp>
#include <rflcpp/rflcpp.hpp>
#include <string>
#include <tuple>
#include <variant>

namespace {

struct EmbeddedLenient {
    std::string first_name;
    int user_age;

    static constexpr auto rflcpp_policies = std::make_tuple(
        rflcpp::policy::lenient{}
    );
};

struct EmbeddedCamelStrict {
    std::string first_name;
    int user_age;

    static constexpr auto rflcpp_policies = std::make_tuple(
        rflcpp::policy::strict{},
        rflcpp::policy::camel_case{}
    );
};

struct EmbeddedExternal {
    std::string first_name;

    static constexpr auto rflcpp_policies = std::make_tuple(
        rflcpp::policy::external_variant{}
    );
};

struct EmbeddedSnake {
    std::string first_name;
    int user_age;

    static constexpr auto rflcpp_policies = std::make_tuple(
        rflcpp::policy::snake_case{}
    );
};

} // namespace

TEST_CASE("Class-embedded strictness policy") {
    SECTION("Embedded Lenient allows extra fields") {
        auto res = rflcpp::from_json<EmbeddedLenient>(R"({"first_name":"Alice","user_age":30,"extra_field":"ignored"})");
        REQUIRE(res.has_value());
        REQUIRE(res->first_name == "Alice");
        REQUIRE(res->user_age == 30);
    }

    SECTION("Embedded Strict rejects extra fields") {
        auto res = rflcpp::from_json<EmbeddedCamelStrict>(R"({"firstName":"Alice","userAge":30,"extraField":"rejected"})");
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind() == rflcpp::error_kind::unknown_field);
    }
}

TEST_CASE("Class-embedded naming policy") {
    SECTION("Embedded Camel Case transformation") {
        EmbeddedCamelStrict val{"Bob", 25};
        auto json = rflcpp::to_json(val);
        REQUIRE(json.find("\"firstName\"") != std::string::npos);
        REQUIRE(json.find("\"userAge\"") != std::string::npos);
    }

    SECTION("Embedded Snake Case transformation") {
        EmbeddedSnake val{"Bob", 25};
        auto json = rflcpp::to_json(val);
        REQUIRE(json.find("\"first_name\"") != std::string::npos);
        REQUIRE(json.find("\"user_age\"") != std::string::npos);
    }
}
