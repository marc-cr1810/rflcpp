#include <catch2/catch_test_macros.hpp>
#include <rflcpp/cli.hpp>

using namespace rflcpp;

struct CliConfig {
    int port = 8080;
    std::string host = "localhost";
    bool verbose = false;
};

TEST_CASE("CLI parser basic", "[cli]") {
    char* argv[] = {(char*)"prog", (char*)"--port", (char*)"9000", (char*)"--host", (char*)"example.com", (char*)"--verbose"};
    int argc = 6;
    
    auto res = cli::parse<CliConfig>(argc, argv);
    REQUIRE(res.has_value());
    REQUIRE(res->port == 9000);
    REQUIRE(res->host == "example.com");
    REQUIRE(res->verbose == true);
}

TEST_CASE("CLI parser with equal sign", "[cli]") {
    char* argv[] = {(char*)"prog", (char*)"--port=1234"};
    int argc = 2;
    
    auto res = cli::parse<CliConfig>(argc, argv);
    REQUIRE(res.has_value());
    REQUIRE(res->port == 1234);
}

TEST_CASE("CLI parser invalid value", "[cli]") {
    char* argv[] = {(char*)"prog", (char*)"--port", (char*)"abc"};
    int argc = 3;
    
    auto res = cli::parse<CliConfig>(argc, argv);
    REQUIRE(!res.has_value());
}

struct CliConfigWithShort {
    rflcpp::field<"port", int, rflcpp::attrs::short_name<'p'>> port = 8080;
    rflcpp::field<"host", std::string, rflcpp::attrs::short_name<'o'>> host = std::string("localhost");
    rflcpp::field<"verbose", bool, rflcpp::attrs::short_name<'v'>> verbose = false;
};

struct CliConfigStrict {
    int value = 0;
};
template <>
struct rflcpp::strict_policy<CliConfigStrict> {
    static constexpr bool strict = true;
};

TEST_CASE("CLI parser short flags", "[cli]") {
    char* argv[] = {(char*)"prog", (char*)"-p", (char*)"9000", (char*)"-o", (char*)"example.com", (char*)"-v"};
    int argc = 6;
    
    auto res = cli::parse<CliConfigWithShort>(argc, argv);
    REQUIRE(res.has_value());
    REQUIRE(res->port.value == 9000);
    REQUIRE(res->host.value == "example.com");
    REQUIRE(res->verbose.value == true);
}

TEST_CASE("CLI parser short flags with equal", "[cli]") {
    char* argv[] = {(char*)"prog", (char*)"-p=1234"};
    int argc = 2;
    
    auto res = cli::parse<CliConfigWithShort>(argc, argv);
    REQUIRE(res.has_value());
    REQUIRE(res->port.value == 1234);
}

TEST_CASE("CLI parser strict error on unknown", "[cli]") {
    char* argv[] = {(char*)"prog", (char*)"--unknown", (char*)"123"};
    int argc = 3;
    
    auto res = cli::parse<CliConfigStrict>(argc, argv);
    REQUIRE(!res.has_value());
    REQUIRE(res.error().kind() == error_kind::unknown_field);
}

