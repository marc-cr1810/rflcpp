# rflcpp

`rflcpp` is a modern C++26 serialization library that uses **static
reflection** (P2996, `<experimental/meta>`) to inspect user types
without macros, base classes, or boilerplate.

It is heavily inspired by [reflect-cpp](https://github.com/getml/reflect-cpp)
but trades that library's clever pre-C++26 tricks for a clean,
reflection-native implementation.

## Documentation index

- [Getting started](getting_started.md)
- [Reflection façade](reflection.md)
- [JSON serialization](json.md)
- [Validation](validation.md)
- [Extending rflcpp](extending.md)

## At a glance

```cpp
#include <rflcpp/rflcpp.hpp>
#include <iostream>

struct Person { std::string name; int age; };

int main() {
    Person p{"Ada", 36};
    std::cout << rflcpp::to_json(p, {.indent = 2}) << "\n";

    auto p2 = rflcpp::from_json<Person>(R"({"name":"Grace","age":85})");
    if (p2) std::cout << p2->name << "\n";
}
```
