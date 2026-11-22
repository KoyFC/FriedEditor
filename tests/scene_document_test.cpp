#include "doctest/doctest.h"

#include "fried/editor/scene_document.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace fried::editor;
using Json = nlohmann::json;

namespace
{
    Json node(const std::string &id, const std::string &name, const std::string &type = "Node2D")
    {
        return {{"id", id}, {"name", name}, {"type", type}, {"x", 1}, {"y", 2}};
    }

    // a (with children a1 and a2), b
    SceneDocument sample()
    {
        SceneDocument document = SceneDocument::empty("Main");
        REQUIRE(document.insertNode("", 0, node("a", "A")));
        REQUIRE(document.insertNode("", 1, node("b", "B")));
        REQUIRE(document.insertNode("a", 0, node("a1", "A1")));
        REQUIRE(document.insertNode("a", 1, node("a2", "A2")));
        return document;
    }

    std::vector<std::string> ids(std::initializer_list<const char *> list)
    {
        return std::vector<std::string>(list.begin(), list.end());
    }
}

TEST_CASE("the file text is stable, so a scene read and written is the same bytes")
{
    SceneDocument document = SceneDocument::withRoot("Main", "Node2D");
    REQUIRE(document.insertNode("main", 0, node("a", "A")));
    REQUIRE(document.insertNode("a", 0, node("a1", "A1")));
    std::string first = document.dump();

    SceneDocument again;
    std::string error;
    REQUIRE(again.parse(first, &error));
    CHECK(again.dump() == first);
    CHECK(first.back() == '\n');
    CHECK(first.find("\"nodes\"") < first.find("\"version\""));
}

TEST_CASE("the scene files the engine's sandbox ships round-trip byte for byte" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    for (const char *name : {"sandbox", "sandbox_ui", "sandbox_3d"})
    {
        std::ifstream file(std::string(std::getenv("FRIED_ENGINE")) + "/sandbox/assets/scenes/" + name + ".scene.json");
        std::stringstream text;
        text << file.rdbuf();

        SceneDocument document;
        std::string error;
        REQUIRE(document.parse(text.str(), &error));
        CHECK(document.dump() == text.str());
    }
}

TEST_CASE("nodes are listed parents first, with their parent, place and depth")
{
    SceneDocument document = sample();
    CHECK(document.nodeIds() == ids({"a", "a1", "a2", "b"}));
    CHECK(document.nodeCount() == 4);
    CHECK(document.childrenOf("") == ids({"a", "b"}));
    CHECK(document.childrenOf("a") == ids({"a1", "a2"}));
    CHECK(document.childrenOf("b").empty());
    CHECK(document.parentOf("a2") == "a");
    CHECK(document.parentOf("a").empty());
    CHECK(document.indexOf("a2") == 1);
    CHECK(document.depthOf("a1") == 1);
    CHECK(document.depthOf("b") == 0);
    CHECK(document.isInside("a2", "a"));
    CHECK_FALSE(document.isInside("a", "a2"));
    CHECK(document.typeOf("a1") == "Node2D");
    CHECK(document.contains("b"));
    CHECK_FALSE(document.contains("zzz"));
}

TEST_CASE("nodes are inserted, moved and removed by id, and each edit reports itself")
{
    SceneDocument document = sample();
    std::vector<DocumentChange> changes;
    document.m_onChange = [&](const DocumentChange &change) { changes.push_back(change); };

    CHECK_FALSE(document.insertNode("", 0, node("a", "again")));
    CHECK_FALSE(document.insertNode("nope", 0, node("c", "C")));
    CHECK_FALSE(document.insertNode("", 9, node("c", "C")));
    CHECK(document.moveNode("b", "", 0));
    CHECK(document.nodeIds() == ids({"b", "a", "a1", "a2"}));

    Json removed;
    std::string parent;
    int index = -1;
    CHECK(document.removeNode("a", &removed, &parent, &index));
    CHECK(parent.empty());
    CHECK(index == 1);
    CHECK(removed["children"].size() == 2);
    CHECK(document.nodeIds() == ids({"b"}));
    CHECK(document.insertNode(parent, index, removed));
    CHECK(document.nodeIds() == ids({"b", "a", "a1", "a2"}));

    REQUIRE(changes.size() == 3);
    CHECK(changes[0].m_kind == DocumentChange::Kind::Moved);
    CHECK(changes[0].m_nodeId == "b");
    CHECK(changes[1].m_kind == DocumentChange::Kind::Removed);
    CHECK(changes[1].m_parentId.empty());
    CHECK(changes[1].m_index == 1);
    CHECK(changes[2].m_kind == DocumentChange::Kind::Added);
    CHECK(changes[2].m_value["children"].size() == 2);
    CHECK(document.isModified());
    CHECK(document.uniqueId("a") == "a-2");
    CHECK(document.uniqueId("c") == "c");
}

TEST_CASE("a node is moved under another or out of it, and never inside itself")
{
    SceneDocument document = sample();
    CHECK(document.moveNode("b", "a", 1));
    CHECK(document.childrenOf("a") == ids({"a1", "b", "a2"}));
    CHECK(document.parentOf("b") == "a");
    CHECK(document.depthOf("b") == 1);

    CHECK_FALSE(document.moveNode("a", "a", 0));
    CHECK_FALSE(document.moveNode("a", "a2", 0));
    CHECK_FALSE(document.moveNode("a", "b", 0));
    CHECK_FALSE(document.moveNode("b", "a", 9));

    // Within the same parent the index is the place once the node has left its old one.
    CHECK(document.moveNode("a1", "a", 2));
    CHECK(document.childrenOf("a") == ids({"b", "a2", "a1"}));

    CHECK(document.moveNode("a2", "", 0));
    CHECK(document.nodeIds() == ids({"a2", "a", "b", "a1"}));
}

TEST_CASE("a parent left with no children stops mentioning them, so the file holds no empty list")
{
    SceneDocument document = sample();
    Json removed;
    std::string parent;
    int index = 0;
    REQUIRE(document.removeNode("a1", &removed, &parent, &index));
    REQUIRE(document.removeNode("a2", &removed, &parent, &index));
    CHECK_FALSE(document.node("a")->contains("children"));
    CHECK(document.insertNode("a", 0, node("a3", "A3")));
    CHECK(document.node("a")->at("children").size() == 1);
}

TEST_CASE("removing a node takes the camera with it when the camera was inside")
{
    SceneDocument document = sample();
    document.setCameraId("a2");
    Json removed;
    std::string parent;
    int index = 0;
    REQUIRE(document.removeNode("a", &removed, &parent, &index));
    CHECK_FALSE(document.cameraId().has_value());
}

TEST_CASE("a node field can be set, read back and left to the engine's default again")
{
    SceneDocument document = sample();
    std::vector<DocumentChange> changes;
    document.m_onChange = [&](const DocumentChange &change) { changes.push_back(change); };

    CHECK(document.setNodeField("a", "x", 40));
    CHECK(document.nodeField("a", "x") == Json(40));
    CHECK(document.setNodeField("a", "x", std::nullopt));
    CHECK_FALSE(document.nodeField("a", "x").has_value());
    CHECK(document.setNodeField("a1", "name", Json("renamed")));
    CHECK(document.nodeField("a1", "name") == Json("renamed"));

    CHECK_FALSE(document.setNodeField("nope", "x", 1));
    CHECK_FALSE(document.setNodeField("a", "id", Json("z")));
    CHECK_FALSE(document.setNodeField("a", "type", Json("Sprite")));
    CHECK_FALSE(document.setNodeField("a", "children", Json::array()));

    REQUIRE(changes.size() == 3);
    CHECK(changes[0].m_kind == DocumentChange::Kind::Field);
    CHECK(changes[0].m_nodeId == "a");
    CHECK(changes[0].m_field == "x");
    CHECK(changes[0].m_value == 40);
}

TEST_CASE("replacing a node's contents keeps its children")
{
    SceneDocument document = sample();
    Json before;
    REQUIRE(document.replaceNode("a", Json{{"id", "a"}, {"type", "Sprite"}, {"name", "A"}}, &before));
    CHECK(document.typeOf("a") == "Sprite");
    CHECK(document.childrenOf("a") == ids({"a1", "a2"}));
    CHECK(before["type"] == "Node2D");
    CHECK_FALSE(before.contains("children"));
    CHECK_FALSE(document.replaceNode("a", Json{{"id", "other"}, {"type", "Sprite"}}, &before));
}

TEST_CASE("a file that is not a scene is refused with a reason")
{
    SceneDocument document;
    std::string error;
    CHECK_FALSE(document.parse("[]", &error));
    CHECK_FALSE(document.parse(R"({"version": 1, "nodes": []})", &error));
    CHECK(error.find("version 1") != std::string::npos);
    CHECK_FALSE(document.parse(R"({"version": 2, "objects": []})", &error));
    CHECK_FALSE(document.parse(R"({"version": 2, "nodes": [{"type": "Node", "name": "no id"}]})", &error));
    CHECK_FALSE(document.parse(R"({"version": 2, "nodes": [{"id": "a"}]})", &error));
    CHECK(error.find("no type") != std::string::npos);
    CHECK_FALSE(document.parse(R"({"version": 2, "nodes": [{"id": "a", "type": "Node", "children": [{"id": "a", "type": "Node"}]}]})", &error));
    CHECK(error.find("used twice") != std::string::npos);
    CHECK_FALSE(document.load("/nonexistent/scene.json", &error));
}

TEST_CASE("a node of a type the player does not know is kept as it is")
{
    SceneDocument document = SceneDocument::empty("Main");
    Json custom = {{"id", "a"}, {"name", "A"}, {"type", "Mystery"}, {"target", "b"}, {"depth", 3}};
    REQUIRE(document.insertNode("", 0, custom));

    std::string path = (std::filesystem::temp_directory_path() / "fried-unknown-type.scene.json").string();
    std::string error;
    REQUIRE(document.save(path, &error));
    SceneDocument reloaded = SceneDocument::empty("Other");
    REQUIRE(reloaded.load(path, &error));
    CHECK(reloaded.data()["nodes"][0] == custom);
    std::filesystem::remove(path);
}

TEST_CASE("a new scene has one root named like it, of the type chosen")
{
    SceneDocument document = SceneDocument::withRoot("Level One", "Node3D");
    CHECK(document.rootId() == "level-one");
    CHECK(document.typeOf("level-one") == "Node3D");
    CHECK(document.nodeCount() == 1);
    CHECK_FALSE(document.wasMigrated());
}

TEST_CASE("a file with several top-level nodes is read with them under a plain root, and says so")
{
    SceneDocument document = sample();
    SceneDocument read;
    std::string error;
    REQUIRE(read.parse(document.dump(), &error));
    CHECK(read.wasMigrated());
    CHECK(read.rootId() == "main-root");
    CHECK(read.typeOf("main-root") == "Node");
    CHECK(read.childrenOf("main-root") == ids({"a", "b"}));
    CHECK(read.parentOf("a1") == "a");

    SceneDocument again;
    REQUIRE(again.parse(read.dump(), &error));
    CHECK_FALSE(again.wasMigrated());
    CHECK(again.dump() == read.dump());
}
