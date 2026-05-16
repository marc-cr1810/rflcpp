#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include <rflcpp/rflcpp.hpp>

namespace {

namespace at = rflcpp::attrs;

struct with_skip {
    std::string                          visible = "v";
    rflcpp::attr<int, at::skip>          hidden{42};
};

struct with_skip_on_write {
    std::string                                    name      = "n";
    rflcpp::attr<std::string, at::skip_on_write>   computed{"derived"};
};

struct with_skip_on_read {
    std::string                                    name = "n";
    rflcpp::attr<int, at::skip_on_read>            ignored_on_read{0};
};

struct with_rename {
    rflcpp::attr<std::string, at::rename<"user_name">> user;
};

struct with_aliases {
    rflcpp::attr<std::string, at::aliases<"mail", "email_addr">> email;
};

struct with_default {
    rflcpp::attr<int, at::default_to<7>> retries;
};

struct with_required {
    rflcpp::attr<std::optional<std::string>, at::required> must_have;
};

struct with_sensitive {
    std::string                                    user;
    rflcpp::attr<std::string, at::sensitive>       password;
};

struct nested_inner {
    int x;
    int y;
};

struct with_flatten {
    std::string                                    name;
    rflcpp::attr<nested_inner, at::flatten>        body;
};

struct with_skip_if_null {
    std::string                                                            name;
    rflcpp::attr<std::optional<std::string>, at::skip_if_null>             alias;
};

struct with_skip_if_default {
    rflcpp::attr<int, at::skip_if_default> counter;
    std::string                            name;
};

} // namespace

TEST_CASE("attrs::skip excludes a field from both directions", "[attr][skip]") {
    with_skip s;
    auto js = rflcpp::to_json(s);
    REQUIRE(js.find("hidden") == std::string::npos);
    REQUIRE(js.find("visible") != std::string::npos);

    auto back = rflcpp::from_json<with_skip>(R"({"visible":"x","hidden":99})");
    REQUIRE(back.has_value());
    REQUIRE(back->visible == "x");
    REQUIRE(back->hidden.value == 42);
}

TEST_CASE("attrs::skip_on_write excludes only from output", "[attr][skip_on_write]") {
    with_skip_on_write s;
    auto js = rflcpp::to_json(s);
    REQUIRE(js.find("computed") == std::string::npos);

    auto back = rflcpp::from_json<with_skip_on_write>(R"({"name":"n","computed":"X"})");
    REQUIRE(back.has_value());
    REQUIRE(back->computed.value == "X");
}

TEST_CASE("attrs::skip_on_read excludes only from input", "[attr][skip_on_read]") {
    with_skip_on_read s; s.ignored_on_read.value = 5;
    auto js = rflcpp::to_json(s);
    REQUIRE(js.find("ignored_on_read") != std::string::npos);

    auto back = rflcpp::from_json<with_skip_on_read>(
        R"({"name":"n","ignored_on_read":999})");
    REQUIRE(back.has_value());
    REQUIRE(back->ignored_on_read.value == 0);
}

TEST_CASE("attrs::rename overrides the wire name", "[attr][rename]") {
    with_rename w{{"ada"}};
    auto js = rflcpp::to_json(w);
    REQUIRE(js.find("user_name") != std::string::npos);
    REQUIRE(js.find("\"user\":") == std::string::npos);

    auto back = rflcpp::from_json<with_rename>(R"({"user_name":"grace"})");
    REQUIRE(back.has_value());
    REQUIRE(back->user.value == "grace");
}

TEST_CASE("attrs::aliases accept alternate input keys", "[attr][aliases]") {
    auto back = rflcpp::from_json<with_aliases>(R"({"mail":"ada@example.com"})");
    REQUIRE(back.has_value());
    REQUIRE(back->email.value == "ada@example.com");

    auto back2 = rflcpp::from_json<with_aliases>(R"({"email_addr":"x@y"})");
    REQUIRE(back2.has_value());
    REQUIRE(back2->email.value == "x@y");
}

TEST_CASE("attrs::default_to supplies a value when key is missing", "[attr][default_to]") {
    auto back = rflcpp::from_json<with_default>(R"({})");
    REQUIRE(back.has_value());
    REQUIRE(back->retries.value == 7);
}

TEST_CASE("attrs::required rejects missing optional", "[attr][required]") {
    auto back = rflcpp::from_json<with_required>(R"({})");
    REQUIRE(!back.has_value());
    REQUIRE(back.error().kind() == rflcpp::error_kind::missing_field);
}

TEST_CASE("attrs::sensitive redacts the output", "[attr][sensitive]") {
    with_sensitive s{"ada", {"hunter2"}};
    auto js = rflcpp::to_json(s);
    REQUIRE(js.find("hunter2") == std::string::npos);
    REQUIRE(js.find("***")     != std::string::npos);

    auto back = rflcpp::from_json<with_sensitive>(
        R"({"user":"ada","password":"hunter2"})");
    REQUIRE(back.has_value());
    REQUIRE(back->password.value == "hunter2");
}

TEST_CASE("attrs::flatten hoists nested fields", "[attr][flatten]") {
    with_flatten w{"x", {{1, 2}}};
    auto js = rflcpp::to_json(w);
    REQUIRE(js.find("\"x\":1") != std::string::npos);
    REQUIRE(js.find("\"y\":2") != std::string::npos);
    REQUIRE(js.find("body")    == std::string::npos);

    auto back = rflcpp::from_json<with_flatten>(
        R"({"name":"x","x":10,"y":20})");
    REQUIRE(back.has_value());
    REQUIRE(back->body.value.x == 10);
    REQUIRE(back->body.value.y == 20);
}

TEST_CASE("attrs::skip_if_null omits empty optionals", "[attr][skip_if_null]") {
    with_skip_if_null w{"x", {std::nullopt}};
    auto js = rflcpp::to_json(w);
    REQUIRE(js.find("alias") == std::string::npos);

    w.alias.value = "shorty";
    js = rflcpp::to_json(w);
    REQUIRE(js.find("\"alias\":\"shorty\"") != std::string::npos);
}

TEST_CASE("attrs::skip_if_default omits default-valued fields", "[attr][skip_if_default]") {
    with_skip_if_default w{{0}, "x"};
    auto js = rflcpp::to_json(w);
    REQUIRE(js.find("counter") == std::string::npos);

    w.counter.value = 99;
    js = rflcpp::to_json(w);
    REQUIRE(js.find("\"counter\":99") != std::string::npos);
}
