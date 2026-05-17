# Getting started

## Requirements

rflcpp **requires** a C++26 toolchain with P2996 static reflection. The
build will refuse to configure otherwise (and print a message telling you
what to point CMake at).

Known-good setups:

* **GCC ≥ 16** -- auto-detected, uses `-freflection`.
* **Bloomberg's `clang-p2996` fork** -- auto-detected, uses
  `-freflection-latest`.
* Future mainline Clang releases once P2996 is upstreamed will be picked up
  automatically by the same probe.

Other requirements:

* CMake **3.28** or newer.
* Git (the tests pull Catch2 in as a submodule).

## Building

```sh
git clone --recursive https://github.com/your-org/rflcpp.git
cd rflcpp
make              # configure + build (Release)
make tests        # build & run the tests
make examples     # build all examples
sudo make install # install to /usr/local
```

If you forgot `--recursive`, the Makefile will run
`git submodule update --init --recursive` for you the first time
you invoke `make`.

Common overrides:

```sh
make BUILD_TYPE=Debug
make PREFIX=$HOME/.local install
make CXX=clang++-20 GENERATOR=Ninja
make RFLCPP_BUILD_TESTS=OFF
```

## Consuming rflcpp from another CMake project

After `make install`, downstream projects can use:

```cmake
find_package(rflcpp REQUIRED)
target_link_libraries(my_app PRIVATE rflcpp::rflcpp)
```

For in-tree consumption, simply `add_subdirectory(path/to/rflcpp)`.

## Feature Flags & Backend Support

`rflcpp` is built around a pay-for-what-you-use modular design. While the core C++26 static reflection façade is always enabled, individual serialization backends are controlled via CMake flags. When an option is enabled, the build system automatically obtains the dependency via `FetchContent` (if not found on the host system).

| CMake Option | Enabled by Default | Serialization Format | Target Library Dependency |
|---|---|---|---|
| `RFLCPP_ENABLE_JSON` | `ON` | JSON & CBOR | [nlohmann/json](https://github.com/nlohmann/json) (v3.11.3) |
| `RFLCPP_ENABLE_XML` | `OFF` | XML | [pugixml](https://github.com/zeux/pugixml) (v1.14) |
| `RFLCPP_ENABLE_YAML` | `OFF` | YAML | [yaml-cpp](https://github.com/jbeder/yaml-cpp) (v0.8.0) |
| `RFLCPP_ENABLE_TOML` | `OFF` | TOML | [tomlplusplus](https://github.com/marzer/tomlplusplus) (v3.4.0) |
| `RFLCPP_ENABLE_MSGPACK`| `ON` | MessagePack | [mpack](https://github.com/ludocode/mpack) (v1.1) |

### Enabling backends in downstream CMake projects

If you are consuming `rflcpp` in-tree using `add_subdirectory`, you can force-enable specific backends in your main `CMakeLists.txt` before calling `add_subdirectory`:

```cmake
set(RFLCPP_ENABLE_YAML ON CACHE BOOL "" FORCE)
set(RFLCPP_ENABLE_TOML ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/rflcpp)
```

Alternatively, configure the build dynamically from your command line:

```sh
cmake -S . -B build -DRFLCPP_ENABLE_YAML=ON -DRFLCPP_ENABLE_XML=ON
```
