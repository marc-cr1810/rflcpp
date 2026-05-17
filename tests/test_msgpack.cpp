#include <catch2/catch_test_macros.hpp>
#include <rflcpp/msgpack.hpp>

using namespace rflcpp;

struct Simple {
    int i;
    std::string s;
};

TEST_CASE("MessagePack round-trip", "[msgpack]") {
    Simple s{42, "hello"};
    auto data = to_msgpack(s);
    auto res = from_msgpack<Simple>(data);
    
    REQUIRE(res.has_value());
    REQUIRE(res->i == 42);
    REQUIRE(res->s == "hello");
}

TEST_CASE("MessagePack with validated type", "[msgpack][validation]") {
    using Positive = validated<int, rules::min_value<1>>;
    struct Val { Positive p; };
    
    Val v{*Positive::make(10)};
    auto data = to_msgpack(v);
    auto res = from_msgpack<Val>(data);
    
    REQUIRE(res.has_value());
    REQUIRE(res->p.get() == 10);
    
    // Test invalid data
    // (We could manually construct an invalid msgpack but for now just roundtrip)
}
