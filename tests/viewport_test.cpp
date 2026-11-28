#include "doctest/doctest.h"

#include "fried/editor/commands.h"
#include "fried/editor/viewport.h"

using namespace fried::editor;

TEST_CASE("the view maps the world to the frame as the engine's camera does")
{
    Viewport view;
    view.setFrameSize(640, 480);
    view.setCamera(100, 50, 2);

    CHECK(view.worldToFrameX(100) == 320);
    CHECK(view.worldToFrameY(50) == 240);
    CHECK(view.worldToFrameX(110) == 340);
    CHECK(view.frameToWorldX(340) == 110);
    CHECK(view.frameToWorldY(view.worldToFrameY(33.5)) == doctest::Approx(33.5));
}

TEST_CASE("zooming keeps the point under the pointer where it was")
{
    Viewport view;
    view.setFrameSize(640, 480);
    view.setCamera(0, 0, 1);

    double before = view.frameToWorldX(500);
    view.zoomAt(500, 100, 2);
    CHECK(view.zoom() == 2);
    CHECK(view.frameToWorldX(500) == doctest::Approx(before));

    view.zoomAt(500, 100, 1000);
    CHECK(view.zoom() == Viewport::s_maxZoom);
}

TEST_CASE("panning moves the world with the pointer")
{
    Viewport view;
    view.setFrameSize(640, 480);
    view.setCamera(0, 0, 2);
    double before = view.frameToWorldX(100);
    view.panByFrame(40, 0);
    CHECK(view.frameToWorldX(140) == doctest::Approx(before));
}

TEST_CASE("a click picks the topmost object, which is the last drawn")
{
    Viewport view;
    view.setFrameSize(640, 480);
    view.setCamera(320, 240, 1);
    std::vector<ObjectBounds> bounds = {{"under", 0, 0, 100, 100}, {"over", 50, 50, 100, 100}, {"far", 400, 400, 10, 10}};

    CHECK(Viewport::hit(view, bounds, 60, 60) == std::optional<std::string>("over"));
    CHECK(Viewport::hit(view, bounds, 10, 10) == std::optional<std::string>("under"));
    CHECK_FALSE(Viewport::hit(view, bounds, 300, 10).has_value());
}

TEST_CASE("the overlay outlines the selection and the game's frame")
{
    Viewport view;
    view.setFrameSize(640, 480);
    view.setCamera(320, 240, 1);
    std::vector<ObjectBounds> bounds = {{"a", 10, 20, 30, 40}};

    DrawList list = view.overlay(bounds, std::string("a"), std::make_pair(320.0, 240.0), 640, 480);
    CHECK(list.size() == 4);
    CHECK(list.back().m_x == 10);
    CHECK(list.back().m_width == 30);
    CHECK(view.overlay(bounds, std::nullopt, std::nullopt, 0, 0).size() == 2);
}

TEST_CASE("dragging merges into one step and undoes to the start")
{
    SceneDocument document = SceneDocument::empty("Main");
    document.insertNode("", 0, nlohmann::json{{"id", "a"}, {"transform", {{"type", "Transform2D"}, {"x", 5}}}});
    CommandStack stack(document);

    for (int step = 1; step <= 4; ++step)
    {
        stack.execute(std::make_unique<SetFields>("a", ComponentRef{"", 0, true}, std::map<std::string, double>{{"x", 5.0 + step * 10}, {"y", step * 3.0}}), true);
    }
    CHECK(document.componentField("a", ComponentRef{"", 0, true}, "x") == nlohmann::json(45));
    stack.undo();
    CHECK(document.componentField("a", ComponentRef{"", 0, true}, "x") == nlohmann::json(5));
    CHECK_FALSE(document.componentField("a", ComponentRef{"", 0, true}, "y").has_value());
    CHECK_FALSE(stack.canUndo());
}

TEST_CASE("the grid has a line at every step in view, and fewer of them when zoomed out")
{
    Viewport view;
    view.setFrameSize(100, 100);
    view.setCamera(50, 50, 1);

    DrawList grid = view.grid(10);
    CHECK(grid.size() == 22);
    int vertical = 0;
    for (const DrawCommand &line : grid)
    {
        CHECK(line.m_kind == DrawCommand::Kind::Line);
        vertical += line.m_width == 0;
    }
    CHECK(vertical == 11);

    // At a tenth of the size the steps would be a pixel apart, so the lines keep to multiples of the step that are far enough.
    view.setCamera(500, 500, 0.1);
    view.setFrameSize(1000, 1000);
    DrawList far = view.grid(10);
    CHECK(far.size() <= 300);
    CHECK(view.grid(0).empty());
}
