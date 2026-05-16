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
  `<experimental/meta>`. No `BOOST_FUSION_ADAPT_STRUCT`-style boilerplate.
* **JSON** serialization and deserialization, with `std::expected`-based
  error handling and `Doxygen`-friendly diagnostics.
* **Validation.** Attach compile-time rules to a value with
  `rflcpp::validated<T, Rules...>`.
* **Named fields** for opting into a wire format that differs from your C++
  identifiers, via `rflcpp::field<"json_name", T>`.
* **Extensible.** Specialize `rflcpp::json_codec<T>` to teach the library
  about your own types.
* **Pay-for-what-you-use.** Every header is independently includable.
* **Header-mostly.** Drops in via CMake `add_subdirectory` or
  `find_package(rflcpp)`.

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

See [`docs/`](docs/index.md):

- [Getting started](docs/getting_started.md)
- [Reflection façade](docs/reflection.md)
- [JSON serialization](docs/json.md)
- [Validation](docs/validation.md)
- [Extending rflcpp](docs/extending.md)

## License

MIT - see [`LICENSE`](LICENSE).
