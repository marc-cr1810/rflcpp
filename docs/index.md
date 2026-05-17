# rflcpp

`rflcpp` is a modern C++26 serialization library that uses **static
reflection** (P2996, `<experimental/meta>`) to inspect user types
without macros, base classes, or boilerplate.

It is heavily inspired by [reflect-cpp](https://github.com/getml/reflect-cpp)
but trades that library's clever pre-C++26 tricks for a clean,
reflection-native implementation.

## Documentation index

* **[Getting started](getting_started.md)** — Requirements, building, CMake integration, and feature flags.
* **[Reflection façade](reflection.md)** — Dynamic traversal, type names, and tuple interop.
* **[JSON serialization](json.md)** — Basic JSON roundtrips, pretty printing, and errors.
* **[Validation](validation.md)** — Attaching invariants to numeric values and containers.
* **[Field attributes](attributes.md)** — Renaming, aliases, skip, redaction, flattening, and defaults.
* **[Serialization formats](formats.md)** — XML, YAML, TOML, CBOR, and MessagePack formats.
* **[JSON Schema generation](schema.md)** — Automatic JSON Schema generation with metadata.
* **[CLI argument parsing](cli.md)** — Deriving CLI parsers directly from reflectable structs.
* **[Reflected Any](any.md)** — Reflection-aware type erasure container.
* **[Dynamic Method Invocation](invoke.md)** — Call member functions by name with dynamic argument coercion.
* **[Merge Patches & Diffing](patch.md)** — RFC 7396 Merge Patches, object diffing, and merging.
* **[Polymorphic Type Registries](registry.md)** — Tags, registries, and dynamic deserialization.
* **[Extending rflcpp](extending.md)** — Custom codecs and adding formats.

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
