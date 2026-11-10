#include "doctest/doctest.h"

#include "fried/editor/commands.h"
#include "fried/editor/gizmo.h"

using namespace fried::editor;

namespace
{
    Viewport view()
    {
        Viewport viewport;
        viewport.setFrameSize(640, 480);
        viewport.setCamera(320, 240, 1);
        return viewport;
    }
}

TEST_CASE("each mode puts its own handles on the object")
{
    Viewport viewport = view();
    ObjectBounds bounds = {"a", 100, 100, 80, 40};
    TransformValues transform;
    transform.m_x = 100;
    transform.m_y = 100;

    CHECK(Gizmo::handles(GizmoMode::Move, viewport, bounds, transform).size() == 2);
    CHECK(Gizmo::handles(GizmoMode::Scale, viewport, bounds, transform).size() == 3);
    auto ring = Gizmo::handles(GizmoMode::Rotate, viewport, bounds, transform);
    REQUIRE(ring.size() == 1);
    CHECK(ring[0].m_x == viewport.worldToFrameX(140));
    CHECK_FALSE(Gizmo::draw(ring).empty());
}

TEST_CASE("a handle is hit on its square, and the ring only near its line")
{
    Viewport viewport = view();
    ObjectBounds bounds = {"a", 100, 100, 80, 40};
    TransformValues transform;
    transform.m_x = 100;
    transform.m_y = 100;

    auto moves = Gizmo::handles(GizmoMode::Move, viewport, bounds, transform);
    double arrowX = viewport.worldToFrameX(100) + Gizmo::s_arrowLength;
    auto hit = Gizmo::hit(moves, arrowX + 3, viewport.worldToFrameY(100));
    REQUIRE(hit.has_value());
    CHECK(hit->m_kind == GizmoHandle::Kind::MoveX);
    CHECK_FALSE(Gizmo::hit(moves, arrowX + 40, viewport.worldToFrameY(100)).has_value());

    auto ring = Gizmo::handles(GizmoMode::Rotate, viewport, bounds, transform);
    CHECK(Gizmo::hit(ring, ring[0].m_x + ring[0].m_size, ring[0].m_y).has_value());
    CHECK_FALSE(Gizmo::hit(ring, ring[0].m_x, ring[0].m_y).has_value());
}

TEST_CASE("dragging an arrow moves along its axis only")
{
    Viewport viewport = view();
    ObjectBounds bounds = {"a", 100, 100, 80, 40};
    TransformValues transform;
    transform.m_x = 100;
    transform.m_y = 100;
    GizmoHandle arrow = Gizmo::handles(GizmoMode::Move, viewport, bounds, transform)[0];

    GizmoDrag drag(arrow, viewport, bounds, transform, arrow.m_x, arrow.m_y);
    auto fields = drag.update(viewport, arrow.m_x + 25, arrow.m_y + 90);
    CHECK(fields.size() == 1);
    CHECK(fields["x"] == 125);
}

TEST_CASE("dragging the ring turns the object by the angle the pointer sweeps")
{
    Viewport viewport = view();
    ObjectBounds bounds = {"a", 100, 100, 80, 40};
    TransformValues transform;
    transform.m_rotation = 10;
    GizmoHandle ring = Gizmo::handles(GizmoMode::Rotate, viewport, bounds, transform)[0];

    // From the right of the centre to below it is a quarter turn clockwise.
    GizmoDrag drag(ring, viewport, bounds, transform, ring.m_x + ring.m_size, ring.m_y);
    CHECK(drag.update(viewport, ring.m_x, ring.m_y + ring.m_size)["rotation"] == doctest::Approx(100));
}

TEST_CASE("dragging a scale handle scales by how much the size grew")
{
    Viewport viewport = view();
    ObjectBounds bounds = {"a", 100, 100, 80, 40};
    TransformValues transform;
    transform.m_scaleX = 2;
    transform.m_scaleY = 2;
    auto handles = Gizmo::handles(GizmoMode::Scale, viewport, bounds, transform);

    GizmoDrag across(handles[0], viewport, bounds, transform, handles[0].m_x, handles[0].m_y);
    auto fields = across.update(viewport, handles[0].m_x + 80, handles[0].m_y);
    CHECK(fields.size() == 1);
    CHECK(fields["scaleX"] == doctest::Approx(4));

    GizmoDrag corner(handles[2], viewport, bounds, transform, handles[2].m_x, handles[2].m_y);
    auto both = corner.update(viewport, handles[2].m_x + 80, handles[2].m_y + 40);
    CHECK(both["scaleX"] == both["scaleY"]);
    CHECK(both["scaleX"] == doctest::Approx(4));
}

TEST_CASE("a handle drag is one step to undo")
{
    SceneDocument document = SceneDocument::empty("Main");
    document.insertObject(0, nlohmann::json{{"id", "a"}, {"components", nlohmann::json::array({nlohmann::json{{"type", "Transform"}}})}});
    CommandStack stack(document);

    for (int step = 1; step <= 3; ++step)
    {
        stack.execute(std::make_unique<SetTransformFields>("a", std::map<std::string, double>{{"scaleX", 1.0 + step}}), true);
    }
    CHECK(document.componentField("a", "Transform", "scaleX") == nlohmann::json(4.0));
    stack.undo();
    CHECK_FALSE(document.componentField("a", "Transform", "scaleX").has_value());
}
