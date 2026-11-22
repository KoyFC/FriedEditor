#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace fried::editor
{
    // A name for one file or folder: not empty, no folder separators, not hidden and not a way up.
    bool isAssetName(const std::string &name);

    // Every folder below a root's directory, empty ones included, written like a reference without the file: "game/sprites".
    std::vector<std::string> listAssetDirectories(const std::string &root, const std::string &directory);

    // The references a scene's data makes to a file of the game, or to any file under a folder when the path is one. A reference is the object {"root": "game", "path": "sprite.png"}.
    int countAssetReferences(const nlohmann::json &value, const std::string &path);

    // Makes the references to a file, or to the files under a folder, name the new place instead. Returns how many were changed.
    int rewriteAssetReferences(nlohmann::json *value, const std::string &from, const std::string &to);

    // The scene files of a folder that refer to the asset, by file name.
    std::vector<std::string> scenesUsingAsset(const std::string &scenesDirectory, const std::string &path);

    // Scenes keep their files in the scenes folder, where the game finds them by name, so the asset operations leave them alone.
    bool isSceneAssetPath(const std::string &path);

    // Each of these takes paths relative to the game's assets directory and says what went wrong in error.
    bool createAssetFolder(const std::string &assetsDirectory, const std::string &path, std::string *error);
    bool copyAsset(const std::string &assetsDirectory, const std::string &from, const std::string &to, std::string *error);
    bool deleteAsset(const std::string &assetsDirectory, const std::string &path, std::string *error);

    // Moves or renames a file or a folder, and rewrites the references to it in the scene files of the project. The scene files are rewritten in the format the editor writes them in.
    bool moveAsset(const std::string &assetsDirectory, const std::string &scenesDirectory, const std::string &from, const std::string &to, int *rewritten, std::string *error);
}
