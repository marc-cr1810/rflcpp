#include <catch2/catch_test_macros.hpp>
#include <rflcpp/rflcpp.hpp>

namespace {

enum class Level {
    Low,
    High
};

struct Dummy {
    // No member functions
};

class Calculator {
public:
    double add(double a, double b) {
        return a + b;
    }

    std::string greet(const std::string& name) {
        return "Hello, " + name + "!";
    }

    void set_level(Level lvl) {
        current_level_ = lvl;
    }

    Level get_level() const {
        return current_level_;
    }

    void reset() {
        current_level_ = Level::Low;
        calls_count_ = 0;
    }

    int get_calls() const {
        return calls_count_;
    }

    void increment_calls() {
        calls_count_++;
    }

private:
    Level current_level_ = Level::Low;
    int calls_count_ = 0;
};

} // namespace

TEST_CASE("Dynamic invoke happy path", "[invoke][happy]") {
    Calculator calc;

    SECTION("Invoke add with exact double types") {
        auto res = rflcpp::invoke(calc, "add", std::vector<rflcpp::any>{10.5, 4.2});
        REQUIRE(res.has_value());
        REQUIRE(!res->empty());
        REQUIRE(res->type_id() == typeid(double));
        REQUIRE(*res->cast<double>() == 14.7);
    }

    SECTION("Invoke greet with exact string type") {
        auto res = rflcpp::invoke(calc, "greet", std::vector<rflcpp::any>{std::string("Ada")});
        REQUIRE(res.has_value());
        REQUIRE(res->type_id() == typeid(std::string));
        REQUIRE(*res->cast<std::string>() == "Hello, Ada!");
    }
}

TEST_CASE("Dynamic invoke void returns", "[invoke][void]") {
    Calculator calc;
    calc.increment_calls();
    REQUIRE(calc.get_calls() == 1);

    auto res = rflcpp::invoke(calc, "reset", std::vector<rflcpp::any>{});
    REQUIRE(res.has_value());
    REQUIRE(res->empty()); // void return returns an empty any
    REQUIRE(calc.get_calls() == 0);
    REQUIRE(calc.get_level() == Level::Low);
}

TEST_CASE("Dynamic invoke parameter coercion", "[invoke][coercion]") {
    Calculator calc;

    SECTION("Coerce integer any to double parameter") {
        // We pass integer 42 and 8, which should coerce to double automatically via JSON roundtrip
        auto res = rflcpp::invoke(calc, "add", std::vector<rflcpp::any>{42, 8});
        REQUIRE(res.has_value());
        REQUIRE(*res->cast<double>() == 50.0);
    }

    SECTION("Coerce string any to enum parameter") {
        // Level is reflected-native, so string "High" should coerce to Level::High enum
        auto res = rflcpp::invoke(calc, "set_level", std::vector<rflcpp::any>{std::string("High")});
        REQUIRE(res.has_value());
        REQUIRE(calc.get_level() == Level::High);
    }
}

TEST_CASE("Dynamic invoke failure handling", "[invoke][failure]") {
    Calculator calc;

    SECTION("Unknown method name") {
        auto res = rflcpp::invoke(calc, "multiply", std::vector<rflcpp::any>{2.0, 3.0});
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind() == rflcpp::error_kind::unknown_field);
    }

    SECTION("Argument count mismatch") {
        // add expects 2 arguments, we pass 3
        auto res = rflcpp::invoke(calc, "add", std::vector<rflcpp::any>{1.0, 2.0, 3.0});
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind() == rflcpp::error_kind::type_mismatch);
    }

    SECTION("Uncoercible argument type") {
        // greet expects a string, we pass a double 42.0 (uncoercible to string)
        auto res = rflcpp::invoke(calc, "greet", std::vector<rflcpp::any>{42.0});
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind() == rflcpp::error_kind::type_mismatch);
    }

    SECTION("Class with no invocable methods") {
        Dummy dummy;
        auto res = rflcpp::invoke(dummy, "reset", std::vector<rflcpp::any>{});
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind() == rflcpp::error_kind::unknown_field);
    }
}
