# rflcpp

> A modern C++26 reflection-based serialization library, inspired by
> [reflect-cpp](https://github.com/getml/reflect-cpp), but written from the
> ground up around the static-reflection facilities standardized by
> [P2996](https://wg21.link/p2996).

```cpp
#include <rflcpp/rflcpp.hpp>

struct User {
    std::string                name;
    int                        age;
    std::optional<std::string> email;
    std::vector<std::string>   roles;
};

int main() {
    User u{"Ada", 36, "ada@example.com", {"admin"}};

    // 1. Serialize.
    std::string js = rflcpp::to_json(u, {.indent = 2});

    // 2. Deserialize, returning std::expected<User, rflcpp::error>.
    auto parsed = rflcpp::from_json<User>(js);
    if (!parsed) std::cerr << parsed.error().what();
}
```

No macros. No mixin base classes. No external code generation. Just C++26.

## Highlights

* **Reflection-native.** Field names, types, and counts come straight from
  `<meta>` / `<experimental/meta>`. No `BOOST_FUSION_ADAPT_STRUCT`-style boilerplate.
* **Format-rich.** Natively supports **JSON** (via `nlohmann/json`), **XML** (via `pugixml`), **YAML** (via `yaml-cpp`), **TOML** (via `toml++`), and binary formats like **CBOR** and **MessagePack** (via `mpack`).
* **Attributes.** Transparently wrap fields using `rflcpp::attr<T, Attrs...>` and `rflcpp::field<"name", T>` to specify `skip`, `rename`, `aliases`, `sensitive` redaction, `flatten`, `default_to`, and more.
* **Validation.** Enforce compile-time invariants with `rflcpp::validated<T, Rules...>` checking on construction and parse boundaries.
* **JSON Schema.** Effortlessly generate Draft 2020-12 schemas from reflectable classes, fully reflecting custom field descriptions and validation constraints.
* **Merge Patches.** Track partial updates with `rflcpp::patch_type<T>`, enabling standard RFC 7396 merge patches and structural diffing (`rflcpp::diff`).
* **Type Registry.** Manage polymorphic serialization of dynamic types via compile-time registries and `rflcpp::registered_any<Registry, Tag>`.
* **CLI parsing.** Instantly derive an elegant, typed command-line arguments parser from standard configuration structs.
* **Dynamic Method Invocation.** Call class member functions dynamically by string name at runtime with automated, typesafe parameter coercion.
* **Header-mostly.** Simply drop in via CMake `add_subdirectory` or `find_package(rflcpp)`.

## Project layout

```
rflcpp/
├── CMakeLists.txt          - top-level build
├── Makefile                - convenience wrapper around CMake
├── include/rflcpp/         - public headers (this is what gets installed)
├── src/                    - internal .cpp / .hpp files
├── examples/               - copy-pasteable runnable examples
├── tests/                  - Catch2 test suite
├── docs/                   - Markdown + optional Doxygen
├── cmake/                  - install-time CMake helpers
└── submodules/Catch2/      - vendored Catch2 (git submodule)
```

## Building

```sh
git clone --recursive https://github.com/your-org/rflcpp.git
cd rflcpp
make           # configure + build (Release)
make tests     # run the test suite via CTest
make examples  # build every example under examples/
make install   # install (override with PREFIX=...)
make help      # see all available targets
```

Forgot `--recursive`? `make` will run
`git submodule update --init --recursive` for you on first use.

### Compiler support

rflcpp **requires** a C++26 toolchain with P2996 static reflection. There
is no fallback path -- the entire point of the library is to showcase
reflection-native serialization. Configuration fails hard with an
actionable message when reflection isn't available.

| Toolchain                                       | Status                                |
|-------------------------------------------------|---------------------------------------|
| GCC ≥ 16                                        | **Supported** (auto-detected, `-freflection`) |
| Bloomberg's `clang-p2996` fork                  | **Supported** (auto-detected, `-freflection-latest`) |
| Mainline Clang once P2996 lands                 | Will be picked up automatically       |
| Anything else                                   | Build refuses to configure            |

## Documentation

See the complete user guides in [`docs/`](docs/index.md):

* [Getting started](docs/getting_started.md) — Requirements, building, CMake integration, and feature flags.
* [Reflection façade](docs/reflection.md) — Dynamic traversal, type names, and tuple interop.
* [JSON serialization](docs/json.md) — Basic JSON roundtrips, pretty printing, and errors.
* [Validation](docs/validation.md) — Attaching constraints to numeric values and containers.
* [Field attributes](docs/attributes.md) — Renaming, aliases, skip, redaction, flattening, and defaults.
* [Serialization formats](docs/formats.md) — XML, YAML, TOML, CBOR, and MessagePack formats.
* [JSON Schema generation](docs/schema.md) — Automatic JSON Schema generation with metadata.
* [CLI argument parsing](docs/cli.md) — Deriving CLI parsers directly from reflectable structs.
* [Reflected Any](docs/any.md) — Reflection-aware type erasure container.
* [Dynamic Method Invocation](docs/invoke.md) — Call member functions by name with dynamic argument coercion.
* [Merge Patches & Diffing](docs/patch.md) — RFC 7396 Merge Patches, object diffing, and merging.
* [Polymorphic Type Registries](docs/registry.md) — Tags, registries, and dynamic deserialization.
* [Extending rflcpp](docs/extending.md) — Custom codecs and adding formats.

## License

MIT - see [`LICENSE`](LICENSE).
