// Specialize `rflcpp::json_codec<T>` to support a non-aggregate type.
// Here `Color` is serialized as a `#rrggbb` hex string.

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <rflcpp/rflcpp.hpp>

class Color {
public:
    Color() = default;
    Color(std::uint8_t r, std::uint8_t g, std::uint8_t b) : r_(r), g_(g), b_(b) {}

    std::uint8_t r() const { return r_; }
    std::uint8_t g() const { return g_; }
    std::uint8_t b() const { return b_; }

private:
    std::uint8_t r_{}, g_{}, b_{};
};

namespace rflcpp {
template <>
struct json_codec<Color> {
    static void write(detail::json::writer& w, const Color& c) {
        std::ostringstream os;
        os << '#' << std::hex << std::setfill('0')
           << std::setw(2) << +c.r()
           << std::setw(2) << +c.g()
           << std::setw(2) << +c.b();
        w.write_string(os.str());
    }

    static result<Color> read(const detail::json::value& v, std::string_view path) {
        if (v.k != detail::json::value::kind::string_ || v.s.size() != 7 || v.s[0] != '#')
            return fail({error_kind::type_mismatch, "expected #rrggbb", std::string{path}});

        auto hex = [](char c) {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + c - 'a';
            if (c >= 'A' && c <= 'F') return 10 + c - 'A';
            return -1;
        };
        auto byte = [&](int off) -> int {
            int hi = hex(v.s[off]), lo = hex(v.s[off + 1]);
            return (hi < 0 || lo < 0) ? -1 : hi * 16 + lo;
        };

        int r = byte(1), g = byte(3), b = byte(5);
        if (r < 0 || g < 0 || b < 0)
            return fail({error_kind::parse_error, "bad hex digit in color", std::string{path}});
        return Color{static_cast<std::uint8_t>(r),
                     static_cast<std::uint8_t>(g),
                     static_cast<std::uint8_t>(b)};
    }
};
} // namespace rflcpp

struct Theme {
    std::string name;
    Color       background;
    Color       foreground;
};

int main() {
    Theme t{"Dracula", Color{0x28, 0x2a, 0x36}, Color{0xf8, 0xf8, 0xf2}};
    std::cout << rflcpp::to_json(t, {.indent = 2}) << "\n";

    auto parsed = rflcpp::from_json<Theme>(
        R"({"name":"Light","background":"#ffffff","foreground":"#000000"})");
    if (parsed) std::cout << "parsed theme " << parsed->name << " ok\n";
}
