// Tour of rflcpp's enum reflection helpers: names, lookups, safe casts,
// runtime and compile-time iteration, JSON encoding, and bit-flag support.

#include <iostream>
#include <string>

#include <rflcpp/rflcpp.hpp>

enum class log_level { trace, debug, info, warn, error };

enum class file_perm : unsigned {
    none  = 0,
    read  = 1 << 0,
    write = 1 << 1,
    exec  = 1 << 2,
};

template <>
struct rflcpp::enum_flags_policy<file_perm> {
    static constexpr bool is_flags = true;
};

struct log_entry {
    log_level   level;
    std::string message;
};

int main() {
    std::cout << "==== Basics ====\n";
    std::cout << "log_level has " << rflcpp::enum_count<log_level>()
              << " enumerators (range " << rflcpp::enum_min<log_level>()
              << ".." << rflcpp::enum_max<log_level>() << ")\n";

    std::cout << "All names: ";
    for (auto n : rflcpp::enum_names<log_level>()) std::cout << n << ' ';
    std::cout << "\n\n";

    std::cout << "==== Lookups ====\n";
    std::cout << "enum_name(warn)             = "
              << rflcpp::enum_name(log_level::warn) << "\n";
    std::cout << "enum_value(\"error\")         = "
              << rflcpp::enum_format(*rflcpp::enum_value<log_level>("error"))
              << "\n";
    std::cout << "enum_index(info)            = "
              << *rflcpp::enum_index(log_level::info) << "\n";
    std::cout << "enum_at(0)                  = "
              << rflcpp::enum_format(*rflcpp::enum_at<log_level>(0)) << "\n";
    std::cout << "enum_underlying(error)      = "
              << rflcpp::enum_underlying(log_level::error) << "\n\n";

    std::cout << "==== Safe casts ====\n";
    std::cout << "enum_cast<log_level>(2)     = "
              << (rflcpp::enum_cast<log_level>(2)
                  ? rflcpp::enum_format(*rflcpp::enum_cast<log_level>(2))
                  : "<invalid>") << "\n";
    std::cout << "enum_cast<log_level>(99)    = "
              << (rflcpp::enum_cast<log_level>(99) ? "<some>" : "<invalid>")
              << "  (rejected, as expected)\n\n";

    std::cout << "==== Iteration ====\n";
    std::cout << "for_each_enum:\n";
    rflcpp::for_each_enum<log_level>([](log_level v, std::string_view n) {
        std::cout << "  " << rflcpp::enum_underlying(v) << " -> " << n << "\n";
    });
    std::cout << "\n";

    std::cout << "template_for_each_enum (compile-time per value):\n";
    rflcpp::template_for_each_enum<log_level>([]<log_level V>() {
        std::cout << "  V=" << rflcpp::enum_underlying(V)
                  << " name=" << rflcpp::enum_name(V) << "\n";
    });
    std::cout << "\n";

    std::cout << "==== JSON ====\n";
    log_entry e{log_level::warn, "disk almost full"};
    std::cout << rflcpp::to_json(e, {.indent = 2}) << "\n\n";

    auto parsed = rflcpp::from_json<log_entry>(
        R"({"level":"error","message":"out of memory"})");
    if (parsed)
        std::cout << "parsed level = " << rflcpp::enum_name(parsed->level) << "\n\n";

    std::cout << "==== Bit-flag enum ====\n";
    auto perms = static_cast<file_perm>(
        static_cast<unsigned>(file_perm::read) |
        static_cast<unsigned>(file_perm::write));
    std::cout << "flag_format = " << rflcpp::enum_flag_format(perms) << "\n";
    std::cout << "as JSON     = " << rflcpp::to_json(perms) << "\n";
}
