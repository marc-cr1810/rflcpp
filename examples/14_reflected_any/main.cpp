#include <iostream>
#include <vector>
#include <rflcpp/any.hpp>
#include <rflcpp/json.hpp>

using namespace rflcpp;

struct Circle {
    double radius;
};

struct Square {
    double side;
};

int main() {
    std::vector<any> shapes;
    shapes.push_back(Circle{5.0});
    shapes.push_back(Square{10.0});
    shapes.push_back(Circle{2.5});

    std::cout << "Heterogeneous shapes in JSON:" << std::endl;
    std::cout << to_json(shapes, {.indent = 2}) << std::endl;

    std::cout << "\nInspecting types at runtime:" << std::endl;
    for (const auto& s : shapes) {
        std::cout << " - " << s.type_name() << std::endl;
    }

    return 0;
}
