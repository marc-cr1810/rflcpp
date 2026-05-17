cmake_minimum_required(VERSION 3.28)

project(rflcpp
    VERSION      0.1.0
    DESCRIPTION  "A modern C++26 reflection-based serialization library"
    HOMEPAGE_URL "https://github.com/your-org/rflcpp"
    LANGUAGES    CXX C
)

# ----------------------------------------------------------------------------
# Project-wide settings
# ----------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD          26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ----------------------------------------------------------------------------
# Options
# ----------------------------------------------------------------------------

option(RFLCPP_BUILD_EXAMPLES "Build rflcpp examples"          ON)
option(RFLCPP_BUILD_TESTS    "Build rflcpp tests"             ON)
option(RFLCPP_BUILD_DOCS     "Build rflcpp documentation"     OFF)
option(RFLCPP_HEADER_ONLY    "Build rflcpp as a header-only target" OFF)
option(RFLCPP_INSTALL        "Generate install rules for rflcpp"     ON)

option(RFLCPP_ENABLE_JSON "Enable JSON support using nlohmann/json" ON)
option(RFLCPP_ENABLE_XML  "Enable XML support using pugixml"       OFF)
option(RFLCPP_ENABLE_YAML "Enable YAML support using yaml-cpp"    OFF)
option(RFLCPP_ENABLE_TOML    "Enable TOML support using tomlplusplus" OFF)
option(RFLCPP_ENABLE_MSGPACK "Enable MessagePack support using mpack" ON)

# ----------------------------------------------------------------------------
# Reflection feature detection (mandatory)
# ----------------------------------------------------------------------------
#
# rflcpp targets C++26 static reflection (P2996) unconditionally. We probe a
# set of candidate compiler flags for a minimal reflection program; if NONE
# of them produces a successful compile, configuration fails hard with an
# actionable message. There is intentionally no fallback path -- the whole
# point of the library is to demonstrate reflection-native serialization.

include(CheckCXXSourceCompiles)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(FetchContent)

# ----------------------------------------------------------------------------
# Dependencies
# ----------------------------------------------------------------------------

if(RFLCPP_ENABLE_JSON)
    find_package(nlohmann_json QUIET)
    if(NOT nlohmann_json_FOUND)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
        )
        FetchContent_MakeAvailable(nlohmann_json)
    endif()
endif()

if(RFLCPP_ENABLE_XML)
    find_package(pugixml QUIET)
    if(NOT pugixml_FOUND)
        FetchContent_Declare(
            pugixml
            GIT_REPOSITORY https://github.com/zeux/pugixml.git
            GIT_TAG v1.14
        )
        FetchContent_MakeAvailable(pugixml)
    endif()
endif()

if(RFLCPP_ENABLE_YAML)
    find_package(yaml-cpp QUIET)
    if(NOT yaml-cpp_FOUND)
        FetchContent_Declare(
            yaml-cpp
            GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
            GIT_TAG 0.8.0
        )
        FetchContent_MakeAvailable(yaml-cpp)
    endif()
endif()

if(RFLCPP_ENABLE_TOML)
    find_package(tomlplusplus QUIET)
    if(NOT tomlplusplus_FOUND)
        FetchContent_Declare(
            tomlplusplus
            GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
            GIT_TAG v3.4.0
        )
        FetchContent_MakeAvailable(tomlplusplus)
    endif()
endif()

if(RFLCPP_ENABLE_MSGPACK)
    FetchContent_Declare(
        mpack
        GIT_REPOSITORY https://github.com/ludocode/mpack.git
        GIT_TAG v1.1
    )
    FetchContent_GetProperties(mpack)
    if(NOT mpack_POPULATED)
        FetchContent_Populate(mpack)
        add_library(mpack STATIC
            ${mpack_SOURCE_DIR}/src/mpack/mpack-common.c
            ${mpack_SOURCE_DIR}/src/mpack/mpack-expect.c
            ${mpack_SOURCE_DIR}/src/mpack/mpack-node.c
            ${mpack_SOURCE_DIR}/src/mpack/mpack-platform.c
            ${mpack_SOURCE_DIR}/src/mpack/mpack-reader.c
            ${mpack_SOURCE_DIR}/src/mpack/mpack-writer.c
        )
        target_include_directories(mpack PUBLIC ${mpack_SOURCE_DIR}/src/mpack)
    endif()
endif()

set(_rflcpp_reflection_test_code [=[
#if __has_include(<meta>)
#  include <meta>
#elif __has_include(<experimental/meta>)
#  include <experimental/meta>
#else
#  error "no reflection header"
#endif
struct S { int x; int y; };
constexpr auto r = ^^S;
consteval std::size_t probe() {
    return std::meta::nonstatic_data_members_of(
        ^^S, std::meta::access_context::unchecked()).size();
}
static_assert(probe() == 2);
int main() { return 0; }
]=])

# Different toolchains spell the P2996 enablement flag differently. Probe
# in priority order; the first that compiles the test program wins.
#   * GCC >= 16:                                 -freflection
#   * Bloomberg's clang-p2996 fork:               -freflection-latest
#   * (future) mainline clang once P2996 lands:   -freflection-ts
set(_rflcpp_refl_flag_candidates
    "-freflection"
    "-freflection-latest"
    "-freflection-ts"
)

set(RFLCPP_REFLECTION_FLAG "")
foreach(_flag IN LISTS _rflcpp_refl_flag_candidates)
    string(MAKE_C_IDENTIFIER "RFLCPP_REFLECTION_OK_${_flag}" _var)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS} -std=c++26 ${_flag}")
    check_cxx_source_compiles("${_rflcpp_reflection_test_code}" ${_var})
    unset(CMAKE_REQUIRED_FLAGS)
    if(${_var})
        set(RFLCPP_REFLECTION_FLAG "${_flag}")
        break()
    endif()
endforeach()

if(NOT RFLCPP_REFLECTION_FLAG)
    message(FATAL_ERROR
        "\n"
        "=============================================================\n"
        "rflcpp requires C++26 static reflection (P2996) and could not\n"
        "find a working compiler+flag combination.\n"
        "\n"
        "Detected compiler:  ${CMAKE_CXX_COMPILER_ID} "
            "${CMAKE_CXX_COMPILER_VERSION}\n"
        "Path:               ${CMAKE_CXX_COMPILER}\n"
        "Flags tried:        ${_rflcpp_refl_flag_candidates}\n"
        "\n"
        "Known working setups:\n"
        "  * GCC >= 16, invoked with -freflection\n"
        "  * Bloomberg's clang-p2996 fork, with -freflection-latest\n"
        "\n"
        "Point CMake at a suitable compiler, e.g.:\n"
        "  cmake -S . -B build -DCMAKE_CXX_COMPILER=/path/to/g++-16\n"
        "or:\n"
        "  make CXX=/path/to/g++-16\n"
        "=============================================================")
endif()

message(STATUS "rflcpp: using reflection flag ${RFLCPP_REFLECTION_FLAG}")

# ----------------------------------------------------------------------------
# Core library target
# ----------------------------------------------------------------------------

set(RFLCPP_PUBLIC_HEADERS
    include/rflcpp/rflcpp.hpp
    include/rflcpp/attributes.hpp
    include/rflcpp/concepts.hpp
    include/rflcpp/derive.hpp
    include/rflcpp/enum_meta.hpp
    include/rflcpp/error.hpp
    include/rflcpp/policy.hpp
    include/rflcpp/reflect.hpp
    include/rflcpp/result.hpp
    include/rflcpp/schema.hpp
    include/rflcpp/validation.hpp
    include/rflcpp/patch.hpp
    include/rflcpp/registry.hpp
)

if(RFLCPP_ENABLE_JSON)
    list(APPEND RFLCPP_PUBLIC_HEADERS include/rflcpp/json.hpp)
endif()
if(RFLCPP_ENABLE_XML)
    list(APPEND RFLCPP_PUBLIC_HEADERS include/rflcpp/xml.hpp)
endif()
if(RFLCPP_ENABLE_YAML)
    list(APPEND RFLCPP_PUBLIC_HEADERS include/rflcpp/yaml.hpp)
endif()
if(RFLCPP_ENABLE_TOML)
    list(APPEND RFLCPP_PUBLIC_HEADERS include/rflcpp/toml.hpp)
endif()
if(RFLCPP_ENABLE_MSGPACK)
    list(APPEND RFLCPP_PUBLIC_HEADERS include/rflcpp/msgpack.hpp)
endif()
list(APPEND RFLCPP_PUBLIC_HEADERS include/rflcpp/cbor.hpp)

# rflcpp is largely header-only; the few cpp files here exist so the library
# has a real translation unit to attach version metadata and the (small)
# non-template error formatting to.
set(RFLCPP_SOURCES
    src/error.cpp
    src/version.cpp
)

set(RFLCPP_INTERNAL_HEADERS
    src/internal_common.hpp
)

if(RFLCPP_HEADER_ONLY)
    add_library(rflcpp INTERFACE)
    set(RFLCPP_SCOPE INTERFACE)
    target_sources(rflcpp INTERFACE
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
        FILES ${RFLCPP_PUBLIC_HEADERS}
    )
    target_compile_definitions(rflcpp INTERFACE RFLCPP_HEADER_ONLY=1)
else()
    add_library(rflcpp ${RFLCPP_SOURCES} ${RFLCPP_INTERNAL_HEADERS})
    set(RFLCPP_SCOPE PUBLIC)
    target_sources(rflcpp PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
        FILES ${RFLCPP_PUBLIC_HEADERS}
    )
    target_include_directories(rflcpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(rflcpp PROPERTIES
        VERSION   ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif()

add_library(rflcpp::rflcpp ALIAS rflcpp)

target_include_directories(rflcpp ${RFLCPP_SCOPE}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(RFLCPP_HEADER_ONLY)
    target_compile_features(rflcpp INTERFACE cxx_std_26)
else()
    target_compile_features(rflcpp PUBLIC cxx_std_26)
endif()

# Propagate the reflection-enabling flag to every consumer of rflcpp.
target_compile_options(rflcpp ${RFLCPP_SCOPE} ${RFLCPP_REFLECTION_FLAG})

if(RFLCPP_ENABLE_JSON)
    target_compile_definitions(rflcpp ${RFLCPP_SCOPE} RFLCPP_ENABLE_JSON)
    target_link_libraries(rflcpp ${RFLCPP_SCOPE} $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>)
endif()

if(RFLCPP_ENABLE_XML)
    target_compile_definitions(rflcpp ${RFLCPP_SCOPE} RFLCPP_ENABLE_XML)
    target_link_libraries(rflcpp ${RFLCPP_SCOPE} $<BUILD_INTERFACE:pugixml>)
endif()

if(RFLCPP_ENABLE_YAML)
    target_compile_definitions(rflcpp ${RFLCPP_SCOPE} RFLCPP_ENABLE_YAML)
    target_link_libraries(rflcpp ${RFLCPP_SCOPE} $<BUILD_INTERFACE:yaml-cpp>)
endif()

if(RFLCPP_ENABLE_TOML)
    target_compile_definitions(rflcpp ${RFLCPP_SCOPE} RFLCPP_ENABLE_TOML)
    target_link_libraries(rflcpp ${RFLCPP_SCOPE} $<BUILD_INTERFACE:tomlplusplus::tomlplusplus>)
endif()

if(RFLCPP_ENABLE_MSGPACK)
    target_compile_definitions(rflcpp ${RFLCPP_SCOPE} RFLCPP_ENABLE_MSGPACK)
    target_link_libraries(rflcpp ${RFLCPP_SCOPE} $<BUILD_INTERFACE:mpack>)
endif()

# Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    if(NOT RFLCPP_HEADER_ONLY)
        target_compile_options(rflcpp PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor)
    endif()
endif()

# ----------------------------------------------------------------------------
# Examples / tests / docs
# ----------------------------------------------------------------------------

if(RFLCPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(RFLCPP_BUILD_TESTS)
    # Catch2 is vendored as a git submodule under submodules/Catch2.
    # If the submodule hasn't been fetched, give the user an actionable hint.
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/submodules/Catch2/CMakeLists.txt")
        message(FATAL_ERROR
            "Catch2 submodule not found at submodules/Catch2.\n"
            "Run: git submodule update --init --recursive\n"
            "or pass -DRFLCPP_BUILD_TESTS=OFF to skip the tests.")
    endif()

    # Keep Catch2's own targets out of `make install`.
    set(CATCH_INSTALL_DOCS    OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS  OFF CACHE BOOL "" FORCE)
    add_subdirectory(submodules/Catch2 EXCLUDE_FROM_ALL)

    enable_testing()
    add_subdirectory(tests)
endif()

if(RFLCPP_BUILD_DOCS)
    add_subdirectory(docs)
endif()

# ----------------------------------------------------------------------------
# Installation / packaging
# ----------------------------------------------------------------------------

if(RFLCPP_INSTALL)
    install(TARGETS rflcpp
        EXPORT  rflcppTargets
        FILE_SET HEADERS
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(EXPORT rflcppTargets
        FILE        rflcppTargets.cmake
        NAMESPACE   rflcpp::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rflcpp
    )

    configure_package_config_file(
        cmake/rflcppConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/rflcppConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rflcpp
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/rflcppConfigVersion.cmake"
        VERSION       ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/rflcppConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/rflcppConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rflcpp
    )
endif()

# ----------------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "rflcpp ${PROJECT_VERSION} configuration:")
message(STATUS "  C++ standard         : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type           : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler             : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Header-only          : ${RFLCPP_HEADER_ONLY}")
message(STATUS "  Reflection flag      : ${RFLCPP_REFLECTION_FLAG}")
message(STATUS "  Build examples       : ${RFLCPP_BUILD_EXAMPLES}")
message(STATUS "  Build tests          : ${RFLCPP_BUILD_TESTS}")
message(STATUS "  Build docs           : ${RFLCPP_BUILD_DOCS}")
message(STATUS "  Install prefix       : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
