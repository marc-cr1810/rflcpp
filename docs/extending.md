# Extending rflcpp

## Adding a new serialization format

Each format (JSON today, more later) lives in its own public header and
exposes a pair of free functions plus an extensibility hook:

```cpp
template <class T> std::string to_FOO(const T&, foo_options = {});
template <class T> result<T>   from_FOO(std::string_view);

template <class T, class = void> struct foo_codec;   // user specializations
```

Internally each format writer dispatches in the same order:

1. `foo_codec<T>` specialization (highest priority).
2. `validated<T, ...>` (unwrap).
3. `field<"name", T>` (unwrap and use the name).
4. `std::optional<T>`, primitives, containers.
5. Reflectable aggregate - traversed via
   `for_each_field`.
6. Static-assert if nothing matched.

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
