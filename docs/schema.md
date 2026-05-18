# JSON Schema Generation

`rflcpp` enables compile-time metadata extraction and dynamic generation of standard **JSON Schema (Draft 2020-12)** descriptions from reflectable types.

## Public API

```cpp
#include <rflcpp/schema.hpp>

template <class T>
[[nodiscard]] std::string to_json_schema();
```

## Features

The schema generator automatically translates C++ type safety rules, attributes, and validation rules directly into standard JSON Schema properties:

1. **Required Fields**: Fields that are not wrapped in `std::optional` or do not contain default fallback values are automatically listed in the schema's `required` list.
2. **Metadata Annotations**: `rflcpp::attrs::description<"text">` and `rflcpp::attrs::title<"text">` attributes are extracted from transparent wrappers and mapped to `description` and `title` keys.
3. **Invariants / Validation Rules**: Custom numerical and string constraints from `rflcpp::validated<T, Rules...>` translate directly to schema boundaries:
   - `rules::minimum<V>` / `rules::exclusive_minimum<V>` -> `minimum`
   - `rules::maximum<V>` / `rules::exclusive_maximum<V>` -> `maximum`
   - `rules::min_length<L>` -> `minLength`
   - `rules::max_length<L>` -> `maxLength`

---

## Detailed Example

Here is how to generate a JSON Schema that incorporates validation rules, titles, and descriptions:

```cpp
#include <iostream>
#include <optional>
#include <string>
#include <rflcpp/rflcpp.hpp>
#include <rflcpp/schema.hpp>

namespace at = rflcpp::attrs;
namespace rl = rflcpp::rules;

// 1. Constrained string type
using Username = rflcpp::validated<std::string,
    rl::min_length<3>,
    rl::max_length<20>>;

// 2. Constrained integer type
using Age = rflcpp::validated<int,
    rl::minimum<0>,
    rl::maximum<120>>;

struct Profile {
    // Wrap field with title and description attributes
    rflcpp::attr<Username,
        at::title<"User Handle">,
        at::description<"Unique username between 3 and 20 characters">> username;

    rflcpp::attr<Age,
        at::description<"User age in years">> age;

    std::optional<std::string> email; // Optional field (not required)
};

int main() {
    std::string schema_json = rflcpp::to_json_schema<Profile>();
    std::cout << schema_json << "\n";
}
```

### Generated JSON Schema Output

The output of `to_json_schema<Profile>()` will be a Draft 2020-12 compliant schema:

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "Profile",
  "$id": "rflcpp://Profile",
  "type": "object",
  "properties": {
    "username": {
      "type": "string",
      "minLength": 3,
      "maxLength": 20,
      "title": "User Handle",
      "description": "Unique username between 3 and 20 characters"
    },
    "age": {
      "type": "integer",
      "minimum": 0,
      "maximum": 120,
      "description": "User age in years"
    },
    "email": {
      "anyOf": [
        {
          "type": "string"
        },
        {
          "type": "null"
        }
      ]
    }
  },
  "required": [
    "username",
    "age"
  ]
}
```

---

## Compile-Time JSON Schema Generation

In addition to dynamic runtime schema generation, `rflcpp` enables fully **compile-time static schema generation**.

### Consteval Schema API

```cpp
#include <rflcpp/schema.hpp>

namespace rflcpp {
    // Generates the JSON Schema at compile-time as a zero-copy std::string_view
    template <class T>
    consteval std::string_view to_json_schema_view();
}
```

### Static Verification Example

Because `to_json_schema_view` is `consteval`, you can statically verify, inspect, or build upon schemas during compilation using `static_assert`:

```cpp
#include <rflcpp/rflcpp.hpp>
#include <rflcpp/schema.hpp>

struct Config {
    int port;
    std::string host;
};

// Assert schema properties at compile-time!
static_assert(rflcpp::to_json_schema_view<int>() == 
    "{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\",\"title\":\"int\",\"$id\":\"rflcpp://int\",\"type\":\"integer\"}"
);

static_assert(rflcpp::to_json_schema_view<Config>().find("\"required\":[\"port\",\"host\"]") != std::string_view::npos);

int main() {
    // High-performance runtime retrieval (zero overhead, copies from pre-compiled static view)
    std::string schema = rflcpp::to_json_schema<Config>();
}
```

See `examples/19_embedded_policies_and_constexpr_schema/` for a complete example.
