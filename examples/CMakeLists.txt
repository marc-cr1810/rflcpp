# Each subdirectory under examples/ is its own self-contained executable.
# `make examples` (top-level) builds them all via the aggregate target.

add_custom_target(rflcpp_examples)

set(RFLCPP_EXAMPLES
    01_basic_reflection
    02_json_roundtrip
    03_nested_types
    04_validation
    05_named_fields
    06_custom_codec
    07_enums
    08_attributes
    09_variants
    10_derive_and_schema
    11_schema_metadata
    12_binary_formats
    13_cli_parser
    14_reflected_any
    15_patch_and_diff
    16_type_registry
    17_method_invocation
    18_direct_json_serialization
    19_embedded_policies_and_constexpr_schema
)

foreach(example IN LISTS RFLCPP_EXAMPLES)
    add_executable(${example} ${example}/main.cpp)
    target_link_libraries(${example} PRIVATE rflcpp::rflcpp)
    set_target_properties(${example} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
    add_dependencies(rflcpp_examples ${example})
endforeach()
