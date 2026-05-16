# JSON serialization

## API

```cpp
namespace rflcpp {
    struct json_options { unsigned indent = 0; bool strict = false; };

    template <class T>
    std::string  to_json(const T& value, json_options opts = {});

    template <class T>
    result<T>    from_json(std::string_view text);

    template <class T>
    T            from_json_or_throw(std::string_view text);
}
```

`result<T>` is `std::expected<T, rflcpp::error>`. The throwing overload
exists for code that prefers exceptions; everything else uses
`std::expected`.

## Supported value types out of the box

| C++ type                              | JSON kind            |
|---------------------------------------|----------------------|
| `bool`                                | `true` / `false`     |
| any integer / floating-point          | number               |
| `std::string` / convertible to `string_view` | string        |
| `std::optional<T>`                    | `null` or inner `T`  |
| `std::vector<T>`, `std::array<T, N>`, ... | array            |
| `std::map<std::string, T>` (and friends with string keys) | object |
| `rflcpp::field<"name", T>`            | object entry         |
| any reflectable aggregate             | object               |

## Pretty printing

```cpp
auto pretty = rflcpp::to_json(value, { .indent = 2 });
```

## Error handling

`from_json` returns `result<T>`. On failure, inspect:

```cpp
auto r = rflcpp::from_json<MyType>(text);
if (!r) {
    std::cerr << r.error().what();       // pretty rendering
    std::cerr << r.error().path();       // dotted path of the offending value
    if (r.error().kind() == rflcpp::error_kind::missing_field) { /* ... */ }
}
```

Possible `error_kind` values: `parse_error`, `type_mismatch`,
`missing_field`, `unknown_field`, `validation_failed`, `out_of_range`,
`user_defined`.

## Extending: custom codecs

To teach `to_json` / `from_json` about a non-reflectable type, specialize
`rflcpp::json_codec<T>`:

```cpp
namespace rflcpp {
template <>
struct json_codec<Color> {
    static void write(detail::json::writer& w, const Color& c) {
        w.write_string(c.as_hex());           // -> JSON string
    }
    static result<Color> read(const detail::json::value& v, std::string_view path) {
        if (v.k != detail::json::value::kind::string_)
            return fail({error_kind::type_mismatch, "expected hex string", std::string{path}});
        return Color::from_hex(v.s);
    }
};
} // namespace rflcpp
```

See `examples/06_custom_codec/` for a complete working example.
