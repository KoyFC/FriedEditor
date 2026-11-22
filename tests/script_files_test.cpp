#include "doctest/doctest.h"

#include "fried/editor/script_files.h"

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace fried::editor;
namespace fs = std::filesystem;

namespace
{
    fs::path workspace()
    {
        fs::path directory = fs::temp_directory_path() / "fried-editor-script-files-test";
        fs::remove_all(directory);
        fs::create_directories(directory / "src" / "nodes");
        fs::create_directories(directory / "engine" / "fried" / "scene");
        fs::create_directories(directory / "scenes");
        std::ofstream(directory / "engine" / "fried" / "scene" / "Node2D.hx") << "package fried.scene;\nclass Node2D {}\n";
        return directory;
    }

    std::string read(const fs::path &file)
    {
        std::ifstream in(file, std::ios::binary);
        std::stringstream text;
        text << in.rdbuf();
        return text.str();
    }
}

TEST_CASE("a script is named like a class")
{
    CHECK(isScriptName("Player"));
    CHECK(isScriptName("Enemy_2"));
    CHECK_FALSE(isScriptName("player"));
    CHECK_FALSE(isScriptName("2Player"));
    CHECK_FALSE(isScriptName("My Node"));
    CHECK_FALSE(isScriptName(""));
}

TEST_CASE("a folder is a package")
{
    CHECK(packageOfFolder("nodes/enemies") == "nodes.enemies");
    CHECK(packageOfFolder("") == "");
}

TEST_CASE("a class is found by its file under the source roots")
{
    fs::path directory = workspace();
    std::ofstream(directory / "src" / "nodes" / "Player.hx") << "package nodes;\n";
    std::vector<std::string> roots = {(directory / "src").string(), (directory / "engine").string()};
    CHECK(findClassModule(roots, "Node2D") == "fried.scene.Node2D");
    CHECK(findClassModule(roots, "Player") == "nodes.Player");
    CHECK(findClassModule(roots, "Missing") == "");
}

TEST_CASE("a new script extends the class that was chosen and imports it")
{
    std::string text = scriptTemplate("nodes.enemies", "Slime", "fried.scene.Node2D");
    CHECK(text.find("package nodes.enemies;") == 0);
    CHECK(text.find("import fried.scene.Node2D;") != std::string::npos);
    CHECK(text.find("@:serializable\nclass Slime extends Node2D {") != std::string::npos);
    CHECK(text.find("override function update(delta:Float):Void {}") != std::string::npos);

    std::string sibling = scriptTemplate("nodes", "Slime", "nodes.Enemy");
    CHECK(sibling.find("import") == std::string::npos);
    CHECK(sibling.find("extends Enemy {") != std::string::npos);
    CHECK(scriptTemplate("nodes", "Plain", "").find("extends Node {") != std::string::npos);
}

TEST_CASE("creating a script writes the file and refuses to overwrite one")
{
    fs::path directory = workspace();
    std::string error;
    CHECK(createScript((directory / "src").string(), "nodes/enemies", "Slime", "fried.scene.Node2D", &error));
    CHECK(read(directory / "src" / "nodes" / "enemies" / "Slime.hx").find("class Slime extends Node2D") != std::string::npos);
    CHECK_FALSE(createScript((directory / "src").string(), "nodes/enemies", "Slime", "", &error));
    CHECK(error.find("already") != std::string::npos);
    CHECK_FALSE(createScript((directory / "src").string(), "nodes", "slime", "", &error));
}

TEST_CASE("moving a script fixes its package and renaming it fixes its class")
{
    fs::path directory = workspace();
    fs::path file = directory / "src" / "nodes" / "Slime.hx";
    std::ofstream(file) << "package nodes;\n\n@:serializable\nclass Slime extends Node {\n\tpublic function new() { super(); }\n}\n";
    CHECK(fixScript(file.string(), "nodes.enemies", "Slime", "Blob"));
    std::string text = read(file);
    CHECK(text.find("package nodes.enemies;") == 0);
    CHECK(text.find("class Blob extends Node") != std::string::npos);
    CHECK(text.find("Slime") == std::string::npos);

    std::ofstream(file) << "class Blob {}\n";
    CHECK(fixScript(file.string(), "nodes", "Blob", "Blob"));
    CHECK(read(file).find("package nodes;\n\nclass Blob") == 0);
}

TEST_CASE("renaming a script renames the nodes of that type in the scenes")
{
    fs::path directory = workspace();
    std::ofstream(directory / "scenes" / "main.scene.json") << R"({"nodes":[{"id":"a","type":"Slime","children":[{"id":"b","type":"Sprite"},{"id":"c","type":"Slime"}]}]})";
    CHECK(renameSceneType((directory / "scenes").string(), "Slime", "Blob") == 2);
    std::string text = read(directory / "scenes" / "main.scene.json");
    CHECK(text.find("Slime") == std::string::npos);
    CHECK(text.find("\"Sprite\"") != std::string::npos);
    CHECK(renameSceneType((directory / "scenes").string(), "Slime", "Blob") == 0);
}
