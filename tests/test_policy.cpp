#include <string>

#include <catch2/catch_test_macros.hpp>

#include <rflcpp/rflcpp.hpp>

namespace {

struct camel_user {
    std::string first_name;
    std::string last_name;
    int         user_age_in_years;
};

struct strict_user {
    std::string name;
};

struct timestamped {
    int created;
};
struct named_user : timestamped {
    std::string name;
};

struct private_holder {
public:
    int  shown = 1;
private:
    int  internal = 99;
};

} // namespace

template <>
struct rflcpp::naming_policy<camel_user> {
    static constexpr auto style = case_style::camel_case;
    static std::string transform(std::string_view n) {
        return detail::apply_case(n, style);
    }
};

template <>
struct rflcpp::access_policy<private_holder> {
    static constexpr auto mode = access_mode::public_only;
};

template <>
struct rflcpp::strict_policy<strict_user> {
    static constexpr bool strict = true;
};

TEST_CASE("naming_policy::camel_case rewrites member names",
          "[policy][naming]") {
    camel_user u{"Ada", "Lovelace", 36};
    auto js = rflcpp::to_json(u);
    REQUIRE(js.find("firstName")       != std::string::npos);
    REQUIRE(js.find("lastName")        != std::string::npos);
    REQUIRE(js.find("userAgeInYears")  != std::string::npos);
}

TEST_CASE("naming_policy round-trips through the renamed keys",
          "[policy][naming][roundtrip]") {
    auto back = rflcpp::from_json<camel_user>(
        R"({"firstName":"Grace","lastName":"Hopper","userAgeInYears":85})");
    REQUIRE(back.has_value());
    REQUIRE(back->first_name == "Grace");
    REQUIRE(back->last_name == "Hopper");
    REQUIRE(back->user_age_in_years == 85);
}

TEST_CASE("access_policy::public_only hides private members from reflection",
          "[policy][access]") {
    constexpr auto field_count = rflcpp::field_count_of<private_holder>();
    STATIC_REQUIRE(field_count == 1);

    private_holder h;
    auto js = rflcpp::to_json(h);
    REQUIRE(js.find("shown")    != std::string::npos);
    REQUIRE(js.find("internal") == std::string::npos);
}

TEST_CASE("Base classes are flattened by default", "[policy][base]") {
    named_user u{};
    u.created = 7;
    u.name    = "Ada";
    auto js = rflcpp::to_json(u);

    REQUIRE(js.find("\"created\":7")    != std::string::npos);
    REQUIRE(js.find("\"name\":\"Ada\"") != std::string::npos);
    REQUIRE(js.find("timestamped")      == std::string::npos);

    auto back = rflcpp::from_json<named_user>(js);
    REQUIRE(back.has_value());
    REQUIRE(back->created == 7);
    REQUIRE(back->name    == "Ada");
}

TEST_CASE("strict_policy rejects unknown keys", "[policy][strict]") {
    auto res = rflcpp::from_json<strict_user>(R"({"name":"Alice","age":30})");
    REQUIRE(!res.has_value());
    REQUIRE(res.error().kind() == rflcpp::error_kind::unknown_field);
}
