#include "doctest/doctest.h"

#include "fried/editor/editor.h"
#include "fried/editor/editor_state.h"
#include "fried/editor/player_builder.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace fried::editor;
namespace fs = std::filesystem;

namespace
{
    struct StateFolder
    {
        fs::path m_path = fs::temp_directory_path() / "fried-editor-state-test";

        StateFolder()
        {
            fs::remove_all(m_path);
            setenv("FRIED_EDITOR_CONFIG", m_path.string().c_str(), 1);
        }

        ~StateFolder()
        {
            setenv("FRIED_EDITOR_CONFIG", "none", 1);
            fs::remove_all(m_path);
        }
    };
}

TEST_CASE("the last scene of each project is remembered")
{
    StateFolder folder;
    CHECK(EditorState::lastScene("/projects/a").empty());

    EditorState::setLastScene("/projects/a", "level.scene.json");
    EditorState::setLastScene("/projects/b", "menu.scene.json");
    EditorState::setLastScene("/projects/a", "boss.scene.json");

    CHECK(EditorState::lastScene("/projects/a") == "boss.scene.json");
    CHECK(EditorState::lastScene("/projects/b") == "menu.scene.json");
}

TEST_CASE("nothing is remembered when the state is turned off")
{
    setenv("FRIED_EDITOR_CONFIG", "none", 1);
    EditorState::setLastScene("/projects/a", "level.scene.json");
    CHECK(EditorState::lastScene("/projects/a").empty());
}

TEST_CASE("the end of a log is what is shown")
{
    fs::path log = fs::temp_directory_path() / "fried-tail-test.log";
    std::ofstream(log) << "one\ntwo\nthree\nfour\n";
    CHECK(tailOfFile(log.string(), 2) == "three\nfour\n");
    CHECK(tailOfFile(log.string(), 10) == "one\ntwo\nthree\nfour\n");
    CHECK(tailOfFile((fs::temp_directory_path() / "fried-no-such.log").string(), 3).empty());
    fs::remove(log);
}

TEST_CASE("a build step says what it does")
{
    PlayerBuilder builder({{{"true"}, {}, "", ""}});
    CHECK(builder.stepCount() == 1);
    CHECK(builder.step() == 0);
    CHECK(builder.stepLabel().empty());
}

TEST_CASE("the editor reopens the scene it had open, and shows the build while it is going on" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    StateFolder folder;
    std::string engine = std::getenv("FRIED_ENGINE");
    std::string error;
    {
        Editor first;
        REQUIRE(first.open(engine + "/sandbox", engine, &error));
        CHECK(fs::path(first.scenePath()).filename() == "sandbox.scene.json");
        REQUIRE(first.openScene(engine + "/sandbox/assets/scenes/sandbox_ui.scene.json", &error));
    }
    Editor second;
    REQUIRE(second.open(engine + "/sandbox", engine, &error));
    CHECK(fs::path(second.scenePath()).filename() == "sandbox_ui.scene.json");

    for (int attempt = 0; attempt < 120000 && second.status() != Editor::Status::Ready && second.status() != Editor::Status::Failed; ++attempt)
    {
        second.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(second.status() == Editor::Status::Ready);

    second.play();
    BuildProgress progress = second.buildProgress();
    CHECK(progress.m_isVisible);
    CHECK(progress.m_canStop);
    CHECK(progress.m_title == "Building the game");
    second.stopGame();
    CHECK_FALSE(second.buildProgress().m_isVisible);
}
