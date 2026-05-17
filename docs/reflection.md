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

## Helper queries and metadata

`rflcpp` provides several advanced compile-time queries:

### `base_count_of<T>()`

Returns the compile-time count of direct base classes of `T`:

```cpp
struct Base { int a; };
struct Derived : Base { int b; };

static_assert(rflcpp::base_count_of<Derived>() == 1);
```

### `type_name_of<T>()`

Returns a compile-time `std::string_view` of the spelled type name. It abstracts away compiler-specific decorations:

```cpp
static_assert(rflcpp::type_name_of<int>() == "int");
static_assert(rflcpp::type_name_of<Point>() == "Point");
```

### `template_for_each_field<T>(fn)`

Visits field metadata at compile time **without** needing an object instance. The functor/lambda is called as `fn.template operator()<FieldType>(field_name)`:

```cpp
rflcpp::template_for_each_field<Point>([]<class FT>(std::string_view name) {
    std::cout << "Field: " << name << " Type: " << rflcpp::type_name_of<FT>() << "\n";
});
```

This is heavily used internally by the library for schema generation and CLI parsers.

## What types are "reflectable"?

The `rflcpp::reflectable_class` concept currently excludes:

* primitives,
* strings / string_views,
* sequence containers,
* map-like containers,
* `std::optional`,

leaving every other class type as a candidate for reflective inspection.
You can always specialize format codecs (e.g. `rflcpp::json_codec<T>`) to override how a
particular type is handled.
