#include <catch2/catch_test_macros.hpp>
#include <rflcpp/rflcpp.hpp>
#include <vector>
#include <string>
#include <cstdio>

namespace {
struct FileTestData {
    int id;
    std::string name;
    std::vector<double> scores;

    bool operator==(const FileTestData& other) const {
        return id == other.id && name == other.name && scores == other.scores;
    }
};
} // namespace

#ifdef RFLCPP_ENABLE_JSON
TEST_CASE("JSON load and save file API", "[file_io][json]") {
    FileTestData original{42, "JSON File Test", {9.5, 8.7, 10.0}};
    const std::string path = "test_data.json";

    auto save_res = rflcpp::json::save(path, original);
    REQUIRE(save_res.has_value());

    auto load_res = rflcpp::json::load<FileTestData>(path);
    REQUIRE(load_res.has_value());
    REQUIRE(*load_res == original);

    // Clean up
    std::remove(path.c_str());
}
#endif

#ifdef RFLCPP_ENABLE_XML
TEST_CASE("XML load and save file API", "[file_io][xml]") {
    FileTestData original{100, "XML File Test", {1.2, 3.4}};
    const std::string path = "test_data.xml";

    auto save_res = rflcpp::xml::save(path, original);
    REQUIRE(save_res.has_value());

    auto load_res = rflcpp::xml::load<FileTestData>(path);
    REQUIRE(load_res.has_value());
    REQUIRE(*load_res == original);

    // Clean up
    std::remove(path.c_str());
}
#endif

#ifdef RFLCPP_ENABLE_YAML
TEST_CASE("YAML load and save file API", "[file_io][yaml]") {
    FileTestData original{999, "YAML File Test", {100.0, 99.9}};
    const std::string path = "test_data.yaml";

    auto save_res = rflcpp::yaml::save(path, original);
    REQUIRE(save_res.has_value());

    auto load_res = rflcpp::yaml::load<FileTestData>(path);
    REQUIRE(load_res.has_value());
    REQUIRE(*load_res == original);

    // Clean up
    std::remove(path.c_str());
}
#endif

#ifdef RFLCPP_ENABLE_TOML
TEST_CASE("TOML load and save file API", "[file_io][toml]") {
    FileTestData original{777, "TOML File Test", {5.5, 4.4}};
    const std::string path = "test_data.toml";

    auto save_res = rflcpp::toml::save(path, original);
    REQUIRE(save_res.has_value());

    auto load_res = rflcpp::toml::load<FileTestData>(path);
    REQUIRE(load_res.has_value());
    REQUIRE(*load_res == original);

    // Clean up
    std::remove(path.c_str());
}
#endif

#ifdef RFLCPP_ENABLE_MSGPACK
TEST_CASE("MsgPack load and save file API", "[file_io][msgpack]") {
    FileTestData original{888, "MsgPack File Test", {1.1, 2.2, 3.3}};
    const std::string path = "test_data.bin";

    auto save_res = rflcpp::msgpack::save(path, original);
    REQUIRE(save_res.has_value());

    auto load_res = rflcpp::msgpack::load<FileTestData>(path);
    REQUIRE(load_res.has_value());
    REQUIRE(*load_res == original);

    // Clean up
    std::remove(path.c_str());
}
#endif
