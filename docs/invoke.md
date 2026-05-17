# Dynamic Method Invocation

rflcpp provides an incredibly powerful, standard-compliant dynamic method invocation engine utilizing C++26 static reflection. This allows you to call member functions of any class by string name with dynamically coerced arguments at runtime.

This is the perfect foundation for building:
* Zero-boilerplate **RPC Dispatchers** (converting JSON payloads to direct C++ calls).
* Interactive **script console bindings**.
* **Dynamic routing** for games or API servers.

---

## The `rflcpp::invoke` API

The main entry point is `rflcpp::invoke`, defined in `<rflcpp/invoke.hpp>` (automatically included by `<rflcpp/rflcpp.hpp>`):

```cpp
template <class T>
rflcpp::result<rflcpp::any> invoke(
    T& obj,
    std::string_view method_name,
    const std::vector<rflcpp::any>& args
);
```

### Parameter Coercion Engine
When invoking a method, `rflcpp::invoke` inspects the parameter signature at compile-time and automatically coerces each type-erased argument at the function boundary:
1. **Fast Path**: If the types match exactly, a direct, zero-overhead cast is performed.
2. **JSON Coercion Fallback**: If the types do not match directly but are compatible (e.g. numeric narrowing/widening, enum-to-string, or structured classes), a JSON-based roundtrip serialization converts the value dynamically.

---

## Basic Example

Here is a simple example showing how to invoke a member function:

```cpp
#include <iostream>
#include <rflcpp/rflcpp.hpp>

class Calculator {
public:
    double add(double a, double b) {
        return a + b;
    }
};

int main() {
    Calculator calc;
    
    // Call "add" dynamically with two coerced integers
    auto result = rflcpp::invoke(calc, "add", std::vector<rflcpp::any>{ 42, 8 });
    
    if (result.has_value()) {
        std::cout << "Result: " << *result->cast<double>() << "\n"; // Outputs: 50.0
    } else {
        std::cout << "Error: " << result.error().message() << "\n";
    }
}
```

---

## Void Return Types

If the invoked member function returns `void`, `rflcpp::invoke` successfully calls the method and returns an empty `rflcpp::any` container (which can be checked via `result->empty()`).

---

## Error Handling

`rflcpp::invoke` returns a `rflcpp::result` containing structured error information:

| Error Case | Error Kind | Description |
|---|---|---|
| Method Not Found | `error_kind::unknown_field` | The class does not have a member function with the requested name. |
| Argument Count Mismatch | `error_kind::type_mismatch` | The number of provided arguments does not match the function's parameter count. |
| Uncoercible Argument Type | `error_kind::type_mismatch` | An argument could not be successfully coerced to the target parameter type. |
| Class has no methods | `error_kind::unknown_field` | The targeted class has no invocable public member functions. |
