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
