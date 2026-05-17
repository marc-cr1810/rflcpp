# Serialization Formats

In addition to JSON, `rflcpp` natively supports multiple text-based and binary formats via separate, lightweight public headers. Each format operates with identical aggregate validation, attribute evaluation, and type-dispatch logic, making switching format backends zero-effort.

## Enabled by Compiler Option Flags

Individual format backends are guarded by compile-time preprocessor options and CMake configurations to guarantee a pay-for-what-you-use compilation model.

| Format | Header | CMake Option | Underpinned Dependency |
|---|---|---|---|
| **JSON** | `<rflcpp/json.hpp>` | Enabled by Default | [nlohmann/json](https://github.com/nlohmann/json) |
| **XML** | `<rflcpp/xml.hpp>` | `RFLCPP_ENABLE_XML` | [pugixml](https://github.com/zeux/pugixml) |
| **YAML** | `<rflcpp/yaml.hpp>` | `RFLCPP_ENABLE_YAML` | [yaml-cpp](https://github.com/jbeder/yaml-cpp) |
| **TOML** | `<rflcpp/toml.hpp>` | `RFLCPP_ENABLE_TOML` | [tomlplusplus](https://github.com/marzer/tomlplusplus) |
| **CBOR** | `<rflcpp/cbor.hpp>` | Enabled with JSON | [nlohmann/json](https://github.com/nlohmann/json) |
| **MessagePack**| `<rflcpp/msgpack.hpp>` | `RFLCPP_ENABLE_MSGPACK`| [mpack](https://github.com/ludocode/mpack) |

## Public Free-Function APIs

Every format implements the same generic, non-throwing and throwing API template:

```cpp
// Non-throwing API (preferred)
template <class T> [[nodiscard]] std::string to_FORMAT(const T& value, format_options opts = {});
template <class T> [[nodiscard]] rflcpp::result<T> from_FORMAT(std::string_view payload);

// Throwing overload (throws std::runtime_error on parse failure)
template <class T> [[nodiscard]] T from_FORMAT_or_throw(std::string_view payload);
```

For binary formats (**CBOR** and **MessagePack**), the functions return or receive raw byte arrays:

```cpp
template <class T> [[nodiscard]] std::vector<uint8_t> to_cbor(const T& value);
template <class T> [[nodiscard]] rflcpp::result<T> from_cbor(std::span<const uint8_t> data);

template <class T> [[nodiscard]] std::vector<uint8_t> to_msgpack(const T& value);
template <class T> [[nodiscard]] rflcpp::result<T> from_msgpack(std::span<const uint8_t> data);
```

---

## Code Examples

### 1. XML (e.g. `RFLCPP_ENABLE_XML=ON`)
Handles nested tags and structure values:
```cpp
#include <iostream>
#include <rflcpp/xml.hpp>

struct Config {
    std::string host;
    int port;
};

int main() {
    Config cfg{"127.0.0.1", 8080};
    
    // Serialize to XML string
    std::string xml_str = rflcpp::to_xml(cfg);
    std::cout << xml_str << "\n";
    // Outputs: <Config><host>127.0.0.1</host><port>8080</port></Config>

    // Deserialize from XML string
    auto parsed = rflcpp::from_xml<Config>(xml_str);
    if (parsed) std::cout << "Parsed port: " << parsed->port << "\n";
}
```

### 2. YAML (e.g. `RFLCPP_ENABLE_YAML=ON`)
Leverages clean block indentations:
```cpp
#include <iostream>
#include <rflcpp/yaml.hpp>

struct Config {
    std::string host;
    int port;
};

int main() {
    Config cfg{"localhost", 9000};
    
    // Serialize
    std::string yaml_str = rflcpp::to_yaml(cfg);
    std::cout << yaml_str << "\n";
    
    // Deserialize
    auto parsed = rflcpp::from_yaml<Config>(yaml_str);
    if (parsed) std::cout << "Parsed host: " << parsed->host << "\n";
}
```

### 3. TOML (e.g. `RFLCPP_ENABLE_TOML=ON`)
Excellent for application configuration structures:
```cpp
#include <iostream>
#include <rflcpp/toml.hpp>

struct DBConfig {
    std::string user;
    std::string password;
};
struct Config {
    std::string title;
    DBConfig database;
};

int main() {
    Config cfg{"App", {"admin", "secret"}};
    
    std::string toml_str = rflcpp::to_toml(cfg);
    std::cout << toml_str << "\n";
    
    auto parsed = rflcpp::from_toml<Config>(toml_str);
    if (parsed) std::cout << "Database User: " << parsed->database.user << "\n";
}
```

### 4. MessagePack (e.g. `RFLCPP_ENABLE_MSGPACK=ON`)
High-performance binary serialization using the lightweight, fast `mpack` library:
```cpp
#include <iostream>
#include <rflcpp/msgpack.hpp>

struct Point {
    double x, y;
};

int main() {
    Point pt{10.5, -20.2};
    
    // Serialize to msgpack bytes
    std::vector<uint8_t> bytes = rflcpp::to_msgpack(pt);
    std::cout << "MessagePack Size: " << bytes.size() << " bytes\n";
    
    // Deserialize
    auto parsed = rflcpp::from_msgpack<Point>(bytes);
    if (parsed) std::cout << "Point: (" << parsed->x << ", " << parsed->y << ")\n";
}
```
