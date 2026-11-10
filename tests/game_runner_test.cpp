#include "fried/editor/game_runner.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
using fried::editor::GameRunner;

TEST_CASE("the game runner reads the target from the project's CMakeLists")
{
    fs::path directory = fs::temp_directory_path() / "fried-game-runner-test";
    fs::create_directories(directory);
    std::ofstream(directory / "CMakeLists.txt") << "project(x)\nfried_add_game( my_game )\n";

    CHECK(GameRunner::targetName(directory.string()) == "my_game");
    CHECK(GameRunner::executablePath(directory.string()) == directory.string() + "/build/my_game");

    auto steps = GameRunner::steps(directory.string(), "");
    REQUIRE(steps.size() == 3);
    CHECK(steps[0].m_arguments[0] == "haxe");
    CHECK(steps[2].m_workDirectory == directory.string());

    std::string error;
    GameRunner runner;
    fs::remove(directory / "CMakeLists.txt");
    CHECK_FALSE(runner.start(directory.string(), "", &error));
    CHECK(runner.state() == GameRunner::State::Failed);
    fs::remove_all(directory);
}

TEST_CASE("the game runner builds and runs the sandbox")
{
    const char *engine = std::getenv("FRIED_ENGINE");
    if (!engine)
    {
        return;
    }
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    setenv("SDL_AUDIODRIVER", "dummy", 1);
    setenv("SDL_RENDER_DRIVER", "software", 1);
    std::string sandbox = std::string(engine) + "/sandbox";
    GameRunner runner;
    std::string error;
    REQUIRE(runner.start(sandbox, (fs::temp_directory_path() / "fried-game-runner-test.log").string(), &error));
    for (int i = 0; i < 3000 && runner.state() == GameRunner::State::Building; ++i)
    {
        runner.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    REQUIRE(runner.state() == GameRunner::State::Running);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    CHECK(runner.pump() == GameRunner::State::Running);
    runner.stop();
    CHECK(runner.state() == GameRunner::State::Idle);
}
