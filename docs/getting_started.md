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
