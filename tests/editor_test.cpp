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

    editor.addObject("Text");
    CHECK(editor.hierarchy().size() == 4);
    CHECK(editor.inspector().size() == 2);
    editor.setPlatform(Platform::NintendoDs);
    pumpUntil(editor, [&] { return false; });
    CHECK(editor.status() == Editor::Status::Ready);
}
