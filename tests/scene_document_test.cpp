#include "doctest/doctest.h"

#include "fried/editor/scene_document.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace fried::editor;
using Json = nlohmann::json;

namespace
{
    Json object(const std::string &id, const std::string &name)
    {
        return {{"id", id},
                {"name", name},
                {"components", Json::array({Json{{"type", "Transform"}, {"x", 1}, {"y", 2}}})}};
    }

    SceneDocument sample()
    {
        SceneDocument document = SceneDocument::empty("Main");
        REQUIRE(document.insertObject(0, object("a", "A")));
        REQUIRE(document.insertObject(1, object("b", "B")));
        return document;
    }
}

TEST_CASE("the file text is stable, so a scene read and written is the same bytes")
{
    SceneDocument document = sample();
    std::string first = document.dump();

    SceneDocument again;
    std::string error;
    REQUIRE(again.parse(first, &error));
    CHECK(again.dump() == first);
    CHECK(first.back() == '\n');
    CHECK(first.find("\"objects\"") < first.find("\"version\""));
}

TEST_CASE("the scene file the engine's sandbox ships round-trips byte for byte" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::ifstream file(std::string(std::getenv("FRIED_ENGINE")) + "/sandbox/assets/scenes/sample.scene.json");
    std::stringstream text;
    text << file.rdbuf();

    SceneDocument document;
    std::string error;
    REQUIRE(document.parse(text.str(), &error));
    CHECK(document.dump() == text.str());
}

TEST_CASE("objects are inserted, moved and removed by id, and each edit reports itself")
{
    SceneDocument document = sample();
    std::vector<DocumentChange> changes;
    document.m_onChange = [&](const DocumentChange &change) { changes.push_back(change); };

    CHECK_FALSE(document.insertObject(0, object("a", "again")));
    CHECK(document.moveObject("b", 0));
    CHECK(document.objectIds() == std::vector<std::string>({"b", "a"}));

    Json removed;
    int index = -1;
    CHECK(document.removeObject("b", &removed, &index));
    CHECK(index == 0);
    CHECK(removed["name"] == "B");
    CHECK(document.insertObject(index, removed));
    CHECK(document.objectIds() == std::vector<std::string>({"b", "a"}));

    REQUIRE(changes.size() == 3);
    CHECK(changes[0].m_kind == DocumentChange::Kind::Structure);
    CHECK(document.isModified());
    CHECK(document.uniqueId("a") == "a-2");
    CHECK(document.uniqueId("c") == "c");
}

TEST_CASE("a component field can be set, read back and left to the engine's default again")
{
    SceneDocument document = sample();
    std::vector<DocumentChange> changes;
    document.m_onChange = [&](const DocumentChange &change) { changes.push_back(change); };

    CHECK(document.setComponentField("a", "Transform", "x", 40));
    CHECK(document.componentField("a", "Transform", "x") == Json(40));
    CHECK(document.setComponentField("a", "Transform", "x", std::nullopt));
    CHECK_FALSE(document.componentField("a", "Transform", "x").has_value());

    CHECK_FALSE(document.setComponentField("a", "Sprite", "flip", Json("None")));
    CHECK_FALSE(document.setComponentField("nope", "Transform", "x", 1));

    REQUIRE(changes.size() == 2);
    CHECK(changes[0].m_kind == DocumentChange::Kind::Field);
    CHECK(changes[0].m_field == "x");
    CHECK(changes[0].m_value == 40);

    CHECK(document.setObjectField("a", "priority", 3));
    CHECK(document.objectField("a", "priority") == Json(3));
    CHECK_FALSE(document.setObjectField("a", "id", Json("z")));
}

TEST_CASE("components are added and removed, except the transform")
{
    SceneDocument document = sample();
    CHECK(document.insertComponent("a", 1, Json{{"type", "Sprite"}, {"flip", "None"}}));
    CHECK_FALSE(document.insertComponent("a", 2, Json{{"type", "Sprite"}}));

    Json removed;
    int index = -1;
    CHECK_FALSE(document.removeComponent("a", "Transform", &removed, &index));
    CHECK(document.removeComponent("a", "Sprite", &removed, &index));
    CHECK(index == 1);
    CHECK(removed["flip"] == "None");
}

TEST_CASE("clearing the last override leaves the file as it was before there were any")
{
    SceneDocument document = sample();
    std::string before = document.dump();

    CHECK(document.setOverrideField("nds", "a", "Transform", "x", 5));
    CHECK(document.overrideField("nds", "a", "Transform", "x") == Json(5));
    CHECK(document.dump() != before);
    CHECK_FALSE(document.setOverrideField("nds", "a", "Sprite", "flip", Json("None")));

    CHECK(document.setOverrideField("nds", "a", "Transform", "x", std::nullopt));
    CHECK(document.dump() == before);
}

TEST_CASE("a file that is not a scene is refused with a reason")
{
    SceneDocument document;
    std::string error;
    CHECK_FALSE(document.parse("[]", &error));
    CHECK_FALSE(document.parse(R"({"version": 2, "objects": []})", &error));
    CHECK(error.find("version 2") != std::string::npos);
    CHECK_FALSE(document.parse(R"({"version": 1, "objects": [{"name": "no id"}]})", &error));
    CHECK_FALSE(document.load("/nonexistent/scene.json", &error));
}
