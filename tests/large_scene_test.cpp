#include "doctest/doctest.h"

#include "fried/editor/commands.h"

#include <chrono>

using namespace fried::editor;
using Json = nlohmann::json;

namespace
{
    double millisecondsOf(const std::function<void()> &work)
    {
        auto start = std::chrono::steady_clock::now();
        work();
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
}

TEST_CASE("a scene of five thousand nodes is edited, moved and written quickly")
{
    SceneDocument document = SceneDocument::empty("Big");
    for (int group = 0; group < 50; ++group)
    {
        std::string parent = "g" + std::to_string(group);
        REQUIRE(document.insertNode("", group, Json{{"id", parent}, {"type", "Node2D"}}));
        Json children = Json::array();
        for (int child = 0; child < 99; ++child)
        {
            children.push_back(Json{{"id", parent + "-" + std::to_string(child)}, {"type", "Sprite"}, {"x", child}});
        }
        Json withChildren = *document.node(parent);
        withChildren["children"] = children;
        std::string removedParent;
        int index = 0;
        Json ignored;
        REQUIRE(document.removeNode(parent, &ignored, &removedParent, &index));
        REQUIRE(document.insertNode("", group, withChildren));
    }
    REQUIRE(document.nodeCount() == 5000);

    CommandStack stack(document);
    double move = millisecondsOf([&] {
        REQUIRE(stack.execute(std::make_unique<MoveNode>("g3-5", "g40", 0)));
        stack.undo();
        stack.redo();
        stack.undo();
    });
    double field = millisecondsOf([&] {
        for (int step = 0; step < 100; ++step)
        {
            stack.execute(std::make_unique<SetNodeField>("g20-50", "x", Json(step)), true);
        }
    });
    std::string text;
    double dump = millisecondsOf([&] { text = document.dump(); });
    MESSAGE("5000 nodes: a move, undo, redo and undo take ", move, " ms, 100 merged field edits ", field, " ms, dump ", dump, " ms");
    CHECK(move < 200);
    CHECK(dump < 200);
}
