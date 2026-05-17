# Merge Patches and Object Diffing

`rflcpp` provides native support for partial structural updates, object diffing, and in-place patching. This is implemented via a transparent helper wrapper `rflcpp::patch_type<T>` that tracks which fields have been modified, and integrates seamlessly with all serialization codecs (acting as a standard **JSON Merge Patch RFC 7396** engine).

## Types and Functions

```cpp
#include <rflcpp/patch.hpp>

namespace rflcpp {
    // 1. A transparent wrapper representing the delta/diff of a struct T
    template <class T>
    struct patch_type;

    // Alias helper
    template <class T>
    using patch_type_t = patch_type<T>;

    // 2. Compute the structural diff between two struct instances
    template <class T>
    constexpr patch_type_t<T> diff(const T& before, const T& after);

    // 3. Apply a structural patch in-place to update a target instance
    template <class T>
    constexpr void patch(T& target, const patch_type_t<T>& patch_obj);
}
```

---

## Detailed Example

This example demonstrates how `patch_type<T>` handles:
1. Parsing a partial JSON Merge Patch payload to update a struct in-place.
2. Generating a minimal diff payload by comparing two struct instances.

```cpp
#include <iostream>
#include <optional>
#include <string>
#include <rflcpp/rflcpp.hpp>
#include <rflcpp/json.hpp>
#include <rflcpp/patch.hpp>

struct User {
    std::string name;
    int age;
    std::optional<std::string> email;
};

int main() {
    User alice{"Alice", 30, "alice@example.com"};
    std::cout << "Original: name=" << alice.name 
              << ", age=" << alice.age 
              << ", email=" << (alice.email ? *alice.email : "null") << "\n";

    // 1. Partial Update / Parse Merge Patch
    // Nickname and name are absent (no change), age is changed, email is cleared to null
    std::string patch_json = R"({"age": 31, "email": null})";
    
    auto parsed_patch = rflcpp::from_json<rflcpp::patch_type<User>>(patch_json);
    if (!parsed_patch) {
        std::cerr << "Failed to parse patch: " << parsed_patch.error().message() << "\n";
        return 1;
    }

    // Apply the patch in-place
    rflcpp::patch(alice, *parsed_patch);
    std::cout << "Patched:  name=" << alice.name 
              << ", age=" << alice.age 
              << ", email=" << (alice.email ? *alice.email : "null") << "\n";

    // 2. Object Diffing
    User bob{"Bob", 25, "bob@example.com"};
    User bob_new{"Bob", 26, std::nullopt}; // age changed, email cleared
    
    auto bob_diff = rflcpp::diff(bob, bob_new);
    
    // Serialize the patch to JSON
    // Codecs natively serialize ONLY fields that are present in the diff
    std::string diff_json = rflcpp::to_json(bob_diff);
    std::cout << "Bob Diff JSON: " << diff_json << "\n";
    // Outputs: {"age":26,"email":null}
}
```

---

## Direct Codec & Protocol Support

Because `rflcpp::patch_type<T>` specializes the format codecs, it integrates natively across all formats:

- **JSON**: Natively supports [RFC 7396 Merge Patch](https://datatracker.ietf.org/doc/html/rfc7396) (omitted fields are absent, null overrides optional values, and present values overwrite the target).
- **YAML / TOML / MessagePack**: Fully behaves identically (only serializes fields present in the diff).
