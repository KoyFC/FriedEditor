#include "doctest/doctest.h"

#include "fried/editor/editor.h"

#include <chrono>
#include <cstdlib>
#include <thread>

using namespace fried::editor;

namespace
{
    void pumpUntil(Editor &editor, const std::function<bool()> &done)
    {
        for (int attempt = 0; attempt < 1000 && !done(); ++attempt)
        {
            editor.pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

// Runs only when FRIED_ENGINE points at an engine checkout whose player is built.
TEST_CASE("the editor shows a project's scene, picks and drags an object, and undoes it" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready && !editor.frame().m_bounds.empty(); });
    REQUIRE(editor.status() == Editor::Status::Ready);
    REQUIRE(editor.frame().m_bounds.size() == 3);

    // The sprite sits at 288, 160 and the view's camera starts at the middle of the frame.
    const Viewport &view = editor.viewport();
    editor.pointerDown(view.worldToFrameX(320), view.worldToFrameY(200), Editor::Pointer::Primary);
    CHECK(editor.selected() == std::optional<std::string>("sprite"));
    editor.pointerMove(view.worldToFrameX(320) + 30, view.worldToFrameY(200) + 10);
    editor.pointerMove(view.worldToFrameX(320) + 40, view.worldToFrameY(200) + 20);
    editor.pointerUp();
    CHECK(editor.document().componentField("sprite", "Transform", "x") == nlohmann::json(328));
    CHECK(editor.document().isModified());

    size_t frames = editor.frame().m_rgba.size();
    pumpUntil(editor, [&] { return !editor.frame().m_bounds.empty() && editor.frame().m_bounds[1].m_x == 328; });
    CHECK(editor.frame().m_bounds[1].m_x == 328);
    CHECK(frames > 0);

    editor.commands().undo();
    CHECK(editor.document().componentField("sprite", "Transform", "x") == nlohmann::json(288));

    // Scale mode puts a handle on the sprite's right edge, which is 128 world pixels wide.
    editor.setGizmoMode(GizmoMode::Scale);
    pumpUntil(editor, [&] { return false; });
    double handleX = view.worldToFrameX(288 + 128);
    double handleY = view.worldToFrameY(160 + 64);
    editor.pointerDown(handleX, handleY, Editor::Pointer::Primary);
    editor.pointerMove(handleX + 64 * view.zoom(), handleY);
    editor.pointerUp();
    CHECK(editor.document().componentField("sprite", "Transform", "scaleX") == nlohmann::json(12.0));
    CHECK(editor.document().componentField("sprite", "Transform", "scaleY") == nlohmann::json(8));
    editor.commands().undo();
    CHECK(editor.document().componentField("sprite", "Transform", "scaleX") == nlohmann::json(8));

    // An image dropped on the sprite swaps its texture, and dropped on empty space makes a new sprite there.
    editor.dropAsset("game", "icon.png", view.worldToFrameX(320), view.worldToFrameY(200));
    CHECK(editor.document().componentField("sprite", "Sprite", "texture") == nlohmann::json({{"root", "game"}, {"path", "icon.png"}}));
    editor.dropAsset("game", "sprite.png", 10, 10);
    CHECK(editor.hierarchy().size() == 4);
    CHECK(editor.selected() == std::optional<std::string>("sprite-2"));
    CHECK(editor.assets().size() > 4);
    editor.commands().undo();
    editor.commands().undo();
    CHECK(editor.hierarchy().size() == 3);

    editor.addObject("Text");
    CHECK(editor.hierarchy().size() == 4);
    CHECK(editor.inspector().size() == 2);
    editor.setPlatform(Platform::NintendoDs);
    pumpUntil(editor, [&] { return false; });
    CHECK(editor.status() == Editor::Status::Ready);
}
