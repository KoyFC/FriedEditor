#include "doctest/doctest.h"

#include "environment.h"

#include "fried/editor/editor.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
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
    CHECK(editor.hierarchy().size() == 5);
    CHECK(editor.selected() == std::optional<std::string>("sprite-2"));
    CHECK(editor.assets().size() > 4);
    editor.commands().undo();
    editor.commands().undo();
    CHECK(editor.hierarchy().size() == 4);

    // A row knows its type, and the classes it extends come after its own name.
    auto sprite = std::find_if(editor.hierarchy().begin(), editor.hierarchy().end(), [](const HierarchyRow &row) { return row.m_id == "sprite"; });
    REQUIRE(sprite != editor.hierarchy().end());
    CHECK(sprite->m_type == "Sprite");
    std::vector<std::string> chain = editor.classChain(sprite->m_type);
    REQUIRE(!chain.empty());
    CHECK(chain.front() == "Sprite");
    CHECK(std::find(chain.begin(), chain.end(), "Node2D") != chain.end());

    editor.addNode("Label");
    CHECK(editor.hierarchy().size() == 5);
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
    for (const char *id : {"post-0-hitbox", "player", "post-0"})
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
        if (candidate.m_id == "post-0")
        {
            player = &candidate;
        }
        else if (candidate.m_id == "post-0-hitbox")
        {
            hitbox = &candidate;
        }
    }
    REQUIRE(player != nullptr);
    REQUIRE(hitbox != nullptr);
    editor.select(std::string("post-0"));
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
            if (Viewport::hit(view, editor.frame().m_bounds, x, y) == std::optional<std::string>("post-0-hitbox") && !Gizmo::hit(editor.handlesFor(view), x, y))
            {
                pressX = x;
                pressY = y;
                found = true;
            }
        }
    }
    REQUIRE(found);
    CHECK(editor.pointerTarget(pressX, pressY).m_kind == Editor::PointerTarget::Kind::Object);
    auto before = editor.document().nodeField("post-0", "x");
    editor.pointerDown(pressX, pressY, Editor::Pointer::Primary);
    editor.pointerMove(pressX + 20 * view.zoom(), pressY);
    editor.pointerUp();
    CHECK(editor.selected() == std::optional<std::string>("post-0"));
    CHECK(editor.document().nodeField("post-0", "x") != before);

    // The hitbox's own x and y are zero, its parent's are not, and the handle follows where it is drawn.
    editor.select(std::string("post-0-hitbox"));
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

TEST_CASE("dragging a node snaps to the grid when it is on, or when the snap key is held while it is off" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready; });
    REQUIRE(editor.openScene(FRIED_EDITOR_TEST_DATA "/sample.scene.json", &error));
    pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 3; });
    const Viewport &view = editor.viewport();

    editor.select(std::string("sprite"));
    auto drag = [&] {
        editor.pointerDown(view.worldToFrameX(320), view.worldToFrameY(200), Editor::Pointer::Primary);
        editor.pointerMove(view.worldToFrameX(320) + 30, view.worldToFrameY(200) + 10);
        editor.pointerMove(view.worldToFrameX(320) + 37, view.worldToFrameY(200) + 13);
        editor.pointerUp();
    };
    auto isOnGrid = [&](double value) { return std::fmod(value, 16.0) == 0; };

    drag();
    CHECK(editor.document().nodeField("sprite", "x") == nlohmann::json(325));
    editor.commands().undo();

    Editor::GridSettings settings = editor.grid();
    settings.m_isSnapping = true;
    editor.setGrid(settings);
    drag();
    CHECK(isOnGrid(editor.document().nodeField("sprite", "x")->get<double>()));
    CHECK(isOnGrid(editor.document().nodeField("sprite", "y")->get<double>()));
    editor.commands().undo();

    // Holding the key turns snapping the other way.
    editor.setSnapHeld(true);
    drag();
    CHECK(editor.document().nodeField("sprite", "x") == nlohmann::json(325));
    editor.setSnapHeld(false);
}

TEST_CASE("the scenes opened last are remembered, the latest first" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    namespace fs = std::filesystem;
    fs::path state = fs::temp_directory_path() / "fried-editor-recent-test";
    fs::remove_all(state);
    setEnvironment("FRIED_EDITOR_CONFIG", state.string().c_str());

    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    std::vector<std::string> scenes = editor.scenePaths();
    REQUIRE(scenes.size() >= 3);

    REQUIRE(editor.openScene(scenes[0], &error));
    REQUIRE(editor.openScene(scenes[1], &error));
    REQUIRE(editor.openScene(scenes[2], &error));
    REQUIRE(editor.openScene(scenes[0], &error));
    std::vector<std::string> recent = editor.recentScenes();
    REQUIRE(recent.size() >= 3);
    CHECK(recent[0] == scenes[0]);
    CHECK(recent[1] == scenes[2]);
    CHECK(recent[2] == scenes[1]);
    CHECK(std::count(recent.begin(), recent.end(), scenes[0]) == 1);

    setEnvironment("FRIED_EDITOR_CONFIG", "none");
    fs::remove_all(state);
}

TEST_CASE("the view is brought to the selected node, zoomed back to a pixel scale and reset" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready; });
    REQUIRE(editor.openScene(FRIED_EDITOR_TEST_DATA "/sample.scene.json", &error));
    pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 3; });

    editor.select(std::string("sprite"));
    editor.focusSelected();
    // The sprite is 128 pixels wide at x 288 and 128 high at y 160.
    CHECK(editor.viewport().cameraX() == doctest::Approx(288 + 64));
    CHECK(editor.viewport().cameraY() == doctest::Approx(160 + 64));

    editor.zoomTo(2);
    CHECK(editor.viewport().zoom() == doctest::Approx(2));
    CHECK(editor.viewport().cameraX() == doctest::Approx(288 + 64));
    editor.zoomTo(1);
    CHECK(editor.viewport().zoom() == doctest::Approx(1));

    editor.resetView();
    CHECK(editor.viewport().zoom() == doctest::Approx(1));
    CHECK(editor.viewport().cameraX() == doctest::Approx(editor.viewport().frameWidth() / 2.0));
}

TEST_CASE("a locked node is not picked or moved in the view, and the lock is remembered for the scene" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    namespace fs = std::filesystem;
    fs::path state = fs::temp_directory_path() / "fried-editor-lock-test";
    fs::remove_all(state);
    setEnvironment("FRIED_EDITOR_CONFIG", state.string().c_str());

    std::string engine = std::getenv("FRIED_ENGINE");
    std::string sample = FRIED_EDITOR_TEST_DATA "/sample.scene.json";
    std::string error;
    {
        Editor editor;
        REQUIRE(editor.open(engine + "/sandbox", engine, &error));
        pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready; });
        REQUIRE(editor.openScene(sample, &error));
        pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 3; });
        const Viewport &view = editor.viewport();

        editor.select(std::string("sprite"));
        editor.setLocked("sprite", true);
        CHECK(editor.isLocked("sprite"));
        CHECK(editor.pointerTarget(view.worldToFrameX(352), view.worldToFrameY(224)).m_kind != Editor::PointerTarget::Kind::Handle);

        // Dragging it does not move it, and a click on it does not pick it, but the hierarchy still can.
        editor.pointerDown(view.worldToFrameX(320), view.worldToFrameY(200), Editor::Pointer::Primary);
        editor.pointerMove(view.worldToFrameX(320) + 30, view.worldToFrameY(200) + 10);
        editor.pointerMove(view.worldToFrameX(320) + 40, view.worldToFrameY(200) + 20);
        editor.pointerUp();
        CHECK(editor.document().nodeField("sprite", "x") == nlohmann::json(288));
        editor.select(std::nullopt);
        editor.pointerDown(view.worldToFrameX(320), view.worldToFrameY(200), Editor::Pointer::Primary);
        editor.pointerUp();
        CHECK(editor.selected() != std::optional<std::string>("sprite"));
        editor.select(std::string("sprite"));
        CHECK(editor.selected() == std::optional<std::string>("sprite"));
    }
    {
        Editor editor;
        REQUIRE(editor.open(engine + "/sandbox", engine, &error));
        REQUIRE(editor.openScene(sample, &error));
        CHECK(editor.isLocked("sprite"));
        editor.setLocked("sprite", false);
        CHECK_FALSE(editor.isLocked("sprite"));
    }
    {
        Editor editor;
        REQUIRE(editor.open(engine + "/sandbox", engine, &error));
        REQUIRE(editor.openScene(sample, &error));
        CHECK_FALSE(editor.isLocked("sprite"));
    }
    setEnvironment("FRIED_EDITOR_CONFIG", "none");
    fs::remove_all(state);
}

TEST_CASE("scenes open in tabs that keep their own changes and history" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    std::string scenes = engine + "/sandbox/assets/scenes/";
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    REQUIRE(editor.openScene(scenes + "sandbox.scene.json", &error));
    REQUIRE(editor.openScene(scenes + "sandbox_ui.scene.json", &error));
    CHECK(editor.activeSceneIndex() == (int)editor.openScenes().size() - 1);

    editor.select(std::string("hints"));
    editor.setField("text", nlohmann::json("changed"));
    CHECK(editor.isSceneModified(scenes + "sandbox_ui.scene.json"));
    CHECK_FALSE(editor.isSceneModified(scenes + "sandbox.scene.json"));

    // Going to another tab leaves the change and its undo where they were.
    REQUIRE(editor.openScene(scenes + "sandbox.scene.json", &error));
    CHECK(editor.scenePath() == scenes + "sandbox.scene.json");
    CHECK_FALSE(editor.commands().canUndo());
    CHECK(editor.isSceneModified(scenes + "sandbox_ui.scene.json"));
    CHECK(editor.hasModifiedScenes());
    REQUIRE(editor.openScene(scenes + "sandbox_ui.scene.json", &error));
    CHECK(editor.commands().canUndo());
    CHECK(editor.document().nodeField("hints", "text") == nlohmann::json("changed"));
    editor.commands().undo();
    CHECK_FALSE(editor.hasModifiedScenes());

    size_t open = editor.openScenes().size();
    REQUIRE(editor.closeScene(scenes + "sandbox_ui.scene.json", &error));
    CHECK(editor.openScenes().size() == open - 1);
    CHECK(editor.scenePath() != scenes + "sandbox_ui.scene.json");
    while (editor.openScenes().size() > 1)
    {
        REQUIRE(editor.closeScene(editor.openScenes().back(), &error));
    }
    CHECK_FALSE(editor.closeScene(editor.openScenes().front(), &error));
}

TEST_CASE("a scene that instances another shows as one node of the class of the other's root" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    std::string scenes = engine + "/sandbox/assets/scenes/";
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready || editor.status() == Editor::Status::Failed; });
    REQUIRE(editor.status() == Editor::Status::Ready);
    REQUIRE(editor.openScene(scenes + "sandbox.scene.json", &error));
    CHECK(editor.instanceOf("player") == "player");
    CHECK(editor.document().typeOf("player") == "Player");

    // What the entry does not set is what the instanced file says, not the engine's default.
    editor.select(std::string("player"));
    bool isSpeedFound = false;
    for (const ClassView &view : editor.inspector())
    {
        for (const FieldView &field : view.m_fields)
        {
            if (field.m_schema.m_name == "speed")
            {
                isSpeedFound = true;
                CHECK_FALSE(field.m_isSet);
                CHECK(field.m_value == nlohmann::json(240));
            }
        }
    }
    CHECK(isSpeedFound);

    // A scene cannot hold itself, directly or through a scene it instances, and every scene has one root, so any of them can be instanced.
    CHECK_FALSE(editor.instanceProblem("sandbox").empty());
    CHECK(editor.instanceProblem("sandbox_ui").empty());
    CHECK(editor.instanceProblem("sandbox_3d").empty());
    CHECK_FALSE(editor.instanceProblem("nothing-here").empty());
    REQUIRE(editor.openScene(scenes + "sandbox_ui.scene.json", &error));
    CHECK_FALSE(editor.instanceProblem("sandbox").empty());

    // Adding one is a command like any other.
    REQUIRE(editor.openScene(scenes + "sandbox.scene.json", &error));
    REQUIRE(editor.addInstance(scenes + "player.scene.json", std::nullopt, &error));
    CHECK(editor.instanceOf("player-2") == "player");
    editor.commands().undo();
    CHECK_FALSE(editor.document().contains("player-2"));
    CHECK(editor.sceneIssues().empty());
}

TEST_CASE("the root node cannot be deleted, duplicated or moved, and what has no parent goes inside it" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready; });
    REQUIRE(editor.openScene(FRIED_EDITOR_TEST_DATA "/sample.scene.json", &error));

    std::string root = editor.document().rootId();
    REQUIRE_FALSE(root.empty());
    CHECK(editor.hierarchy().front().m_id == root);
    CHECK(editor.hierarchy().front().m_depth == 0);

    size_t count = editor.document().nodeCount();
    editor.select(root);
    editor.removeSelected();
    CHECK_FALSE(editor.duplicateSelected());
    editor.moveSelected(1);
    editor.reparent(root, "sprite", 0);
    CHECK(editor.document().nodeCount() == count);
    CHECK(editor.document().parentOf(root).empty());

    // With the root selected or with nothing selected, a new node, a pasted one and an instance go under the root.
    editor.addNode("Label");
    CHECK(editor.document().parentOf(*editor.selected()) == root);
    editor.select(root);
    editor.addNode("Sprite", editor.selected());
    CHECK(editor.document().parentOf(*editor.selected()) == root);
    editor.copySelected();
    editor.select(root);
    REQUIRE(editor.paste(""));
    CHECK(editor.document().parentOf(*editor.selected()) == root);
}

TEST_CASE("G, R and S change the selected node with the pointer or a typed number, and cancelling leaves no trace" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready && !editor.frame().m_bounds.empty(); });
    REQUIRE(editor.openScene(FRIED_EDITOR_TEST_DATA "/sample.scene.json", &error));
    pumpUntil(editor, [&] { return editor.frame().m_bounds.size() == 3; });
    REQUIRE(editor.frame().m_bounds.size() == 3);

    const Viewport &view = editor.viewport();
    editor.pointerDown(view.worldToFrameX(320), view.worldToFrameY(200), Editor::Pointer::Primary);
    editor.pointerUp();
    REQUIRE(editor.selected() == std::optional<std::string>("sprite"));
    REQUIRE(editor.document().nodeField("sprite", "x") == nlohmann::json(288));

    double startX = view.worldToFrameX(320);
    double startY = view.worldToFrameY(200);

    // Moving follows the pointer, and an axis holds the other one still.
    REQUIRE(editor.beginModal(Editor::Modal::Move, startX, startY));
    editor.modalAxis('x');
    editor.modalMove(startX + 30 * view.zoom(), startY + 50 * view.zoom());
    CHECK(editor.document().nodeField("sprite", "x") == nlohmann::json(318));
    CHECK(editor.document().nodeField("sprite", "y") == nlohmann::json(160));
    CHECK(editor.modalText() == "Move X");
    editor.confirmModal();
    CHECK_FALSE(editor.isModal());
    editor.commands().undo();
    CHECK(editor.document().nodeField("sprite", "x") == nlohmann::json(288));

    // A typed number replaces the pointer.
    REQUIRE(editor.beginModal(Editor::Modal::Move, startX, startY));
    editor.modalAxis('y');
    editor.modalType('-');
    editor.modalType('1');
    editor.modalType('2');
    CHECK(editor.modalText() == "Move Y: -12");
    CHECK(editor.document().nodeField("sprite", "y") == nlohmann::json(148));
    editor.modalBackspace();
    CHECK(editor.document().nodeField("sprite", "y") == nlohmann::json(159));
    editor.confirmModal();
    editor.commands().undo();

    // Cancelling puts the node back and there is nothing to redo.
    REQUIRE(editor.beginModal(Editor::Modal::Scale, startX + 10, startY));
    editor.modalType('3');
    CHECK(editor.document().nodeField("sprite", "scaleX") == nlohmann::json(24));
    editor.cancelModal();
    CHECK(editor.document().nodeField("sprite", "scaleX") == nlohmann::json(8));
    CHECK_FALSE(editor.commands().canRedo());

    REQUIRE(editor.beginModal(Editor::Modal::Rotate, startX + 10, startY));
    editor.modalType('9');
    editor.modalType('0');
    CHECK(editor.document().nodeField("sprite", "rotation") == nlohmann::json(90.0));
    editor.cancelModal();
    CHECK(editor.commands().canRedo() == false);
}

TEST_CASE("save scene as writes a new file, edits it and leaves the old one as it was saved" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    Editor editor;
    std::string error;
    REQUIRE(editor.open(engine + "/sandbox", engine, &error));
    pumpUntil(editor, [&] { return editor.status() == Editor::Status::Ready; });
    REQUIRE(editor.openScene(FRIED_EDITOR_TEST_DATA "/sample.scene.json", &error));
    auto contents = [](const std::string &path) {
        std::ifstream in(path);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    const std::string original = contents(FRIED_EDITOR_TEST_DATA "/sample.scene.json");
    const std::string target = engine + "/sandbox/assets/scenes/SaveAsTest.scene.json";
    std::filesystem::remove(target);

    editor.addNode("Label");
    REQUIRE(editor.saveSceneAs(" SaveAsTest ", &error));
    CHECK(std::filesystem::path(editor.scenePath()).filename().string() == "SaveAsTest.scene.json");
    CHECK(std::filesystem::exists(target));
    CHECK(editor.commands().isClean());
    CHECK(contents(FRIED_EDITOR_TEST_DATA "/sample.scene.json") == original);
    // A name another scene has, and a name with a folder in it, are refused.
    CHECK_FALSE(editor.saveSceneAs("sandbox", &error));
    CHECK_FALSE(editor.saveSceneAs("a/b", &error));
    std::filesystem::remove(target);
}
