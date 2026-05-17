# All test cases are aggregated into a single Catch2 executable so the
# Catch2 runtime is built once and linking stays fast.

set(RFLCPP_TEST_SOURCES
    test_reflect.cpp
    test_validation.cpp
    test_json.cpp
    test_enum.cpp
    test_attributes.cpp
    test_policy.cpp
    test_variant.cpp
    test_derive.cpp
    test_schema.cpp
    test_any.cpp
    test_cli.cpp
    test_patch.cpp
    test_registry.cpp
    test_invoke.cpp
)

if(RFLCPP_ENABLE_XML)
    list(APPEND RFLCPP_TEST_SOURCES test_xml.cpp)
endif()
if(RFLCPP_ENABLE_YAML)
    list(APPEND RFLCPP_TEST_SOURCES test_yaml.cpp)
endif()
if(RFLCPP_ENABLE_TOML)
    list(APPEND RFLCPP_TEST_SOURCES test_toml.cpp)
endif()
if(RFLCPP_ENABLE_MSGPACK)
    list(APPEND RFLCPP_TEST_SOURCES test_msgpack.cpp)
endif()

add_executable(rflcpp_tests ${RFLCPP_TEST_SOURCES})

target_link_libraries(rflcpp_tests
    PRIVATE
        rflcpp::rflcpp
        Catch2::Catch2WithMain
)

set_target_properties(rflcpp_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/submodules/Catch2/extras")
include(Catch)

catch_discover_tests(rflcpp_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)
