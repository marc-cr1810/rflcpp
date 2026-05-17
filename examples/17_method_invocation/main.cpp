// examples/17_method_invocation/main.cpp
// SPDX-License-Identifier: MIT

#include <iostream>
#include <string>
#include <vector>
#include <rflcpp/rflcpp.hpp>

// A simple user account service
class UserService {
public:
    void set_username(std::string name) {
        username_ = std::move(name);
        std::cout << "  [UserService] Username updated to: " << username_ << "\n";
    }

    std::string get_username() const {
        return username_;
    }

    bool authenticate(std::string password) {
        bool success = (password == "super-secret");
        std::cout << "  [UserService] Authentication attempt: " << (success ? "SUCCESS" : "FAILED") << "\n";
        return success;
    }

    void reset() {
        username_ = "Anonymous";
        std::cout << "  [UserService] Service reset to defaults.\n";
    }

private:
    std::string username_ = "Anonymous";
};

// A simulated network RPC request payload
struct RPCRequest {
    std::string method;
    std::vector<rflcpp::any> arguments;
};

// Automated RPC Dispatcher that routes request dynamically to the service
void dispatch_rpc(UserService& service, const RPCRequest& req) {
    std::cout << ">>> Dispatching RPC command: '" << req.method << "' with " << req.arguments.size() << " arguments...\n";
    
    // Core Dynamic Invocation!
    auto result = rflcpp::invoke(service, req.method, req.arguments);
    
    if (result.has_value()) {
        std::cout << "    Status: SUCCESS\n";
        if (result->empty()) {
            std::cout << "    Returned: void\n";
        } else {
            // Print based on type
            if (result->type_id() == typeid(std::string)) {
                std::cout << "    Returned (string): \"" << *result->cast<std::string>() << "\"\n";
            } else if (result->type_id() == typeid(bool)) {
                std::cout << "    Returned (bool): " << (*result->cast<bool>() ? "true" : "false") << "\n";
            }
        }
    } else {
        std::cout << "    Status: FAILED\n";
        std::cout << "    Error Kind: " << static_cast<int>(result.error().kind()) << "\n";
        std::cout << "    Error Message: " << result.error().message() << "\n";
        if (!result.error().path().empty()) {
            std::cout << "    Error Path: " << result.error().path() << "\n";
        }
    }
    std::cout << "\n";
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << "rflcpp C++26 Reflection-Driven RPC Dispatcher Showcase\n";
    std::cout << "=========================================================\n\n";

    UserService service;

    // 1. Dispatch "set_username" command
    RPCRequest req1{
        .method = "set_username",
        .arguments = { std::string("Ada Lovelace") }
    };
    dispatch_rpc(service, req1);

    // 2. Dispatch "get_username" command
    RPCRequest req2{
        .method = "get_username",
        .arguments = {}
    };
    dispatch_rpc(service, req2);

    // 3. Dispatch "authenticate" command (Success path)
    RPCRequest req3{
        .method = "authenticate",
        .arguments = { std::string("super-secret") }
    };
    dispatch_rpc(service, req3);

    // 4. Dispatch "authenticate" command (Failure path)
    RPCRequest req4{
        .method = "authenticate",
        .arguments = { std::string("wrong-password") }
    };
    dispatch_rpc(service, req4);

    // 5. Dispatch "reset" command (Void return)
    RPCRequest req5{
        .method = "reset",
        .arguments = {}
    };
    dispatch_rpc(service, req5);

    // 6. Negative Path: Non-existent method name
    RPCRequest req6{
        .method = "delete_account",
        .arguments = {}
    };
    dispatch_rpc(service, req6);

    // 7. Negative Path: Parameter count mismatch
    RPCRequest req7{
        .method = "set_username",
        .arguments = { std::string("Grace"), 123 } // set_username expects 1 argument, 2 passed
    };
    dispatch_rpc(service, req7);

    // 8. Negative Path: Uncoercible parameter type
    RPCRequest req8{
        .method = "authenticate",
        .arguments = { 9999 } // expects a string password, int passed
    };
    dispatch_rpc(service, req8);

    return 0;
}
