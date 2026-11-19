#include "environment.h"
#include "machines.h"
#include "doctest/doctest.h"

#include "fried/editor/project_settings.h"

#include "fried/core/image.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

using namespace fried::editor;
namespace fs = std::filesystem;

namespace
{
    struct TemporaryProject
    {
        fs::path m_root;
        Project m_project;

        explicit TemporaryProject(const std::string &name)
            : m_root(fs::temp_directory_path() / ("fried-editor-settings-" + name))
        {
            fs::remove_all(m_root);
            fs::create_directories(m_root / "assets" / "scenes");
            write("project.fried", R"({"name": "Sample", "organization": "Org", "version": "01.00", "gameClass": "Main"})");
            write("assets/scenes/main.scene.json", "{}");
            write("assets/scenes/menu.scene.json", "{}");
            write("assets/scenes/notes.txt", "");
            std::string error;
            REQUIRE(m_project.load(m_root.string(), &error));
        }

        ~TemporaryProject() { fs::remove_all(m_root); }

        void write(const std::string &relative, const std::string &text) const { std::ofstream(m_root / relative, std::ios::binary) << text; }

        std::string read(const std::string &relative) const
        {
            std::ifstream file(m_root / relative, std::ios::binary);
            std::stringstream text;
            text << file.rdbuf();
            return text.str();
        }
    };
}

TEST_CASE("a setting that passes its check is written to the project file at once")
{
    TemporaryProject project("save");
    ProjectSettings settings(project.m_project);
    int saves = 0;
    settings.m_onSaved = [&saves] { ++saves; };

    CHECK(settings.setName("Renamed").empty());
    CHECK(settings.setWindowTitle("A window").empty());
    CHECK(settings.setVitaTitleId("SAMP00001").empty());
    CHECK(settings.setStartScene("menu").empty());
    CHECK(settings.setPackagingText("nds", "subtitle1", "A line").empty());
    CHECK(saves == 5);

    Project onDisk;
    std::string error;
    REQUIRE(onDisk.load(project.m_root.string(), &error));
    CHECK(onDisk.name() == "Renamed");
    CHECK(onDisk.windowTitle() == "A window");
    CHECK(onDisk.vitaTitleId() == "SAMP00001");
    CHECK(onDisk.startScene() == "menu");
    CHECK(onDisk.packagingText("nds", "subtitle1") == "A line");
    CHECK(project.read("project.fried").find("\"gameClass\": \"Main\"") != std::string::npos);

    // Setting what is already there writes nothing.
    CHECK(settings.setName("Renamed").empty());
    CHECK(saves == 5);
}

TEST_CASE("a setting that fails its check leaves the project and its file as they were")
{
    TemporaryProject project("refuse");
    ProjectSettings settings(project.m_project);
    std::string before = project.read("project.fried");
    int saves = 0;
    settings.m_onSaved = [&saves] { ++saves; };

    CHECK(settings.setName("a/b").find("The name may not contain") == 0);
    CHECK(settings.setOrganization("") == "The organization must not be empty.");
    CHECK(settings.setVersion("1.0").find("The version must be two digits") == 0);
    CHECK(settings.setVersion("") == "The version must not be empty.");
    CHECK(settings.setVitaTitleId("sample").find("The title ID must be four capital letters") == 0);
    CHECK(settings.setStartScene("missing") == "The project has no scene named missing.");
    CHECK(settings.setPackagingText("nds", "subtitle1", std::string(32, 'a')) == "This line holds 31 characters at most.");
    CHECK(settings.setPackagingText("3ds", "description", std::string(129, 'a')) == "This line holds 128 characters at most.");
    CHECK(settings.setDisplay(DisplaySettings{"fit", 0, 480, "nearest"}).find("must be above zero") != std::string::npos);
    CHECK(settings.setDisplay(DisplaySettings{"cover", 640, 480, "nearest"}) == "The engine has no display mode named cover.");

    CHECK(saves == 0);
    CHECK(project.m_project.name() == "Sample");
    CHECK(project.read("project.fried") == before);

    // Thirty one letters fit however many bytes they take.
    std::string accented;
    for (int index = 0; index < 31; ++index)
    {
        accented += "\xc3\xb1";
    }
    CHECK(settings.setPackagingText("nds", "subtitle2", accented).empty());
}

TEST_CASE("the display is saved and changes the size the game draws in on a platform")
{
    TemporaryProject project("display");
    ProjectSettings settings(project.m_project);
    CHECK(project.m_project.logicalSize(Platform::NintendoDs) == std::make_pair(256, 192));

    CHECK(settings.setDisplay(DisplaySettings{"fit", 640, 480, "linear"}).empty());
    CHECK(project.m_project.logicalSize(Platform::NintendoDs) == std::make_pair(640, 480));
    CHECK(project.m_project.display().m_filter == "linear");

    CHECK(settings.setDisplay(DisplaySettings{"default", 0, 0, "nearest"}).empty());
    CHECK(project.m_project.logicalSize(Platform::NintendoDs) == std::make_pair(256, 192));
    CHECK(project.read("project.fried").find("width") == std::string::npos);
}

TEST_CASE("vsync is saved as it is turned off and on")
{
    TemporaryProject project("vsync");
    ProjectSettings settings(project.m_project);
    CHECK(project.m_project.vsync());

    CHECK(settings.setVsync(false).empty());
    CHECK(project.read("project.fried").find("\"vsync\": false") != std::string::npos);

    CHECK(settings.setVsync(true).empty());
    CHECK(project.read("project.fried").find("vsync") == std::string::npos);
}

TEST_CASE("the physics rate is saved, leaves no key at 60 and refuses one out of range")
{
    TemporaryProject project("physics-rate");
    ProjectSettings settings(project.m_project);
    CHECK(project.m_project.physicsRate() == 60);
    CHECK(settings.setPhysicsRate(0) == "The physics rate must be between 1 and 1000 steps a second.");
    CHECK(settings.setPhysicsRate(1001) == "The physics rate must be between 1 and 1000 steps a second.");
    CHECK(settings.setPhysicsRate(30).empty());
    CHECK(project.read("project.fried").find("\"physicsRate\": 30") != std::string::npos);
    CHECK(settings.setPhysicsRate(60).empty());
    CHECK(project.read("project.fried").find("physicsRate") == std::string::npos);
}

TEST_CASE("the default target framerate is saved and refuses a negative one")
{
    TemporaryProject project("framerate");
    ProjectSettings settings(project.m_project);
    CHECK(settings.setDefaultTargetFramerate(-1) == "The target framerate cannot be negative.");
    CHECK(settings.setDefaultTargetFramerate(144).empty());
    CHECK(project.read("project.fried").find("\"defaultTargetFramerate\": 144") != std::string::npos);
    CHECK(settings.setDefaultTargetFramerate(0).empty());
    CHECK(project.read("project.fried").find("defaultTargetFramerate") == std::string::npos);
}

TEST_CASE("the start scene is one of the project's scene files")
{
    TemporaryProject project("scenes");
    ProjectSettings settings(project.m_project);
    CHECK(settings.startSceneChoices() == std::vector<std::string>{"main", "menu"});
}

TEST_CASE("an image becomes a platform's icon files in the project")
{
    TemporaryProject project("icon");
    ProjectSettings settings(project.m_project);

    fried::core::Image art;
    art.m_width = 200;
    art.m_height = 100;
    art.m_rgba.assign((size_t)200 * 100 * 4, 180);
    std::string source = (project.m_root / "art.png").string();
    REQUIRE(fried::core::writeImage(source, art));

    std::vector<std::string> notes;
    std::string error;
    REQUIRE(settings.setIcon(Platform::NintendoDs, source, &notes, &error));
    CHECK(notes == std::vector<std::string>{"Uses the center 100x100 of 200x100.", "Reduced from 100x100."});
    CHECK(settings.iconFiles(Platform::NintendoDs) == std::vector<std::string>{project.m_root.string() + "/nds/icon.bmp"});
    int width = 0, height = 0;
    REQUIRE(fried::core::imageSize(project.m_root.string() + "/nds/icon.bmp", &width, &height));
    CHECK(width == 32);

    REQUIRE(settings.setIcon(Platform::Cg50, source, &notes, &error));
    CHECK(fs::exists(project.m_root / "cg50" / "icon-uns.png"));
    CHECK(fs::exists(project.m_root / "cg50" / "icon-sel.png"));
    CHECK(settings.iconFiles(Platform::Cg50).size() == 2);

    CHECK_FALSE(settings.setIcon(Platform::Web, source, &notes, &error));
    CHECK(error == "The Web has no icon.");
    CHECK_FALSE(settings.setIcon(Platform::Pc, (project.m_root / "project.fried").string(), &notes, &error));
    CHECK(error.find("could not be read as an image") != std::string::npos);
    CHECK_FALSE(fs::exists(project.m_root / "assets" / "icon.png"));
}

TEST_CASE("the PC is built for the system and for Windows, and every other platform for itself")
{
    using fried::core::BuildTarget;
    std::vector<BuildTarget> pc = ProjectBuild::targetsOf(Platform::Pc);
    CHECK(pc.front() == BuildTarget::Pc);
    CHECK(ProjectBuild::targetsOf(Platform::NintendoDs) == std::vector<BuildTarget>{BuildTarget::NintendoDs});
    CHECK(ProjectBuild::targetsOf(Platform::Web) == std::vector<BuildTarget>{BuildTarget::Web});
}

TEST_CASE("a target's build settings start at its own folder and are kept once set")
{
    using fried::core::BuildTarget;
    TemporaryProject project("build-settings");
    ProjectBuild build(project.m_project);
    build.useToolchains(linuxMachine());

    CHECK(build.settings(BuildTarget::Pc).m_directory == "build");
    CHECK(build.settings(BuildTarget::Vita).m_directory == "build/vita");
    CHECK(build.settings(BuildTarget::Pc).m_type == fried::core::BuildType::Debug);
    CHECK(build.status(BuildTarget::Pc) == "Ready");
    CHECK(build.commands(BuildTarget::Pc) == "haxe build.hxml -debug\ncmake -S . -B build -DCMAKE_BUILD_TYPE=Debug\ncmake --build build --config Debug --parallel");

    BuildSettings release;
    release.m_type = fried::core::BuildType::Release;
    release.m_directory = "out dir";
    release.m_logToFile = true;
    build.setSettings(BuildTarget::Pc, release);
    CHECK(build.settings(BuildTarget::Pc).m_directory == "out dir");
    CHECK(build.commands(BuildTarget::Pc) ==
          "haxe build.hxml -D fried_log_file\ncmake -S . -B \"out dir\" -DCMAKE_BUILD_TYPE=Release\ncmake --build \"out dir\" --config Release --parallel");

    release.m_directory.clear();
    build.setSettings(BuildTarget::Pc, release);
    CHECK_FALSE(build.isReady(BuildTarget::Pc));
    CHECK(build.status(BuildTarget::Pc) == "Ready");
    std::string error;
    CHECK_FALSE(build.start(BuildTarget::Pc, &error));
    CHECK(error == "There is no build directory to build in.");
}

TEST_CASE("the settings of a build are remembered for the project between runs")
{
    using fried::core::BuildTarget;
    TemporaryProject project("build-remembered");
    fs::path config = project.m_root / "config";
    setEnvironment("FRIED_EDITOR_CONFIG", config.string().c_str());
    {
        ProjectBuild build(project.m_project);
        BuildSettings settings;
        settings.m_type = fried::core::BuildType::Release;
        settings.m_directory = "build/shipping";
        build.setSettings(BuildTarget::NintendoDs, settings);
    }
    ProjectBuild again(project.m_project);
    CHECK(again.settings(BuildTarget::NintendoDs).m_type == fried::core::BuildType::Release);
    CHECK(again.settings(BuildTarget::NintendoDs).m_directory == "build/shipping");
    CHECK(again.settings(BuildTarget::Nintendo3ds).m_directory == "build/3ds");
    setEnvironment("FRIED_EDITOR_CONFIG", "none");
}

TEST_CASE("a build runs its steps in the project's folder and can be dismissed when it ends")
{
    using fried::core::BuildTarget;
    TemporaryProject project("build-run");
    ProjectBuild build(project.m_project);
    build.useToolchains(linuxMachine());

    // The build's first step is haxe, which fails here for want of a build.hxml, or cannot start at all where haxe is not installed.
    std::string error;
    REQUIRE(build.start(BuildTarget::Pc, &error));
    CHECK(build.isRunning());
    for (int attempt = 0; attempt < 2000 && build.pump() == ProjectBuild::State::Running; ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(build.state() == ProjectBuild::State::Failed);
    CHECK(build.runner().lines().front().m_text == "$ haxe build.hxml -debug");
    CHECK_FALSE(build.runner().failure().empty());

    build.dismiss();
    CHECK(build.state() == ProjectBuild::State::Idle);
    CHECK(build.runner().lines().empty());
}

TEST_CASE("the game a build made is the executable or package in its build folder")
{
    TemporaryProject project("built");
    ProjectBuild build(project.m_project);
    CHECK(build.builtFile().empty());

    std::filesystem::create_directories(project.m_root / "build" / "cpp");
    std::filesystem::create_directories(project.m_root / "build" / "CMakeFiles");
    project.write("build/cpp/Main.cpp", "");
    project.write("build/CMakeFiles/notes.exe", "");
    project.write("build/libengine.a", "");
    CHECK(build.builtFile().empty());

    project.write("build/sample", "binary");
    std::filesystem::permissions(project.m_root / "build" / "sample", std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
    CHECK(std::filesystem::path(build.builtFile()).filename() == "sample");
}
