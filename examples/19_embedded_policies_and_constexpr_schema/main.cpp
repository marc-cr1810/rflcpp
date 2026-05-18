// 19_embedded_policies_and_constexpr_schema/main.cpp - Embedded Policies &
// Compile-Time Schema SPDX-License-Identifier: MIT

#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <variant>

#include <rflcpp/rflcpp.hpp>

// A user struct with collocated embedded reflection policies
struct ModernUser {
  // Collocate policies exactly inside the struct definition!
  static constexpr auto rflcpp_policies = std::make_tuple(
      rflcpp::policy::camel_case{},      // Serialize member names in camelCase
      rflcpp::policy::strict{},          // Reject unrecognized fields during
                                         // deserialization
      rflcpp::policy::external_variant{} // External tagging style for variants
  );

  int user_id;           // will be serialized as "userId"
  std::string user_name; // will be serialized as "userName"
  std::optional<std::string>
      display_name; // will be serialized as "displayName"
};

// Compile-Time static verification of schemas!
// The JSON Schema of ModernUser is resolved entirely inside a static_assert at
// compile-time!
static_assert(rflcpp::to_json_schema_view<int>() ==
              "{\"$schema\":\"https://json-schema.org/draft/2020-12/"
              "schema\",\"title\":\"int\",\"$id\":\"rflcpp://"
              "int\",\"type\":\"integer\"}");

int main() {
  ModernUser user{
      .user_id = 999, .user_name = "test_user", .display_name = "Test User"};

  // Serialize to JSON, showing the camelCase naming policy applied
  // automatically
  std::string json = rflcpp::to_json(user);
  std::cout
      << "Serialized ModernUser (Auto-camelCase via Embedded Policies):\n";
  std::cout << json << "\n\n";

  // Show strict mode rejecting a JSON payload with extra attributes
  std::string invalid_json =
      R"({"userId":999,"userName":"test","extraField":"not_allowed"})";
  auto result = rflcpp::from_json<ModernUser>(invalid_json);
  if (!result) {
    std::cout << "Strict mode correctly rejected invalid payload:\n";
    std::cout << result.error().what() << "\n\n";
  }

  // Export compile-time generated JSON Schema
  constexpr std::string_view schema_view =
      rflcpp::to_json_schema_view<ModernUser>();
  std::cout << "=== Compile-Time Static JSON Schema ===\n";
  std::cout << schema_view << "\n";
}
