# Reflection façade

`<rflcpp/reflect.hpp>` is the foundational layer. Every other feature in
the library is implemented on top of it.

## Compile-time queries

```cpp
template <reflectable_class T>
consteval std::size_t            field_count_of();

template <reflectable_class T>
consteval std::array<std::string_view, N> field_names_of();
```

Both are usable in constant expressions.

```cpp
struct Point { int x, y, z; };

static_assert(rflcpp::field_count_of<Point>() == 3);
static_assert(rflcpp::field_names_of<Point>()[0] == "x");
```

## Visitation

```cpp
template <reflectable_class T, class F>
constexpr void for_each_field(T&& obj, F&& fn);
```

`fn` is called as `fn(name, ref)` where `name` is a `std::string_view`
known at compile time and `ref` is an lvalue reference to the field
(with const-ness preserved if `obj` is const). Internally we use a
C++26 `template for` (expansion statement) so each invocation of `fn`
is instantiated at the field's actual type.

```cpp
Point p{1, 2, 3};
rflcpp::for_each_field(p, [](auto name, auto& v) {
    std::cout << name << " = " << v << "\n";
    v *= 2;                  // mutate through the reference
});
```

## Tuple interop

```cpp
auto refs = rflcpp::to_tuple(p);    // std::tuple<int&, int&, int&>
Point q   = rflcpp::from_tuple<Point>(std::tuple{4, 5, 6});
```

`to_tuple` is invaluable for one-liners like
`std::get<0>(rflcpp::to_tuple(p)) = 42;`, structured bindings over arbitrary
aggregates, or generic algorithms that already speak tuples.

## What types are "reflectable"?

The `rflcpp::reflectable_class` concept currently excludes:

* primitives,
* strings / string_views,
* sequence containers,
* map-like containers,
* `std::optional`,

leaving every other class type as a candidate for reflective inspection.
You can always specialize `rflcpp::json_codec<T>` to override how a
particular type is serialized.
