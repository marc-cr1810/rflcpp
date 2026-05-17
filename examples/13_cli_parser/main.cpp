#include <iostream>
#include <rflcpp/cli.hpp>

using namespace rflcpp;

struct AppConfig {
    attr<int, attrs::description<"Port to listen on">> port{8080};
    attr<std::string, attrs::description<"Host address">> host{"127.0.0.1"};
    attr<bool, attrs::description<"Enable verbose logging">> verbose{false};
};

int main(int argc, char** argv) {
    auto res = cli::parse<AppConfig>(argc, argv);
    
    if (!res) {
        if (res.error().message() != "help requested") {
            std::cerr << "Error: " << res.error().message() << std::endl;
            return 1;
        }
        return 0;
    }
    
    auto cfg = *res;
    std::cout << "Configured App:" << std::endl;
    std::cout << "  Host: " << *cfg.host << std::endl;
    std::cout << "  Port: " << *cfg.port << std::endl;
    std::cout << "  Verbose: " << (*cfg.verbose ? "yes" : "no") << std::endl;
    
    return 0;
}
