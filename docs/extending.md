# Extending rflcpp

## Custom Codecs for Other Formats

Just as with [JSON serialization](json.md), you can specialize format-specific codecs for non-reflectable custom or third-party types. Each supported serialization format has a corresponding codec hook structure:

| Format | Header | Codec Hook Struct | Specialization Signature |
|---|---|---|---|
| **JSON** | `<rflcpp/json.hpp>` | `json_codec<T>` | `static nlohmann::json write(const T&);`<br>`static result<T> read(const nlohmann::json&, std::string_view);` |
| **YAML** | `<rflcpp/yaml.hpp>` | `yaml_codec<T>` | `static YAML::Node write(const T&);`<br>`static result<T> read(const YAML::Node&, std::string_view);` |
| **TOML** | `<rflcpp/toml.hpp>` | `toml_codec<T>` | `static toml::node write(const T&);`<br>`static result<T> read(const toml::node&, std::string_view);` |
| **XML** | `<rflcpp/xml.hpp>` | `xml_codec<T>` | `static void write(pugi::xml_node&, const T&);`<br>`static result<T> read(const pugi::xml_node&, std::string_view);` |
| **MessagePack** | `<rflcpp/msgpack.hpp>` | `msgpack_codec<T>`| `static void write(mpack_writer_t*, const T&);`<br>`static result<T> read(mpack_node_t);` |

### YAML Custom Codec Example

Here is how you would specialize the YAML codec for a custom type:

```cpp
#include <rflcpp/yaml.hpp>

namespace rflcpp {
template <>
struct yaml_codec<Color> {
    static YAML::Node write(const Color& c) {
        return YAML::Node(c.as_hex());
    }

    static result<Color> read(const YAML::Node& node, std::string_view path) {
        if (!node.IsScalar()) {
            return fail({error_kind::type_mismatch, "expected scalar hex string", std::string{path}});
        }
        return Color::from_hex(node.as<std::string>());
    }
};
} // namespace rflcpp
```

## Adding a new serialization format

All format serializers in `rflcpp` dispatch in a consistent, logical hierarchy. If you want to write a completely new backend, you should expose free functions and a custom codec following this template:

```cpp
template <class T> std::string to_FOO(const T&, foo_options = {});
template <class T> result<T>   from_FOO(std::string_view);

template <class T, class = void> struct foo_codec;   // user specializations
```

Internally, your writer should dispatch in the following priority order:

1. `foo_codec<T>` specialization (highest priority).
2. `validated<T, ...>` unwrap and check.
3. `attr<T, Attrs...>` / `field<"name", T>` unwrap and extract attribute properties.
4. Standard library primitives, optional, variants, enums, maps, and sequence containers.
5. Reflectable aggregates - inspected statically via `for_each_field`.
6. Static-assert if no dispatch match was found.

## Adding a custom field-level transformation

Wrap your type in a `field<>` to give it an explicit JSON key, or in a
`validated<>` to attach runtime invariants. Both wrappers compose:

```cpp
using SafeName = rflcpp::field<"name",
    rflcpp::validated<std::string,
        rflcpp::rules::non_empty,
        rflcpp::rules::max_length<64>>>;
```

## Adding a new built-in validation rule

Define a type that matches the `validation_rule` concept (see
[Validation](validation.md)). No registration step is needed - validation
rules are looked up by direct template instantiation, so simply use your
new rule as a `validated<T, ...>` parameter.
