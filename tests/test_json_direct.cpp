#include <catch2/catch_test_macros.hpp>
#include <rflcpp/rflcpp.hpp>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <variant>
#include <span>

namespace {

struct DirectPoint {
    double x;
    double y;
};

struct DirectPerson {
    std::string name;
    int age;
    std::vector<std::string> tags;
    std::optional<std::string> nickname;
};

struct DirectFlattened {
    DirectPoint pt;
    rflcpp::attr<std::string, rflcpp::attrs::sensitive> secret;
};

struct FlattenedHolder {
    rflcpp::attr<DirectFlattened, rflcpp::attrs::flatten> flat;
    std::string normal;
};

enum class DirectColor {
    Red,
    Green,
    Blue
};

} // namespace

TEST_CASE("Direct JSON basic primitive serialization") {
    SECTION("Booleans") {
        REQUIRE(rflcpp::json::direct::write(true) == "true");
        REQUIRE(rflcpp::json::direct::write(false) == "false");
    }

    SECTION("Integers") {
        REQUIRE(rflcpp::json::direct::write(42) == "42");
        REQUIRE(rflcpp::json::direct::write(-100) == "-100");
    }

    SECTION("Floating point numbers") {
        REQUIRE(rflcpp::json::direct::write(3.14) == "3.14");
    }

    SECTION("Strings and escaping") {
        REQUIRE(rflcpp::json::direct::write(std::string("hello")) == "\"hello\"");
        REQUIRE(rflcpp::json::direct::write(std::string("hello \"world\" \n \t")) == "\"hello \\\"world\\\" \\n \\t\"");
    }
}

TEST_CASE("Direct JSON struct serialization") {
    SECTION("Simple aggregate") {
        DirectPoint p{1.5, -2.5};
        std::string json = rflcpp::json::direct::write(p);
        REQUIRE(json == "{\"x\":1.5,\"y\":-2.5}");
    }

    SECTION("Nested complex aggregate") {
        DirectPerson p{"Alice", 30, {"developer", "dreamer"}, "Al"};
        std::string json = rflcpp::json::direct::write(p);
        REQUIRE(json == "{\"name\":\"Alice\",\"age\":30,\"tags\":[\"developer\",\"dreamer\"],\"nickname\":\"Al\"}");
    }

    SECTION("Missing optional serialization") {
        DirectPerson p{"Bob", 25, {}, std::nullopt};
        std::string json = rflcpp::json::direct::write(p);
        REQUIRE(json == "{\"name\":\"Bob\",\"age\":25,\"tags\":[],\"nickname\":null}");
    }
}

TEST_CASE("Direct JSON span serialization and overflow") {
    DirectPoint p{12.34, 56.78};
    char buf[128];
    std::span<char> sp(buf, 128);

    auto res = rflcpp::json::direct::write_to_span(sp, p);
    REQUIRE(res.has_value());
    size_t len = res.value();
    std::string json(buf, len);
    REQUIRE(json == "{\"x\":12.34,\"y\":56.78}");

    // Overflow check
    char tiny_buf[10];
    std::span<char> tiny_sp(tiny_buf, 10);
    auto overflow_res = rflcpp::json::direct::write_to_span(tiny_sp, p);
    REQUIRE(!overflow_res.has_value());
    REQUIRE(overflow_res.error().kind() == rflcpp::error_kind::out_of_range);
}

TEST_CASE("Direct JSON enums") {
    REQUIRE(rflcpp::json::direct::write(DirectColor::Green) == "\"Green\"");
}

TEST_CASE("Direct JSON flattening and sensitive attributes") {
    FlattenedHolder f{{DirectFlattened{DirectPoint{1.0, 2.0}, {"my-password"}}}, "some-text"};
    std::string json = rflcpp::json::direct::write(f);
    REQUIRE(json == "{\"pt\":{\"x\":1,\"y\":2},\"secret\":\"***\",\"normal\":\"some-text\"}");
}
