# Validation

`rflcpp::validated<T, Rules...>` is a thin wrapper around a value of type
`T` that guarantees, by construction, that every `Rule` in the pack has
been satisfied.

## Built-in rules (`rflcpp::rules`)

| Rule                           | Applies to             |
|--------------------------------|------------------------|
| `min_value<V>` / `max_value<V>`| numeric types          |
| `min_length<N>` / `max_length<N>` | containers / strings |
| `non_empty`                    | containers / strings   |

The rule concept is intentionally tiny - if you want to add your own,
just provide a type with:

```cpp
struct my_rule {
    static constexpr std::string_view name() noexcept { return "my_rule"; }
    template <class T>
    static std::optional<std::string> check(const T& v) {
        return /* nullopt on success, message on failure */;
    }
};
```

## Two construction styles

```cpp
using Age = rflcpp::validated<int, rflcpp::rules::min_value<0>, rflcpp::rules::max_value<130>>;

// Throwing - convenient at construction sites:
Age a{42};

// Non-throwing - convenient at deserialization boundaries:
rflcpp::result<Age> r = Age::make(999);
if (!r) std::cerr << r.error().what();
```

## Interaction with JSON

When a `validated<T, ...>` appears as a field in a serialized type,
`to_json` writes the inner `T` unchanged. `from_json` doesn't currently
re-validate on parse (that's planned: see the `error_kind::validation_failed`
support already wired through `error`).

## Composition with `field<>`

```cpp
struct User {
    rflcpp::field<"name", rflcpp::validated<std::string, rflcpp::rules::non_empty>> name;
    rflcpp::field<"age",  Age>                                                       age;
};
```
