#include "doctest/doctest.h"

#include "fried/editor/editor.h"

#include <algorithm>
#include <cmath>
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
    REQUIRE(editor.openScene(FRIED_EDITOR_TEST_DATA "/sample.scene.json", &error));
    pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 3; });
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
    editor.commands().undo();

    // Changing the platform only changes what the viewport previews, so an edit still reaches the one value the scene has.
    editor.setPlatform(Platform::NintendoDs);
    editor.select(std::string("sprite"));
    editor.setField("Transform", "scaleX", nlohmann::json(2));
    CHECK(editor.document().componentField("sprite", "Transform", "scaleX") == nlohmann::json(2));
    CHECK(editor.document().data().find("platforms") == editor.document().data().end());
    pumpUntil(editor, [&] { return false; });
    CHECK(editor.status() == Editor::Status::Ready);
}

TEST_CASE("the editor shows the sandbox's scene with the components of the game" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    for (int attempt = 0; attempt < 120000 && editor.status() != Editor::Status::Ready && editor.status() != Editor::Status::Failed; ++attempt)
    {
        editor.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(editor.status() == Editor::Status::Ready);
    REQUIRE(editor.openScene(engine + "/sandbox/assets/scenes/sandbox.scene.json", &error));
    pumpUntil(editor, [&] { return !editor.frame().m_bounds.empty() && editor.frame().m_bounds.size() == 8; });
    REQUIRE(editor.frame().m_bounds.size() == 8);

    // A box is as big as it says, so the wall can be picked anywhere along it.
    for (const auto &bound : editor.frame().m_bounds)
    {
        if (bound.m_id == "wall")
        {
            CHECK(bound.m_width == 900);
            CHECK(bound.m_height == 48);
        }
    }

    editor.select(std::string("camera"));
    bool hasReference = false;
    for (const ComponentView &component : editor.inspector())
    {
        for (const FieldView &field : component.m_fields)
        {
            if (component.m_type == "CameraFollow" && field.m_schema.m_name == "target")
            {
                hasReference = field.m_schema.m_kind == "object" && field.m_value == "player";
            }
        }
    }
    CHECK(hasReference);
}

TEST_CASE("the overlay stays with the picture it was drawn for while the view is panned" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready && !editor.frame().m_bounds.empty(); });
    REQUIRE(editor.status() == Editor::Status::Ready);

    auto before = editor.overlay();
    unsigned long serial = editor.frameSerial();
    editor.pointerDown(100, 100, Editor::Pointer::Pan);
    editor.pointerMove(160, 130);
    editor.pointerUp();

    auto during = editor.overlay();
    REQUIRE(before.size() == during.size());
    for (size_t index = 0; index < before.size(); ++index)
    {
        CHECK(before[index].m_x == during[index].m_x);
        CHECK(before[index].m_y == during[index].m_y);
    }

    pumpUntil(editor, [&] { return editor.frameSerial() > serial + 1; });
    auto after = editor.overlay();
    CHECK(after[0].m_x != before[0].m_x);
}

TEST_CASE("the picture and the overlay agree on where the wall is, at any pan and zoom" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready && !editor.frame().m_bounds.empty(); });
    REQUIRE(editor.openScene(engine + "/sandbox/assets/scenes/sandbox.scene.json", &error));
    pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 8; });

    struct Step
    {
        double m_panX, m_panY, m_zoom;
    };
    for (Step step : {Step{0, 0, 1}, Step{80, 40, 1}, Step{-120, 10, 1}, Step{0, 0, 3}})
    {
        if (step.m_zoom != 1)
        {
            editor.wheel(640, 360, 1.0);
            editor.wheel(640, 360, 1.0);
        }
        editor.pointerDown(100, 100, Editor::Pointer::Pan);
        editor.pointerMove(100 + step.m_panX, 100 + step.m_panY);
        editor.pointerUp();
        unsigned long serial = editor.frameSerial();
        pumpUntil(editor, [&] { return editor.frameSerial() > serial + 1; });

        const Frame &frame = editor.frame();
        int minX = frame.m_width, maxX = -1, minY = frame.m_height, maxY = -1;
        for (int y = 0; y < frame.m_height; ++y)
        {
            for (int x = 0; x < frame.m_width; ++x)
            {
                const unsigned char *pixel = &frame.m_rgba[((size_t)y * frame.m_width + x) * 4];
                if (pixel[0] == 70 && pixel[1] == 70 && pixel[2] == 90)
                {
                    minX = std::min(minX, x);
                    maxX = std::max(maxX, x);
                    minY = std::min(minY, y);
                    maxY = std::max(maxY, y);
                }
            }
        }
        const Viewport &view = editor.viewport();
        INFO("frame ", frame.m_width, "x", frame.m_height, " zoom ", view.zoom(), " camera ", view.cameraX(), ",", view.cameraY());
        double expectedLeft = view.worldToFrameX(-450), expectedTop = view.worldToFrameY(160);
        double expectedRight = view.worldToFrameX(450), expectedBottom = view.worldToFrameY(208);
        // The wall's border takes a pixel or two off each side of its fill.
        CHECK(std::abs(minY - expectedTop) <= 3);
        CHECK(std::abs(maxY - expectedBottom) <= 3);
        CHECK(std::abs(maxX - expectedRight) <= 3);
    }
}

TEST_CASE("how long a camera move takes to come back as a frame" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready && !editor.frame().m_bounds.empty(); });
    REQUIRE(editor.openScene(engine + "/sandbox/assets/scenes/sandbox.scene.json", &error));
    pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 8; });

    editor.setViewSize(864, 490);
    pumpUntil(editor, [&] { return editor.frame().m_width == 864; });
    REQUIRE(editor.frame().m_width == 864);
    REQUIRE(editor.frame().m_height == 490);

    using Clock = std::chrono::steady_clock;
    double total = 0, worst = 0;
    const int moves = 40;
    for (int index = 0; index < moves; ++index)
    {
        unsigned long serial = editor.frameSerial();
        auto start = Clock::now();
        editor.pointerDown(100, 100, Editor::Pointer::Pan);
        editor.pointerMove(100 + 7, 100 + 3);
        editor.pointerUp();
        while (editor.frameSerial() == serial)
        {
            editor.pump();
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
        double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        total += elapsed;
        worst = std::max(worst, elapsed);
    }
    MESSAGE("frame ", editor.frame().m_width, "x", editor.frame().m_height, ": a pan step takes ", total / moves, " ms on average and ", worst, " ms at worst");
}

TEST_CASE("the display frame is the display's size divided by the camera's zoom" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready && !editor.frame().m_bounds.empty(); });
    REQUIRE(editor.openScene(engine + "/sandbox/assets/scenes/sandbox.scene.json", &error));
    pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 8; });

    auto frameWidth = [&] {
        for (const DrawCommand &command : editor.overlay())
        {
            if (command.m_kind == DrawCommand::Kind::Rect && command.m_rgba == 0xa0ffffff)
            {
                return command.m_width;
            }
        }
        return -1.0;
    };
    double zoom = editor.viewport().zoom();
    CHECK(frameWidth() == doctest::Approx(640 * zoom));

    editor.select(std::string("camera"));
    editor.setField("Camera", "zoom", nlohmann::json(2.0));
    unsigned long serial = editor.frameSerial();
    pumpUntil(editor, [&] { return editor.frameSerial() > serial + 1; });
    CHECK(frameWidth() == doctest::Approx(320 * zoom));
}

TEST_CASE("the empty part of a frame has the editor's background colour, before and after a resize" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready && !editor.frame().m_bounds.empty(); });
    REQUIRE(editor.openScene(engine + "/sandbox/assets/scenes/sandbox.scene.json", &error));
    pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 8; });

    auto corner = [&](int x, int y) {
        const Frame &frame = editor.frame();
        const unsigned char *pixel = &frame.m_rgba[((size_t)y * frame.m_width + x) * 4];
        return std::to_string(pixel[0]) + "," + std::to_string(pixel[1]) + "," + std::to_string(pixel[2]);
    };
    auto empty = [&] { return corner(editor.frame().m_width - 3, editor.frame().m_height - 3); };
    CHECK(empty() == "40,40,48");

    editor.setViewSize(864, 490);
    pumpUntil(editor, [&] { return editor.frame().m_width == 864; });
    REQUIRE(editor.frame().m_width == 864);
    CHECK(empty() == "40,40,48");
}
