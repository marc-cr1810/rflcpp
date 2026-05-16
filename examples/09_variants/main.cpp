// std::variant serialization with serde-style tagging modes.

#include <iostream>
#include <string>
#include <variant>

#include <rflcpp/rflcpp.hpp>

struct click    { int x, y; };
struct scroll   { int dy; };
struct keypress { std::string key; };

using event = std::variant<click, scroll, keypress>;

template <>
struct rflcpp::variant_policy<event> {
    static constexpr auto tagging = variant_tagging::internal;
    static constexpr std::string_view tag_field     = "type";
    static constexpr std::string_view content_field = "content";
};

int main() {
    event e1 = click{120, 240};
    event e2 = keypress{"ctrl+s"};

    std::cout << rflcpp::to_json(e1, {.indent = 2}) << "\n\n";
    std::cout << rflcpp::to_json(e2, {.indent = 2}) << "\n\n";

    auto parsed = rflcpp::from_json<event>(R"({"type":"scroll","dy":42})");
    if (parsed && std::holds_alternative<scroll>(*parsed))
        std::cout << "parsed scroll dy = " << std::get<scroll>(*parsed).dy << "\n";
}
