#include "doctest/doctest.h"

#include "fried/editor/commands.h"

using namespace fried::editor;
using Json = nlohmann::json;

namespace
{
    Json object(const std::string &id)
    {
        return {{"id", id}, {"name", id}, {"components", Json::array({Json{{"type", "Transform"}, {"x", 1}}, Json{{"type", "Sprite"}}})}};
    }

    SceneDocument sample()
    {
        SceneDocument document = SceneDocument::empty("Main");
        document.insertObject(0, object("a"));
        document.insertObject(1, object("b"));
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
    document.setOverrideField("nds", "a", "Transform", "x", 5);
    document.setCameraId("b");
    CommandStack stack(document);

    roundTrips(document, stack, [] { return std::make_unique<SetComponentField>("a", "Transform", "x", Json(9)); });
    roundTrips(document, stack, [] { return std::make_unique<SetComponentField>("a", "Transform", "x", std::nullopt); });
    roundTrips(document, stack, [] { return std::make_unique<SetObjectField>("a", "priority", Json(2)); });
    roundTrips(document, stack, [] { return std::make_unique<AddObject>(1, object("c")); });
    roundTrips(document, stack, [] { return std::make_unique<RemoveObject>("a"); });
    roundTrips(document, stack, [] { return std::make_unique<RemoveObject>("b"); });
    roundTrips(document, stack, [] { return std::make_unique<MoveObject>("a", 1); });
    roundTrips(document, stack, [] { return std::make_unique<AddComponent>("a", 2, Json{{"type", "Camera"}}); });
    roundTrips(document, stack, [] { return std::make_unique<RemoveComponent>("a", "Sprite"); });
    roundTrips(document, stack, [] { return std::make_unique<SetOverrideField>("nds", "a", "Transform", "x", Json(7)); });
    roundTrips(document, stack, [] { return std::make_unique<SetOverrideField>("vita", "a", "Transform", "y", Json(7)); });
    roundTrips(document, stack, [] { return std::make_unique<SetCamera>(std::nullopt); });
}

TEST_CASE("removing an object takes its overrides and camera role, and undo returns them")
{
    SceneDocument document = sample();
    document.setOverrideField("nds", "a", "Transform", "x", 5);
    document.setCameraId("a");
    CommandStack stack(document);

    REQUIRE(stack.execute(std::make_unique<RemoveObject>("a")));
    CHECK_FALSE(document.cameraId().has_value());
    CHECK(document.data().find("platforms") == document.data().end());

    stack.undo();
    CHECK(document.cameraId() == std::optional<std::string>("a"));
    CHECK(document.overrideField("nds", "a", "Transform", "x") == Json(5));
}

TEST_CASE("a drag is one step to undo when its edits are merged")
{
    SceneDocument document = sample();
    CommandStack stack(document);

    for (int x = 10; x <= 50; x += 10)
    {
        stack.execute(std::make_unique<SetComponentField>("a", "Transform", "x", Json(x)), true);
    }
    CHECK(document.componentField("a", "Transform", "x") == Json(50));
    stack.execute(std::make_unique<SetComponentField>("a", "Transform", "y", Json(3)), true);

    stack.undo();
    CHECK_FALSE(document.componentField("a", "Transform", "y").has_value());
    CHECK(document.componentField("a", "Transform", "x") == Json(50));
    stack.undo();
    CHECK(document.componentField("a", "Transform", "x") == Json(1));
    CHECK_FALSE(stack.canUndo());
}

TEST_CASE("a new edit drops what was undone, and the clean position follows saves")
{
    SceneDocument document = sample();
    CommandStack stack(document);
    int notified = 0;
    stack.m_onChange = [&] { ++notified; };

    CHECK(stack.isClean());
    stack.execute(std::make_unique<SetObjectField>("a", "name", Json("one")));
    CHECK_FALSE(stack.isClean());
    stack.markClean();
    CHECK(stack.isClean());
    CHECK_FALSE(document.isModified());

    stack.undo();
    CHECK_FALSE(stack.isClean());
    stack.execute(std::make_unique<SetObjectField>("a", "name", Json("two")));
    CHECK_FALSE(stack.canRedo());
    CHECK_FALSE(stack.isClean());
    CHECK(stack.undoLabel() == "Change name");
    CHECK(notified == 3);

    CHECK_FALSE(stack.execute(std::make_unique<SetObjectField>("missing", "name", Json("x"))));
}

TEST_CASE("dragging on a platform changes only its overrides, as one step")
{
    SceneDocument document = sample();
    std::string before = document.dump();
    CommandStack stack(document);

    for (int x = 10; x <= 30; x += 10)
    {
        stack.execute(std::make_unique<SetOverrideTransformFields>("nds", "a", std::map<std::string, double>{{"x", (double)x}, {"y", 2.0}}), true);
    }
    CHECK(document.overrideField("nds", "a", "Transform", "x") == Json(30.0));
    CHECK(document.componentField("a", "Transform", "x") == Json(1));

    stack.undo();
    CHECK(document.dump() == before);
    CHECK_FALSE(stack.canUndo());
}
