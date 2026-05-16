#include <string>
#include <tuple>

#include <catch2/catch_test_macros.hpp>

#include <rflcpp/rflcpp.hpp>

namespace {

struct Point { int x; int y; int z; };
struct Empty {};

} // namespace

TEST_CASE("field_count_of reports the number of non-static data members",
          "[reflect][field_count]") {
    STATIC_REQUIRE(rflcpp::field_count_of<Point>() == 3u);
    STATIC_REQUIRE(rflcpp::field_count_of<Empty>() == 0u);
}

TEST_CASE("for_each_field exposes mutable references", "[reflect][for_each_field]") {
    Point p{1, 2, 3};

    SECTION("mutating via for_each_field changes the original") {
        rflcpp::for_each_field(p, [](auto, int& v) { v *= 10; });
        REQUIRE(p.x == 10);
        REQUIRE(p.y == 20);
        REQUIRE(p.z == 30);
    }

    SECTION("read-only visitation works on const objects") {
        const Point cp{4, 5, 6};
        int sum = 0;
        rflcpp::for_each_field(cp, [&](auto, int const& v) { sum += v; });
        REQUIRE(sum == 15);
    }
}

TEST_CASE("to_tuple returns references that alias the original",
          "[reflect][to_tuple]") {
    Point p{1, 2, 3};
    auto t = rflcpp::to_tuple(p);

    std::get<0>(t) = 99;
    std::get<2>(t) = 77;

    REQUIRE(p.x == 99);
    REQUIRE(p.y == 2);
    REQUIRE(p.z == 77);
}

TEST_CASE("from_tuple reconstructs an aggregate", "[reflect][from_tuple]") {
    Point q = rflcpp::from_tuple<Point>(std::tuple{4, 5, 6});
    REQUIRE(q.x == 4);
    REQUIRE(q.y == 5);
    REQUIRE(q.z == 6);
}
