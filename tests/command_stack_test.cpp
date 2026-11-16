#include "doctest/doctest.h"

#include "fried/editor/commands.h"

using namespace fried::editor;
using Json = nlohmann::json;

namespace
{
    Json node(const std::string &id)
    {
        return {{"id", id}, {"name", id}, {"type", "Sprite"}, {"x", 1}, {"flip", "None"}};
    }

    // a (with child a1), b
    SceneDocument sample()
    {
        SceneDocument document = SceneDocument::empty("Main");
        document.insertNode("", 0, node("a"));
        document.insertNode("", 1, node("b"));
        document.insertNode("a", 0, node("a1"));
        document.markSaved();
        return document;
    }

    template <typename Make>
    void roundTrips(SceneDocument &document, CommandStack &stack, Make make)
    {
        std::string before = document.dump();
        REQUIRE(stack.execute(make()));
        CHECK(document.dump() != before);
        std::string after = document.dump();
        stack.undo();
        CHECK(document.dump() == before);
        stack.redo();
        CHECK(document.dump() == after);
        stack.undo();
    }
}

TEST_CASE("every command undoes to the exact bytes it started from, and redoes to the same result")
{
    SceneDocument document = sample();
    document.setCameraId("b");
    CommandStack stack(document);

    roundTrips(document, stack, [] { return std::make_unique<SetNodeField>("a", "x", Json(9)); });
    roundTrips(document, stack, [] { return std::make_unique<SetNodeField>("a", "x", std::nullopt); });
    roundTrips(document, stack, [] { return std::make_unique<SetNodeField>("a", "priority", Json(2)); });
    roundTrips(document, stack, [] { return std::make_unique<SetNodeField>("a", "name", Json("renamed")); });
    roundTrips(document, stack, [] { return std::make_unique<SetNodeFields>("a", std::map<std::string, double>{{"x", 5}, {"y", 6}}); });
    roundTrips(document, stack, [] { return std::make_unique<AddNode>("", 1, node("c")); });
    roundTrips(document, stack, [] { return std::make_unique<AddNode>("a", 1, node("c")); });
    roundTrips(document, stack, [] { return std::make_unique<RemoveNode>("a"); });
    roundTrips(document, stack, [] { return std::make_unique<RemoveNode>("a1"); });
    roundTrips(document, stack, [] { return std::make_unique<RemoveNode>("b"); });
    roundTrips(document, stack, [] { return std::make_unique<MoveNode>("a", "", 1); });
    roundTrips(document, stack, [] { return std::make_unique<MoveNode>("b", "a", 0); });
    roundTrips(document, stack, [] { return std::make_unique<MoveNode>("a1", "", 0); });
    roundTrips(document, stack, [] { return std::make_unique<ChangeNodeType>("a", "Label", std::set<std::string>{"x"}); });
    roundTrips(document, stack, [] { return std::make_unique<SetCamera>(std::nullopt); });
}

TEST_CASE("removing a node takes its subtree and its camera role, and undo returns both")
{
    SceneDocument document = sample();
    document.setCameraId("a1");
    CommandStack stack(document);

    REQUIRE(stack.execute(std::make_unique<RemoveNode>("a")));
    CHECK_FALSE(document.cameraId().has_value());
    CHECK_FALSE(document.contains("a1"));

    stack.undo();
    CHECK(document.cameraId() == std::optional<std::string>("a1"));
    CHECK(document.parentOf("a1") == "a");
}

TEST_CASE("a node that is not moved anywhere is not a step to undo")
{
    SceneDocument document = sample();
    CommandStack stack(document);
    CHECK_FALSE(stack.execute(std::make_unique<MoveNode>("a", "", 0)));
    CHECK_FALSE(stack.execute(std::make_unique<MoveNode>("a", "a1", 0)));
    CHECK_FALSE(stack.canUndo());
}

TEST_CASE("changing a node's type keeps the fields the new class has and the name, and undo brings everything back")
{
    SceneDocument document = sample();
    CommandStack stack(document);
    REQUIRE(stack.execute(std::make_unique<ChangeNodeType>("a", "Label", std::set<std::string>{"x", "text"})));
    CHECK(document.typeOf("a") == "Label");
    CHECK(document.nodeField("a", "x") == Json(1));
    CHECK(document.nodeField("a", "name") == Json("a"));
    CHECK_FALSE(document.nodeField("a", "flip").has_value());
    CHECK(document.childrenOf("a") == std::vector<std::string>({"a1"}));

    stack.undo();
    CHECK(document.typeOf("a") == "Sprite");
    CHECK(document.nodeField("a", "flip") == Json("None"));
    CHECK_FALSE(stack.execute(std::make_unique<ChangeNodeType>("a", "Sprite", std::set<std::string>{})));
}

TEST_CASE("a drag is one step to undo when its edits are merged")
{
    SceneDocument document = sample();
    CommandStack stack(document);

    for (int x = 10; x <= 50; x += 10)
    {
        stack.execute(std::make_unique<SetNodeField>("a", "x", Json(x)), true);
    }
    CHECK(document.nodeField("a", "x") == Json(50));
    stack.execute(std::make_unique<SetNodeField>("a", "y", Json(3)), true);

    stack.undo();
    CHECK_FALSE(document.nodeField("a", "y").has_value());
    CHECK(document.nodeField("a", "x") == Json(50));
    stack.undo();
    CHECK(document.nodeField("a", "x") == Json(1));
    CHECK_FALSE(stack.canUndo());
}

TEST_CASE("a new edit drops what was undone, and the clean position follows saves")
{
    SceneDocument document = sample();
    CommandStack stack(document);
    int notified = 0;
    stack.m_onChange = [&] { ++notified; };

    CHECK(stack.isClean());
    stack.execute(std::make_unique<SetNodeField>("a", "name", Json("one")));
    CHECK_FALSE(stack.isClean());
    stack.markClean();
    CHECK(stack.isClean());
    CHECK_FALSE(document.isModified());

    stack.undo();
    CHECK_FALSE(stack.isClean());
    stack.execute(std::make_unique<SetNodeField>("a", "name", Json("two")));
    CHECK_FALSE(stack.canRedo());
    CHECK_FALSE(stack.isClean());
    CHECK(stack.undoLabel() == "Change name");
    CHECK(notified == 3);

    CHECK_FALSE(stack.execute(std::make_unique<SetNodeField>("missing", "name", Json("x"))));
}
