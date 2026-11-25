#include "doctest/doctest.h"

#include "fried/editor/asset_files.h"

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace fried::editor;
namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace
{
    fs::path project()
    {
        fs::path directory = fs::temp_directory_path() / "fried-editor-asset-files-test";
        fs::remove_all(directory);
        fs::create_directories(directory / "assets" / "scenes");
        fs::create_directories(directory / "assets" / "sprites");
        std::ofstream(directory / "assets" / "sprites" / "hero.png") << "x";
        std::ofstream(directory / "assets" / "icon.png") << "x";
        Json scene = {{"version", 2}, {"name", "Main"},
                      {"nodes", Json::array({{{"id", "a"}, {"type", "Sprite"}, {"texture", {{"root", "game"}, {"path", "sprites/hero.png"}}},
                                              {"children", Json::array({{{"id", "b"}, {"type", "Sprite"}, {"texture", {{"root", "engine"}, {"path", "sprites/hero.png"}}}}})}}})}};
        std::ofstream(directory / "assets" / "scenes" / "main.scene.json") << scene.dump(1, '\t') << "\n";
        return directory;
    }
}

TEST_CASE("a file name is one name, not a path")
{
    CHECK(isAssetName("hero.png"));
    CHECK_FALSE(isAssetName(""));
    CHECK_FALSE(isAssetName("a/b"));
    CHECK_FALSE(isAssetName(".hidden"));
    CHECK_FALSE(isAssetName(".."));
}

TEST_CASE("references to a file or to a folder are counted and rewritten, and the engine's are left alone")
{
    Json data = {{"a", {{"root", "game"}, {"path", "sprites/hero.png"}}},
                 {"b", Json::array({{{"root", "game"}, {"path", "sprites/boss/a.png"}}, {{"root", "engine"}, {"path", "sprites/hero.png"}}})}};
    CHECK(countAssetReferences(data, "sprites/hero.png") == 1);
    CHECK(countAssetReferences(data, "sprites") == 2);
    CHECK(countAssetReferences(data, "sprite") == 0);

    CHECK(rewriteAssetReferences(&data, "sprites", "art") == 2);
    CHECK(data["a"]["path"] == "art/hero.png");
    CHECK(data["b"][0]["path"] == "art/boss/a.png");
    CHECK(data["b"][1]["path"] == "sprites/hero.png");
}

TEST_CASE("a folder is made, and an empty one is listed")
{
    fs::path directory = project();
    std::string assets = (directory / "assets").string();
    std::string error;
    CHECK(createAssetFolder(assets, "audio/music", &error));
    CHECK_FALSE(createAssetFolder(assets, "audio/music", &error));
    CHECK_FALSE(createAssetFolder(assets, "audio/..", &error));
    auto folders = listAssetDirectories("game", assets);
    CHECK(std::find(folders.begin(), folders.end(), "game/audio/music") != folders.end());
    CHECK(std::find(folders.begin(), folders.end(), "game/sprites") != folders.end());
    fs::remove_all(directory);
}

TEST_CASE("moving a file moves it on disk and in the scenes that name it, and a folder takes what is in it")
{
    fs::path directory = project();
    std::string assets = (directory / "assets").string();
    std::string scenes = (directory / "assets" / "scenes").string();
    std::string error;
    int rewritten = 0;

    CHECK(scenesUsingAsset(scenes, "sprites/hero.png") == std::vector<std::string>{"main.scene.json"});
    REQUIRE(moveAsset(assets, scenes, "sprites/hero.png", "hero.png", &rewritten, &error));
    CHECK(rewritten == 1);
    CHECK(fs::exists(directory / "assets" / "hero.png"));
    CHECK_FALSE(fs::exists(directory / "assets" / "sprites" / "hero.png"));

    std::ifstream file(directory / "assets" / "scenes" / "main.scene.json");
    std::stringstream text;
    text << file.rdbuf();
    Json scene = Json::parse(text.str());
    CHECK(scene["nodes"][0]["texture"]["path"] == "hero.png");
    CHECK(scene["nodes"][0]["children"][0]["texture"]["path"] == "sprites/hero.png");

    REQUIRE(moveAsset(assets, scenes, "sprites", "art", &rewritten, &error));
    CHECK(fs::exists(directory / "assets" / "art"));

    CHECK_FALSE(moveAsset(assets, scenes, "hero.png", "icon.png", &rewritten, &error));
    CHECK_FALSE(moveAsset(assets, scenes, "art", "art/inner", &rewritten, &error));
    CHECK_FALSE(moveAsset(assets, scenes, "scenes/main.scene.json", "main.scene.json", &rewritten, &error));
    CHECK_FALSE(moveAsset(assets, scenes, "icon.png", "scenes/icon.png", &rewritten, &error));
    fs::remove_all(directory);
}

TEST_CASE("a file is copied and deleted, and the scenes are not deleted from here")
{
    fs::path directory = project();
    std::string assets = (directory / "assets").string();
    std::string error;
    CHECK(copyAsset(assets, "icon.png", "icon copy.png", &error));
    CHECK_FALSE(copyAsset(assets, "icon.png", "icon copy.png", &error));
#ifndef _WIN32
    fs::path trash = std::getenv("FRIED_EDITOR_TRASH");
    fs::remove_all(trash);
#endif
    CHECK(deleteAsset(assets, "icon copy.png", &error));
    CHECK_FALSE(fs::exists(directory / "assets" / "icon copy.png"));
#ifndef _WIN32
    CHECK(fs::exists(trash / "files" / "icon copy.png"));
    CHECK(fs::exists(trash / "info" / "icon copy.png.trashinfo"));
#endif
    CHECK_FALSE(deleteAsset(assets, "scenes/main.scene.json", &error));
    fs::remove_all(directory);
}
