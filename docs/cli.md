# Command-Line Interface (CLI) Parser

`rflcpp` features a reflection-native declarative command-line argument parser. By passing a standard reflectable configuration struct, the library automatically translates input arguments, handles option names, validates constraints, and generates professional formatted help documentation.

## Public API

```cpp
#include <rflcpp/cli.hpp>

namespace rflcpp::cli {
    // 1. Parse command-line arguments into a reflectable configuration struct T
    template <class T>
    [[nodiscard]] rflcpp::result<T> parse(int argc, const char* const* argv);

    // 2. Format and print argument options and descriptions to stdout
    template <class T>
    void print_help(std::string_view program_name = "app");
}
```

---

## Detailed Example

Here is a configuration struct utilizing attributes (descriptions and defaults) parsed automatically via the CLI engine:

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <rflcpp/rflcpp.hpp>
#include <rflcpp/cli.hpp>

namespace at = rflcpp::attrs;

struct Config {
    // 1. A required argument with a clear description
    rflcpp::attr<std::string, 
        at::description<"The server address to bind to">> host;

    // 2. An argument with a default fallback if omitted
    rflcpp::attr<int, 
        at::default_to<8080>, 
        at::description<"The server port">> port;

    // 3. A boolean flag (defaults to false, set to true if --verbose is present)
    rflcpp::attr<bool, 
        at::description<"Enable debug logs">> verbose;
};

int main(int argc, char** argv) {
    // 1. Catch help flags automatically (-h or --help)
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "-h" || arg == "--help") {
            rflcpp::cli::print_help<Config>(argv[0]);
            return 0;
        }
    }

    // 2. Parse arguments
    auto parsed = rflcpp::cli::parse<Config>(argc, argv);
    if (!parsed) {
        std::cerr << "CLI Parse Error: " << parsed.error().message() << "\n\n";
        rflcpp::cli::print_help<Config>(argv[0]);
        return 1;
    }

    std::cout << "Successfully parsed Config:\n"
              << "  Host:    " << parsed->host << "\n"
              << "  Port:    " << parsed->port << "\n"
              << "  Verbose: " << (parsed->verbose ? "true" : "false") << "\n";
}
```

---

## Syntax Options

The CLI parser supports standard command-line argument syntaxes:

1. **Option Key-Value Pairs**:
   - Equal-separated: `--host=127.0.0.1`
   - Space-separated: `--host 127.0.0.1`
2. **Boolean Flags**:
   - Passing `--verbose` sets the field to `true`.
   - Passing `--verbose=false` explicitly overrides it.
3. **Missing Keys**:
   - If a key has a `default_to<V>` attribute or is a standard `std::optional`, it defaults accordingly.
   - Otherwise, an error is returned.

## Generated Help Documentation

When calling `rflcpp::cli::print_help<Config>("my_server")`, the parser generates a clean CLI overview:

```text
Usage: my_server [options]

Options:
  --host <string>     The server address to bind to (required)
  --port <integer>     The server port (default: 8080)
  --verbose           Enable debug logs
```
