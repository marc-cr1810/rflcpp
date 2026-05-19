#include <catch2/catch_test_macros.hpp>
#include <rflcpp/any.hpp>
#include <rflcpp/json.hpp>

using namespace rflcpp;

struct Shape {
    int id;
};

TEST_CASE("Reflected any basic", "[any]") {
    any a = Shape{123};
    REQUIRE(a.type_name() == "Shape");
    
    auto s = a.cast<Shape>();
    REQUIRE(s != nullptr);
    REQUIRE(s->id == 123);
    
    auto s2 = a.cast<int>();
    REQUIRE(s2 == nullptr);
}

TEST_CASE("Reflected any JSON serialization", "[any][json]") {
    any a = Shape{456};
    auto j = to_json(a);
    REQUIRE(j == "{\"id\":456}");
}

TEST_CASE("Reflected any in a vector", "[any][vector]") {
    std::vector<any> vec;
    vec.push_back(Shape{1});
    vec.push_back(100);
    vec.push_back(std::string{"test"});
    
    REQUIRE(vec[0].type_name() == "Shape");
    REQUIRE(vec[1].type_name() == "int");
    REQUIRE(vec[2].type_name() == "string");
}

TEST_CASE("Reflected any with smart pointers", "[any][smart_pointer]") {
    auto p1 = std::make_shared<Shape>(Shape{789});
    any a1 = p1;
    REQUIRE(a1.type_name() == "Shape");
    
    auto s1 = a1.cast<Shape>();
    REQUIRE(s1 != nullptr);
    REQUIRE(s1->id == 789);
    
    auto p2 = std::make_unique<Shape>(Shape{999});
    any a2 = std::move(p2);
    REQUIRE(a2.type_name() == "Shape");
    
    auto s2 = a2.cast<Shape>();
    REQUIRE(s2 != nullptr);
    REQUIRE(s2->id == 999);
}

