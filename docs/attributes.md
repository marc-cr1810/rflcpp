# Field and Struct Attributes

In `rflcpp`, field attributes allow you to customize how your C++ struct data is represented on the wire. Field metadata is declared declaratively using transparent compile-time wrappers:

1. `rflcpp::attr<T, Attrs...>`: Declares field metadata while preserving the default reflected member name.
2. `rflcpp::field<"name", T, Attrs...>`: Declares field metadata and explicitly overrides the wire field name to `"name"`.

Both wrappers act as transparent wrappers; they implicitly convert to `T&` and `const T&` and support dereference (`operator*` and `operator->`).

## Built-in Attributes (`rflcpp::attrs::*`)

All attributes reside in the `rflcpp::attrs` namespace.

| Attribute | Category | Description |
|---|---|---|
| `skip` | Exclude | Completely ignores the field during both serialization and deserialization. |
| `skip_on_write` (alias `read_only`) | Exclude | Ignores the field during serialization only. |
| `skip_on_read` (alias `write_only`) | Exclude | Ignores the field during deserialization only. |
| `sensitive` | Redaction | Serializes the field as `"***"` (or null) to protect sensitive data; deserializes normally. |
| `required` | Requirement | Forces optional types (`std::optional<T>`) to be mandatory on input. |
| `flatten` | Layout | Hoists the nested struct's fields directly into the surrounding parent object. |
| `skip_if_null` | Layout | Omits optional fields entirely from output when they are null rather than emitting `null`. |
| `skip_if_default` | Layout | Omits fields from output if their value equals `T{}`. |
| `rename<"wire_name">` | Naming | Overrides the wire key name (alternative to `rflcpp::field`). |
| `aliases<"name1", "name2", ...>` | Naming | Accepts alternative input key names during deserialization. Canonical name is still used for writing. |
| `default_to<value>` | Defaulting | Substitutes a default constant value if the key is missing from input. |
| `title<"Text">` | Schema | Human-readable title embedded in the generated JSON Schema. |
| `description<"Text">` | Schema | Human-readable description embedded in the generated JSON Schema. |

## Detailed Example

The following example covers how to use attributes to rename keys, set alias alternatives, default missing keys, redact sensitive fields, skip fields, flatten nested structures, and omit nulls:

```cpp
#include <iostream>
#include <optional>
#include <string>
#include <rflcpp/rflcpp.hpp>

namespace at = rflcpp::attrs;

struct Address {
    std::string street;
    std::string city;
};

struct User {
    // 1. Rename to "user_name" and accept "name" or "userName" on input
    rflcpp::attr<std::string,
        at::rename<"user_name">,
        at::aliases<"name", "userName">> username;

    int age;

    // 2. Censor output to "***"
    rflcpp::attr<std::string, at::sensitive> password;

    // 3. Fall back to 3 if "retries" is missing on deserialization
    rflcpp::attr<int, at::default_to<3>> retries;

    // 4. Omit entirely from output if it's std::nullopt
    rflcpp::attr<std::optional<std::string>, at::skip_if_null> nickname;

    // 5. Excluded from output, read only
    rflcpp::attr<std::string, at::write_only> computed_etag;

    // 6. Completely skipped on both read and write
    rflcpp::attr<int, at::skip> internal_id;

    // 7. Flatten nested address fields directly into the User object
    rflcpp::attr<Address, at::flatten> address;
};

int main() {
    User u{
        .username = {"ada"},
        .age = 36,
        .password = {"hunter2"},
        .retries = {5},
        .nickname = {std::nullopt},
        .computed_etag = {"abc-123"},
        .internal_id = {99},
        .address = {{"10 Downing St", "London"}},
    };

    // Serialize to JSON
    // nickname is omitted, password is redacted, internal_id is absent, Address fields are flattened
    std::cout << rflcpp::to_json(u, {.indent = 2}) << "\n";
}
```

## JSON Schema Integration

When generating JSON schemas, `rflcpp::attrs::title` and `rflcpp::attrs::description` are automatically parsed and embedded directly in the output JSON Schema. See the [JSON Schema guide](schema.md) for details.
