#include "doctest/doctest.h"

#include "environment.h"

#include "fried/editor/editor.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <filesystem>
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
    editor.pointerUp();
    CHECK(editor.document().nodeField("sprite", "x") == nlohmann::json(288));

    editor.pointerDown(view.worldToFrameX(320), view.worldToFrameY(200), Editor::Pointer::Primary);
    editor.pointerMove(view.worldToFrameX(320) + 30, view.worldToFrameY(200) + 10);
    editor.pointerMove(view.worldToFrameX(320) + 40, view.worldToFrameY(200) + 20);
    editor.pointerUp();
    CHECK(editor.document().nodeField("sprite", "x") == nlohmann::json(328));
    CHECK(editor.document().isModified());

    size_t frames = editor.frame().m_rgba.size();
    pumpUntil(editor, [&] { return !editor.frame().m_bounds.empty() && editor.frame().m_bounds[1].m_x == 328; });
    CHECK(editor.frame().m_bounds[1].m_x == 328);
    CHECK(frames > 0);

    editor.commands().undo();
    CHECK(editor.document().nodeField("sprite", "x") == nlohmann::json(288));

    // Scale mode puts a handle on the sprite's right edge, which is 128 world pixels wide.
    editor.setGizmoMode(GizmoMode::Scale);
    pumpUntil(editor, [&] { return false; });
    double handleX = view.worldToFrameX(288 + 128);
    double handleY = view.worldToFrameY(160 + 64);
    CHECK(editor.pointerTarget(handleX, handleY).m_kind == Editor::PointerTarget::Kind::Handle);
    CHECK(editor.pointerTarget(handleX, handleY).m_handle == GizmoHandle::Kind::ScaleX);
    CHECK(editor.pointerTarget(view.worldToFrameX(288 + 64), view.worldToFrameY(160 + 64)).m_kind == Editor::PointerTarget::Kind::Object);
    CHECK(editor.pointerTarget(5, 5).m_kind == Editor::PointerTarget::Kind::None);
    editor.pointerDown(handleX, handleY, Editor::Pointer::Primary);
    editor.pointerMove(handleX + 64 * view.zoom(), handleY);
    editor.pointerUp();
    CHECK(editor.document().nodeField("sprite", "scaleX") == nlohmann::json(12.0));
    CHECK(editor.document().nodeField("sprite", "scaleY") == nlohmann::json(8));
    editor.commands().undo();
    CHECK(editor.document().nodeField("sprite", "scaleX") == nlohmann::json(8));

    // An image dropped on the sprite swaps its texture, and dropped on empty space makes a new sprite there.
    editor.dropAsset("game", "icon.png", view.worldToFrameX(320), view.worldToFrameY(200));
    CHECK(editor.document().nodeField("sprite", "texture") == nlohmann::json({{"root", "game"}, {"path", "icon.png"}}));
    editor.dropAsset("game", "sprite.png", 10, 10);
    CHECK(editor.hierarchy().size() == 4);
    CHECK(editor.selected() == std::optional<std::string>("sprite-2"));
    CHECK(editor.assets().size() > 4);
    editor.commands().undo();
    editor.commands().undo();
    CHECK(editor.hierarchy().size() == 3);

    // A row knows its type, and the classes it extends come after its own name.
    auto sprite = std::find_if(editor.hierarchy().begin(), editor.hierarchy().end(), [](const HierarchyRow &row) { return row.m_id == "sprite"; });
    REQUIRE(sprite != editor.hierarchy().end());
    CHECK(sprite->m_type == "Sprite");
    std::vector<std::string> chain = editor.classChain(sprite->m_type);
    REQUIRE(!chain.empty());
    CHECK(chain.front() == "Sprite");
    CHECK(std::find(chain.begin(), chain.end(), "Node2D") != chain.end());

    editor.addNode("Label");
    CHECK(editor.hierarchy().size() == 4);
    CHECK(editor.inspector().size() == 4);
    editor.commands().undo();

    // Changing the platform only changes what the viewport previews, so an edit still reaches the one value the scene has.
    editor.setPlatform(Platform::NintendoDs);
    editor.select(std::string("sprite"));
    editor.setField("scaleX", nlohmann::json(2));
    CHECK(editor.document().nodeField("sprite", "scaleX") == nlohmann::json(2));
    CHECK(editor.document().data().find("platforms") == editor.document().data().end());
    pumpUntil(editor, [&] { return false; });
    CHECK(editor.status() == Editor::Status::Ready);
}

TEST_CASE("the editor shows the sandbox's scene with the nodes of the game" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
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
    pumpUntil(editor, [&] { return !editor.frame().m_bounds.empty() && editor.frame().m_bounds.size() == 14; });
    REQUIRE(editor.frame().m_bounds.size() == 14);

    // A box is as big as it says, so the wall can be picked anywhere along it.
    for (const auto &bound : editor.frame().m_bounds)
    {
        if (bound.m_id == "wall")
        {
            CHECK(bound.m_width == 900);
            CHECK(bound.m_height == 48);
        }
    }

    editor.select(std::string("camera-follow"));
    bool hasReference = false;
    for (const ClassView &view : editor.inspector())
    {
        for (const FieldView &field : view.m_fields)
        {
            if (view.m_class == "CameraFollow" && field.m_schema.m_name == "target")
            {
                hasReference = field.m_schema.m_kind == "object" && field.m_value == "player";
            }
        }
    }
    CHECK(hasReference);

    // The inspector has a section for each class of the chain, the node's own first, and the name in the last.
    editor.select(std::string("player"));
    std::vector<ClassView> sections = editor.inspector();
    REQUIRE(sections.size() == 5);
    CHECK(sections[0].m_class == "Player");
    CHECK(sections[1].m_class == "Sprite");
    CHECK(sections[2].m_class == "Node2D");
    CHECK(sections[3].m_class == "CanvasItem");
    CHECK(sections[4].m_class == "Node");
    CHECK(sections[4].m_fields[0].m_schema.m_name == "name");
}

TEST_CASE("the move handle of a child is where the child is in the world, not at the world's origin" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
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
    pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 14; });
    REQUIRE(editor.frame().m_bounds.size() == 14);

    editor.setGizmoMode(GizmoMode::Move);
    for (const char *id : {"player-hitbox", "player", "post-0"})
    {
        const ObjectBounds *box = nullptr;
        for (const ObjectBounds &candidate : editor.frame().m_bounds)
        {
            if (candidate.m_id == id)
            {
                box = &candidate;
            }
        }
        REQUIRE(box != nullptr);
        REQUIRE(box->m_origin.has_value());
        editor.select(std::string(id));
        std::vector<GizmoHandle> handles = editor.handlesFor(editor.viewport());
        REQUIRE(handles.size() == 3);
        CHECK(handles[0].m_originX == doctest::Approx(editor.viewport().worldToFrameX(box->m_origin->first)));
        CHECK(handles[0].m_originY == doctest::Approx(editor.viewport().worldToFrameY(box->m_origin->second)));
    }

    // What is selected is dragged from anywhere inside its box, even where its child is drawn over it, and the child is not picked up instead.
    const ObjectBounds *player = nullptr;
    const ObjectBounds *hitbox = nullptr;
    for (const ObjectBounds &candidate : editor.frame().m_bounds)
    {
        if (candidate.m_id == "player")
        {
            player = &candidate;
        }
        else if (candidate.m_id == "player-hitbox")
        {
            hitbox = &candidate;
        }
    }
    REQUIRE(player != nullptr);
    REQUIRE(hitbox != nullptr);
    editor.select(std::string("player"));
    const Viewport &view = editor.viewport();
    // A point where the hitbox is what a press would pick and no handle is, since other nodes and the gizmo can cover parts of it.
    double pressX = 0;
    double pressY = 0;
    bool found = false;
    for (int column = 1; column < 8 && !found; ++column)
    {
        for (int row = 1; row < 8 && !found; ++row)
        {
            double x = view.worldToFrameX(hitbox->m_x + hitbox->m_width * column / 8);
            double y = view.worldToFrameY(hitbox->m_y + hitbox->m_height * row / 8);
            if (Viewport::hit(view, editor.frame().m_bounds, x, y) == std::optional<std::string>("player-hitbox") && !Gizmo::hit(editor.handlesFor(view), x, y))
            {
                pressX = x;
                pressY = y;
                found = true;
            }
        }
    }
    REQUIRE(found);
    CHECK(editor.pointerTarget(pressX, pressY).m_kind == Editor::PointerTarget::Kind::Object);
    auto before = editor.document().nodeField("player", "x");
    editor.pointerDown(pressX, pressY, Editor::Pointer::Primary);
    editor.pointerMove(pressX + 20 * view.zoom(), pressY);
    editor.pointerUp();
    CHECK(editor.selected() == std::optional<std::string>("player"));
    CHECK(editor.document().nodeField("player", "x") != before);

    // The hitbox's own x and y are zero, its parent's are not, and the handle follows where it is drawn.
    editor.select(std::string("player-hitbox"));
    CHECK(editor.handlesFor(editor.viewport())[0].m_originX != doctest::Approx(editor.viewport().worldToFrameX(0)));
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
    pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 14; });

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
    pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 14; });

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
    pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 14; });

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
    editor.setField("zoom", nlohmann::json(2.0));
    unsigned long serial = editor.frameSerial();
    pumpUntil(editor, [&] { return editor.frameSerial() > serial + 1; });
    CHECK(frameWidth() == doctest::Approx(320 * zoom));
}

TEST_CASE("the empty part of a frame has the editor's background color, before and after a resize" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready && !editor.frame().m_bounds.empty(); });
    REQUIRE(editor.openScene(engine + "/sandbox/assets/scenes/sandbox.scene.json", &error));
    pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 14; });

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

TEST_CASE("a scene opens with the view it was left with" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    namespace fs = std::filesystem;
    fs::path state = fs::temp_directory_path() / "fried-editor-view-test";
    fs::remove_all(state);
    setEnvironment("FRIED_EDITOR_CONFIG", state.string().c_str());

    std::string engine = std::getenv("FRIED_ENGINE");
    std::string sample = FRIED_EDITOR_TEST_DATA "/sample.scene.json";
    double x = 0, y = 0, zoom = 1;
    std::string error;
    {
        Editor editor;
        REQUIRE(editor.open(engine + "/sandbox", engine, &error));
        pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready; });
        REQUIRE(editor.status() == Editor::Status::Ready);
        REQUIRE(editor.openScene(sample, &error));
        pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 3; });

        editor.wheel(100, 100, 3);
        editor.pointerDown(50, 50, Editor::Pointer::Pan);
        editor.pointerMove(90, 70);
        editor.pointerUp();
        x = editor.viewport().cameraX();
        y = editor.viewport().cameraY();
        zoom = editor.viewport().zoom();
        REQUIRE(zoom != doctest::Approx(1.0));

        // Another scene has a view of its own, and coming back finds this one as it was.
        std::string other;
        for (const std::string &path : editor.scenePaths())
        {
            other = path;
        }
        REQUIRE(!other.empty());
        REQUIRE(editor.openScene(other, &error));
        CHECK(editor.viewport().zoom() != doctest::Approx(zoom));
        REQUIRE(editor.openScene(sample, &error));
        CHECK(editor.viewport().cameraX() == doctest::Approx(x));
        CHECK(editor.viewport().cameraY() == doctest::Approx(y));
        CHECK(editor.viewport().zoom() == doctest::Approx(zoom));
        editor.wheel(100, 100, 1);
        editor.saveView();
    }

    // A new editor, as after a restart, finds it as it was when the last one closed.
    {
        Editor editor;
        REQUIRE(editor.open(engine + "/sandbox", engine, &error));
        pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready; });
        REQUIRE(editor.openScene(sample, &error));
        CHECK(editor.viewport().zoom() != doctest::Approx(zoom));
        CHECK(editor.viewport().zoom() > zoom);
    }
    setEnvironment("FRIED_EDITOR_CONFIG", "none");
    fs::remove_all(state);
}

TEST_CASE("a node is duplicated, copied and pasted with its children, under new ids and names" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready; });
    REQUIRE(editor.openScene(FRIED_EDITOR_TEST_DATA "/sample.scene.json", &error));
    std::size_t before = editor.hierarchy().size();

    CHECK(editor.copySelected().empty());
    CHECK(!editor.duplicateSelected());

    editor.select(std::string("sprite"));
    std::string name = editor.document().nodeField("sprite", "name").value_or("").get<std::string>();
    REQUIRE(editor.duplicateSelected());
    CHECK(editor.hierarchy().size() == before + 1);
    REQUIRE(editor.selected());
    CHECK(*editor.selected() != "sprite");
    CHECK(editor.document().typeOf(*editor.selected()) == editor.document().typeOf("sprite"));
    CHECK(editor.document().parentOf(*editor.selected()) == editor.document().parentOf("sprite"));
    CHECK(editor.document().indexOf(*editor.selected()) == editor.document().indexOf("sprite") + 1);
    CHECK(editor.document().nodeField(*editor.selected(), "name").value_or("").get<std::string>() == name + "2");
    editor.commands().undo();
    CHECK(editor.hierarchy().size() == before);

    // Pasting twice gives two more, each with ids and names of its own.
    editor.select(std::string("sprite"));
    std::string text = editor.copySelected();
    CHECK(text.rfind("fried-nodes:", 0) == 0);
    REQUIRE(editor.paste(text));
    std::string first = *editor.selected();
    editor.select(std::string("sprite"));
    REQUIRE(editor.paste(text));
    CHECK(*editor.selected() != first);
    CHECK(editor.hierarchy().size() == before + 2);
    CHECK(editor.document().nodeField(first, "name") != editor.document().nodeField(*editor.selected(), "name"));
    editor.commands().undo();
    editor.commands().undo();
    CHECK(editor.hierarchy().size() == before);

    // Text that is not a copy pastes the last copy the editor made.
    CHECK(editor.paste("something else"));
    editor.commands().undo();
}
