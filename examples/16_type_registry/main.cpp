#include <iostream>
#include <vector>
#include <rflcpp/reflect.hpp>
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

// Register Dog and Cat under a dynamic tag registry
using PetRegistry = rflcpp::type_registry<Dog, Cat>;

// Dynamic Pet polymorphic type wrapper
using DynamicPet = rflcpp::registered_any<PetRegistry, "type">;

int main() {
    std::cout << "=== rflcpp C++26 Reflection-Native Type Registry Example ===" << std::endl;
    
    // 1. Serialize a heterogeneous array of pets
    std::vector<DynamicPet> pets;
    pets.push_back(Dog{"Fido", 8});
    pets.push_back(Cat{"Garfield", true});
    
    std::string json_out = rflcpp::to_json(pets);
    std::cout << "Serialized Pets: " << json_out << std::endl;
    
    // 2. Deserialize tagged pets polymorphically
    std::string json_input = R"([
        {"type": "Dog", "name": "Rover", "bark_volume": 10},
        {"type": "Cat", "name": "Felix", "lazy": false}
    ])";
    
    auto parsed_pets = rflcpp::from_json<std::vector<DynamicPet>>(json_input);
    if (!parsed_pets) {
        std::cerr << "Failed to parse pets: " << parsed_pets.error().message() << std::endl;
        return 1;
    }
    
    std::cout << "Parsed Pets successfully:" << std::endl;
    for (const auto& pet : *parsed_pets) {
        std::cout << "  Type: " << pet.value.type_name();
        if (auto* dog = pet.value.cast<Dog>()) {
            std::cout << " (Dog name=" << dog->name << ", bark_volume=" << dog->bark_volume << ")" << std::endl;
        } else if (auto* cat = pet.value.cast<Cat>()) {
            std::cout << " (Cat name=" << cat->name << ", lazy=" << (cat->lazy ? "yes" : "no") << ")" << std::endl;
        }
    }
    
    return 0;
}
