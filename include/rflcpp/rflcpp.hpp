// rflcpp - Modern C++26 reflection-based serialization.
// SPDX-License-Identifier: MIT
#pragma once

#define RFLCPP_VERSION_MAJOR 0
#define RFLCPP_VERSION_MINOR 1
#define RFLCPP_VERSION_PATCH 0
#define RFLCPP_VERSION_STRING "0.1.0"

#include <rflcpp/attributes.hpp>
#include <rflcpp/concepts.hpp>
#include <rflcpp/derive.hpp>
#include <rflcpp/enum_meta.hpp>
#include <rflcpp/error.hpp>
#include <rflcpp/policy.hpp>
#include <rflcpp/reflect.hpp>
#include <rflcpp/result.hpp>
#include <rflcpp/validation.hpp>

#ifdef RFLCPP_ENABLE_JSON
#include <rflcpp/json.hpp>
#include <rflcpp/schema.hpp>
#endif

#ifdef RFLCPP_ENABLE_XML
#include <rflcpp/xml.hpp>
#endif

#ifdef RFLCPP_ENABLE_YAML
#include <rflcpp/yaml.hpp>
#endif

#ifdef RFLCPP_ENABLE_TOML
#include <rflcpp/toml.hpp>
#endif

#ifdef RFLCPP_ENABLE_MSGPACK
#include <rflcpp/msgpack.hpp>
#endif

#include <rflcpp/invoke.hpp>

namespace rflcpp {}
