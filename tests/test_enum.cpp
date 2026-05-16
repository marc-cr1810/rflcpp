#include <catch2/catch_test_macros.hpp>

#include <rflcpp/rflcpp.hpp>

namespace {

enum class color { red, green, blue };

enum class permission : unsigned {
    none  = 0,
    read  = 1,
    write = 2,
    exec  = 4,
};

} // namespace

template <>
struct rflcpp::enum_flags_policy<permission> {
    static constexpr bool is_flags = true;
};

TEST_CASE("enum_entries returns every enumerator", "[enum][reflection]") {
    constexpr auto entries = rflcpp::enum_entries<color>();
    STATIC_REQUIRE(entries.size() == 3);
    REQUIRE(entries[0].second == "red");
    REQUIRE(entries[1].second == "green");
    REQUIRE(entries[2].second == "blue");
}

TEST_CASE("enum_name maps value -> identifier", "[enum][reflection]") {
    REQUIRE(rflcpp::enum_name(color::red)   == "red");
    REQUIRE(rflcpp::enum_name(color::green) == "green");
    REQUIRE(rflcpp::enum_name(color::blue)  == "blue");
}

TEST_CASE("enum_value maps identifier -> value", "[enum][reflection]") {
    REQUIRE(rflcpp::enum_value<color>("red")   == color::red);
    REQUIRE(rflcpp::enum_value<color>("blue")  == color::blue);
    REQUIRE(rflcpp::enum_value<color>("teal")  == std::nullopt);
}

TEST_CASE("Enums round-trip through JSON as strings", "[enum][json]") {
    auto js = rflcpp::to_json(color::green);
    REQUIRE(js == "\"green\"");

    auto back = rflcpp::from_json<color>(js);
    REQUIRE(back.has_value());
    REQUIRE(*back == color::green);
}

TEST_CASE("Bit-flag enums encode as arrays of names", "[enum][flags]") {
    permission p = static_cast<permission>(
        static_cast<unsigned>(permission::read) |
        static_cast<unsigned>(permission::write));

    auto js = rflcpp::to_json(p);
    REQUIRE(js.find("read")  != std::string::npos);
    REQUIRE(js.find("write") != std::string::npos);

    auto back = rflcpp::from_json<permission>(R"(["read","exec"])");
    REQUIRE(back.has_value());
    REQUIRE(static_cast<unsigned>(*back) ==
            (static_cast<unsigned>(permission::read) |
             static_cast<unsigned>(permission::exec)));
}

TEST_CASE("Unknown enum name surfaces a type_mismatch", "[enum][error]") {
    auto r = rflcpp::from_json<color>("\"nonexistent\"");
    REQUIRE(!r.has_value());
    REQUIRE(r.error().kind() == rflcpp::error_kind::type_mismatch);
}

TEST_CASE("enum_count returns the number of enumerators", "[enum][count]") {
    STATIC_REQUIRE(rflcpp::enum_count<color>() == 3);
    STATIC_REQUIRE(rflcpp::enum_count<permission>() == 4);
}

TEST_CASE("enum_values and enum_names return matching arrays",
          "[enum][values][names]") {
    constexpr auto vs = rflcpp::enum_values<color>();
    constexpr auto ns = rflcpp::enum_names<color>();
    STATIC_REQUIRE(vs.size() == ns.size());
    STATIC_REQUIRE(vs[0] == color::red);
    STATIC_REQUIRE(ns[2] == "blue");
}

TEST_CASE("enum_index and enum_at are inverses", "[enum][index]") {
    REQUIRE(rflcpp::enum_index(color::green) == 1u);
    REQUIRE(rflcpp::enum_at<color>(1) == color::green);
    REQUIRE(rflcpp::enum_at<color>(99) == std::nullopt);

    auto raw = static_cast<color>(42);
    REQUIRE(rflcpp::enum_index(raw) == std::nullopt);
}

TEST_CASE("enum_format produces a qualified label", "[enum][format]") {
    REQUIRE(rflcpp::enum_format(color::blue) == "color::blue");
    auto raw = static_cast<color>(42);
    REQUIRE(rflcpp::enum_format(raw) == "color::<unknown>");
}

TEST_CASE("enum_contains validates names and values", "[enum][contains]") {
    REQUIRE( rflcpp::enum_contains<color>("green"));
    REQUIRE(!rflcpp::enum_contains<color>("teal"));
    REQUIRE( rflcpp::enum_contains(color::red));
    REQUIRE(!rflcpp::enum_contains(static_cast<color>(99)));
}

TEST_CASE("enum_cast safely converts from the underlying type",
          "[enum][cast]") {
    REQUIRE(rflcpp::enum_cast<color>(0) == color::red);
    REQUIRE(rflcpp::enum_cast<color>(2) == color::blue);
    REQUIRE(rflcpp::enum_cast<color>(99) == std::nullopt);
}

TEST_CASE("enum_underlying yields the raw integer", "[enum][underlying]") {
    STATIC_REQUIRE(rflcpp::enum_underlying(color::green) == 1);
    STATIC_REQUIRE(rflcpp::enum_underlying(permission::exec) == 4u);
}

TEST_CASE("enum_min / enum_max walk all enumerators", "[enum][bounds]") {
    REQUIRE(rflcpp::enum_min<color>() == 0);
    REQUIRE(rflcpp::enum_max<color>() == 2);
    REQUIRE(rflcpp::enum_min<permission>() == 0u);
    REQUIRE(rflcpp::enum_max<permission>() == 4u);
}

TEST_CASE("for_each_enum visits each enumerator at runtime",
          "[enum][iterate]") {
    std::string joined;
    rflcpp::for_each_enum<color>([&](color v, std::string_view n) {
        (void)v;
        joined += n; joined += '|';
    });
    REQUIRE(joined == "red|green|blue|");

    int count = 0;
    rflcpp::for_each_enum<color>([&](color) { ++count; });
    REQUIRE(count == 3);
}

TEST_CASE("template_for_each_enum exposes values as NTTPs",
          "[enum][template_for]") {
    int seen_indexes = 0;
    rflcpp::template_for_each_enum<color>([&]<color V>() {
        if constexpr (V == color::red)   seen_indexes |= 1;
        if constexpr (V == color::green) seen_indexes |= 2;
        if constexpr (V == color::blue)  seen_indexes |= 4;
    });
    REQUIRE(seen_indexes == 7);
}

TEST_CASE("enum_flag_format joins set flags with '|'",
          "[enum][flag_format]") {
    permission p = static_cast<permission>(
        static_cast<unsigned>(permission::read) |
        static_cast<unsigned>(permission::exec));
    auto s = rflcpp::enum_flag_format(p);
    REQUIRE(s.find("read") != std::string::npos);
    REQUIRE(s.find("exec") != std::string::npos);
    REQUIRE(s.find('|')    != std::string::npos);

    REQUIRE(rflcpp::enum_flag_format(permission::none) == "0");
}
