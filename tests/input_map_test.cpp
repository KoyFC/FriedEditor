#include "doctest/doctest.h"

#include "fried/editor/input_map.h"

using namespace fried::editor;
using Json = nlohmann::json;

namespace
{
    InputNames names()
    {
        return InputNames::fromJson(Json::parse(R"({
            "keys": [{"name": "A", "code": 4}, {"name": "W", "code": 26}, {"name": "Space", "code": 44}],
            "mouseButtons": [{"name": "Left", "code": 1}],
            "gamepadButtons": [{"name": "South", "code": 0}],
            "gamepadAxes": [{"name": "LeftX", "code": 0}, {"name": "LeftY", "code": 1}]
        })"));
    }

    Json block(const Json &bindings, const std::string &type = "button")
    {
        return {{"maps", Json::array({{{"name", "Gameplay"}, {"actions", Json::array({{{"name", "Jump"}, {"type", type}, {"bindings", bindings}}})}}})}};
    }
}

TEST_CASE("the names the engine reports are read, and a key is found by its number")
{
    InputNames read = names();
    CHECK_FALSE(read.isEmpty());
    CHECK(keyNameOf(read, 26) == "W");
    CHECK(keyNameOf(read, 999).empty());
    CHECK(InputNames::fromJson(Json()).isEmpty());
}

TEST_CASE("a block that is right has no problems, and an absent one has none either")
{
    CHECK(checkInputBlock(Json::object(), names()).empty());
    CHECK(checkInputBlock(block(Json::array({{{"single", "key:Space"}}, {{"single", "pad:South"}}, {{"single", "mouse:Left"}}})), names()).empty());
    CHECK(checkInputBlock(block(Json::array({{{"vector", Json::array({"key:W", "key:A", "key:A", "key:W"})}}, {{"stick", Json::array({"LeftX", "LeftY"})}}}), "vector2"), names()).empty());
}

TEST_CASE("every mistake of a block is told with where it is")
{
    auto problems = checkInputBlock(block(Json::array({{{"single", "key:Nope"}}, {{"axis", Json::array({"key:A", "key:W"})}}, {{"single", "pad:South"}, {"axis", Json::array()}}})), names());
    REQUIRE(problems.size() == 3);
    CHECK(problems[0].find("Gameplay/Jump") != std::string::npos);
    CHECK(problems[0].find("Nope") != std::string::npos);
    CHECK(problems[1].find("does not take a axis binding") != std::string::npos);
    CHECK(problems[2].find("exactly one") != std::string::npos);

    Json twice = {{"maps", Json::array({{{"name", "A"}}, {{"name", "A"}}})}};
    CHECK(checkInputBlock(twice, names()).size() == 1);
    CHECK(checkInputBlock(block(Json::array({{{"stick", Json::array({"key:W", "LeftY"})}}}), "vector2"), names()).size() == 1);
}

TEST_CASE("bindings are made, read and changed by their parts")
{
    CHECK(bindingKindsFor("button") == std::vector<std::string>{"single"});
    CHECK(bindingKindsFor("value") == std::vector<std::string>{"single", "axis"});
    CHECK(bindingKindsFor("vector2") == std::vector<std::string>{"vector", "stick"});

    Json binding = newBinding("vector");
    CHECK(bindingKind(binding) == "vector");
    CHECK(bindingPartCount("vector") == 4);
    CHECK(bindingPartLabel("vector", 2) == "Left");
    CHECK(bindingPart(binding, 3) == "key:Right");
    setBindingPart(&binding, 3, "key:D");
    CHECK(binding["vector"][3] == "key:D");

    Json single = newBinding("single");
    setBindingPart(&single, 0, "pad:South");
    CHECK(single["single"] == "pad:South");
    CHECK(sourceLabel("pad:South") == "Pad South");
    CHECK(sourceLabel("LeftX") == "LeftX");
    CHECK(sourceText("key", "W") == "key:W");
    CHECK(checkInputBlock(block(Json::array({newBinding("single")})), names()).empty());
}

TEST_CASE("a name is made that none of the others has")
{
    CHECK(uniqueInputName({}, "Action") == "Action");
    CHECK(uniqueInputName({"Action"}, "Action") == "Action2");
    CHECK(uniqueInputName({"Action", "Action2"}, "Action") == "Action3");
}
