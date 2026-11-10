#include "doctest/doctest.h"

#include "fried/editor/asset_browser.h"

#include <filesystem>
#include <fstream>

using namespace fried::editor;

TEST_CASE("files are told apart by their extension")
{
    CHECK(assetKindOf("sprites/Hero.PNG") == AssetKind::Image);
    CHECK(assetKindOf("a.jpeg") == AssetKind::Image);
    CHECK(assetKindOf("NunitoSans.ttf") == AssetKind::Font);
    CHECK(assetKindOf("music.ogg") == AssetKind::Audio);
    CHECK(assetKindOf("scenes/main.scene.json") == AssetKind::Scene);
    CHECK(assetKindOf("notes.txt") == AssetKind::Other);
}

TEST_CASE("a root is listed with relative paths, sorted, without hidden files")
{
    namespace fs = std::filesystem;
    fs::path directory = fs::temp_directory_path() / "fried-editor-assets-test";
    fs::remove_all(directory);
    fs::create_directories(directory / "sprites");
    std::ofstream(directory / "sprites" / "b.png") << "x";
    std::ofstream(directory / "a.wav") << "x";
    std::ofstream(directory / ".gitkeep") << "";

    auto entries = listAssets("game", directory.string());
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].m_path == "a.wav");
    CHECK(entries[1].reference() == "game/sprites/b.png");
    CHECK(entries[1].m_kind == AssetKind::Image);

    CHECK(listAssets("game", (directory / "missing").string()).empty());
    fs::remove_all(directory);
}

TEST_CASE("a reference is read back into a root and a path, and anything else is refused")
{
    std::string root, path;
    CHECK(parseAssetReference("engine/fonts/a.ttf", &root, &path));
    CHECK(root == "engine");
    CHECK(path == "fonts/a.ttf");
    CHECK_FALSE(parseAssetReference("other/a.png", &root, &path));
    CHECK_FALSE(parseAssetReference("game/", &root, &path));
    CHECK_FALSE(parseAssetReference("a.png", &root, &path));
}
