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

TEST_CASE("a hold keeps what was done before out of reach and takes back everything done after it")
{
    SceneDocument document = sample();
    CommandStack stack(document);
    REQUIRE(stack.execute(std::make_unique<SetNodeField>("a", "x", Json(5))));
    std::string before = document.dump();

    stack.hold();
    CHECK(stack.isHeld());
    CHECK_FALSE(stack.canUndo());

    // A drag that continues the step from before the hold is a step of its own.
    REQUIRE(stack.execute(std::make_unique<SetNodeField>("a", "x", Json(6)), true));
    REQUIRE(stack.execute(std::make_unique<SetNodeField>("b", "x", Json(7))));
    CHECK(stack.canUndo());

    stack.revertHeld();
    CHECK_FALSE(stack.isHeld());
    CHECK(document.dump() == before);
    CHECK_FALSE(stack.canRedo());
    CHECK(stack.canUndo());
}

TEST_CASE("the steps are listed from the oldest and a jump undoes or redoes up to the one chosen")
{
    SceneDocument document = sample();
    CommandStack stack(document);
    std::string original = document.dump();
    REQUIRE(stack.execute(std::make_unique<SetNodeField>("a", "x", Json(5))));
    std::string afterFirst = document.dump();
    REQUIRE(stack.execute(std::make_unique<SetNodeField>("b", "x", Json(6))));
    REQUIRE(stack.execute(std::make_unique<SetNodeField>("b", "x", Json(7))));
    CHECK(stack.stepCount() == 3);
    CHECK(stack.position() == 3);
    CHECK(stack.firstStep() == 0);

    stack.jumpTo(1);
    CHECK(document.dump() == afterFirst);
    CHECK(stack.position() == 1);
    CHECK(stack.canRedo());

    stack.jumpTo(0);
    CHECK(document.dump() == original);

    stack.jumpTo(99);
    CHECK(stack.position() == 3);
    CHECK(document.nodeField("b", "x").value() == Json(7));
}

TEST_CASE("the held steps are handed back oldest first and can be made again")
{
    SceneDocument document = sample();
    CommandStack stack(document);
    REQUIRE(stack.execute(std::make_unique<SetNodeField>("a", "x", Json(5))));
    std::string before = document.dump();

    stack.hold();
    REQUIRE(stack.execute(std::make_unique<SetNodeField>("b", "x", Json(6))));
    REQUIRE(stack.execute(std::make_unique<SetNodeField>("a1", "x", Json(7))));
    REQUIRE(stack.execute(std::make_unique<SetNodeField>("a1", "x", Json(8))));
    stack.undo();
    std::string withChanges = document.dump();

    auto taken = stack.takeHeld();
    CHECK_FALSE(stack.isHeld());
    CHECK(document.dump() == before);
    // The step that had been undone is not among them.
    REQUIRE(taken.size() == 2);
    CHECK(stack.stepCount() == 1);

    for (auto &command : taken)
    {
        REQUIRE(stack.execute(std::move(command)));
    }
    CHECK(document.dump() == withChanges);
}

TEST_CASE("reverting fields takes them out of the node in one step, and undo puts them back")
{
    SceneDocument document = sample();
    CommandStack stack(document);
    std::string before = document.dump();

    REQUIRE(stack.execute(std::make_unique<ClearNodeFields>("a", std::vector<std::string>{"x", "flip"})));
    CHECK_FALSE(document.nodeField("a", "x").has_value());
    CHECK_FALSE(document.nodeField("a", "flip").has_value());
    CHECK(document.nodeField("a", "name").has_value());

    stack.undo();
    CHECK(document.dump() == before);
    stack.redo();
    CHECK_FALSE(document.nodeField("a", "x").has_value());
}
