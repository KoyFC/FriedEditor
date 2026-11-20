#include "doctest/doctest.h"

#include "fried/editor/viewport3d.h"

using namespace fried::editor;

TEST_CASE("the 3D view projects the way the engine's Camera3D does")
{
    Viewport3D view;
    view.setFrameSize(200, 100);
    view.set(0, 0, 0, 10, 0, 0);

    auto origin = view.project(0, 0, 0);
    REQUIRE(origin.has_value());
    CHECK(origin->first == doctest::Approx(100));
    CHECK(origin->second == doctest::Approx(50));

    auto right = view.project(1, 0, 0);
    REQUIRE(right.has_value());
    CHECK(right->first == doctest::Approx(100 + 86.6025 / 10));

    auto above = view.project(0, 1, 0);
    REQUIRE(above.has_value());
    CHECK(above->second < 50);

    CHECK_FALSE(view.project(0, 0, -20).has_value());
}

TEST_CASE("the 3D view looks about its own position, flies, zooms and focuses")
{
    Viewport3D view;
    view.setFrameSize(200, 100);
    view.set(0, 0, 0, 10, 0, 0);
    CHECK(view.z() == doctest::Approx(-10));

    view.look(100, 0);
    CHECK(view.rotationY() == doctest::Approx(30));
    CHECK(view.z() == doctest::Approx(-10));

    view.look(0, 1000);
    CHECK(view.rotationX() == doctest::Approx(89));

    view.set(0, 0, 0, 10, 0, 0);
    view.fly(3, 2, 1);
    CHECK(view.z() == doctest::Approx(-7));
    CHECK(view.x() == doctest::Approx(2));
    CHECK(view.y() == doctest::Approx(1));

    view.dolly(1);
    CHECK(view.distance() == doctest::Approx(10 / 1.1));

    view.focus(1, 2, 3);
    CHECK(view.targetY() == 2);
}

TEST_CASE("panning the 3D view moves the target against the pointer")
{
    Viewport3D view;
    view.setFrameSize(200, 100);
    view.set(0, 0, 0, 10, 0, 0);
    view.pan(10, 0);
    CHECK(view.targetX() < 0);
    CHECK(view.targetY() == doctest::Approx(0));
    view.pan(0, 10);
    CHECK(view.targetY() > 0);
}

TEST_CASE("a line behind the camera is dropped and one that crosses is cut")
{
    Viewport3D view;
    view.setFrameSize(200, 100);
    view.set(0, 0, 0, 10, 0, 0);

    DrawList list;
    view.addLine(list, 0, 0, -20, 1, 0, -30, 0xffffffff);
    CHECK(list.empty());
    view.addLine(list, 0, 0, -20, 0, 0, 5, 0xffffffff);
    REQUIRE(list.size() == 1);
    CHECK(list[0].m_x == doctest::Approx(100));
}

TEST_CASE("the ground grid has lines that stop at the near plane")
{
    Viewport3D view;
    view.setFrameSize(640, 480);
    view.set(0, 0, 0, 10, 0, 20);
    CHECK(view.grid().size() > 20);
}
