# Polymorphic Type Registries

`rflcpp` supports the serialization and deserialization of heterogeneous polymorphic object hierarchies. By using compile-time registries and dynamic wrapper containers, the library can dynamically determine which concrete class to instantiate at runtime based on a field tag (e.g. `"type": "Dog"`).

## Types and Classes

```cpp
#include <rflcpp/registry.hpp>

namespace rflcpp {
    // 1. Compile-time registry list of dynamic types
    template <class... Types>
    struct type_registry;

    // 2. Type-erased wrapper matching a registry list and tagged key
    template <class Registry, fixed_string_registry TagField = "type">
    struct registered_any {
        rflcpp::any value;

        // Operators & transparent unwrap
        constexpr rflcpp::any& operator*() noexcept;
        constexpr rflcpp::any* operator->() noexcept;
    };
}
```

---

## Detailed Example

This example demonstrates how to serialize and deserialize a heterogeneous collection containing dynamic `Dog` and `Cat` structs using `rflcpp::registered_any`:

```cpp
#include <iostream>
#include <vector>
#include <rflcpp/rflcpp.hpp>
#include <rflcpp/json.hpp>
#include <rflcpp/registry.hpp>

struct Dog {
    std::string name;
    int bark_volume;
};

struct Cat {
    std::string name;
    bool lazy;
};

// 1. Register Dog and Cat under a type_registry list
using PetRegistry = rflcpp::type_registry<Dog, Cat>;

// 2. Declare our dynamic Pet wrapper using the tag field name "type"
using DynamicPet = rflcpp::registered_any<PetRegistry, "type">;

int main() {
    // === 1. SERIALIZATION ===
    std::vector<DynamicPet> pets;
    pets.push_back(Dog{"Fido", 8});
    pets.push_back(Cat{"Garfield", true});
    
    std::string json_out = rflcpp::to_json(pets);
    std::cout << "Serialized Pets: " << json_out << "\n";
    // Outputs:
    // [{"type":"Dog","name":"Fido","bark_volume":8},{"type":"Cat","name":"Garfield","lazy":true}]

    // === 2. DESERIALIZATION ===
    std::string json_input = R"([
        {"type": "Dog", "name": "Rover", "bark_volume": 10},
        {"type": "Cat", "name": "Felix", "lazy": false}
    ])";
    
    auto parsed_pets = rflcpp::from_json<std::vector<DynamicPet>>(json_input);
    if (!parsed_pets) {
        std::cerr << "Failed to parse: " << parsed_pets.error().message() << "\n";
        return 1;
    }
    
    std::cout << "Parsed " << parsed_pets->size() << " Pets:\n";
    for (const auto& pet : *parsed_pets) {
        // cast to inspect underlying type
        if (auto* dog = pet.value.cast<Dog>()) {
            std::cout << "  * Dog: name=" << dog->name << ", bark_volume=" << dog->bark_volume << "\n";
        } else if (auto* cat = pet.value.cast<Cat>()) {
            std::cout << "  * Cat: name=" << cat->name << ", lazy=" << (cat->lazy ? "yes" : "no") << "\n";
        }
    }
}
```

---

## Multiple Formats Support

`rflcpp::registered_any` integrates dynamically across all text-based and binary serialization backends when they are enabled:

- **JSON**: Tagged and deserialized natively using `deserialize_json`.
- **YAML**: Deserialized via `deserialize_yaml`.
- **TOML**: Deserialized via `deserialize_toml`.
- **XML**: Deserialized via `deserialize_xml` (reads type tag attributes or children).
- **MessagePack**: Deserialized dynamically via `deserialize_msgpack`.
