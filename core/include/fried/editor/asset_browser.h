#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace fried::editor
{
    enum class AssetKind
    {
        Image,
        Font,
        Audio,
        Model,
        Scene,
        Script,
        Other
    };

    // A file under one of the roots the editor lists: the game's assets and the engine's, which the engine reads, and the game's source, "src", where its scripts are.
    struct AssetEntry
    {
        // "game", "engine" or "src". A scene file names the first two this way.
        std::string m_root;

        // Relative to the root, with forward slashes.
        std::string m_path;
        AssetKind m_kind = AssetKind::Other;

        // How the file is written in the inspector and dragged between panels: "game/sprite.png".
        std::string reference() const { return m_root + "/" + m_path; }
    };

    AssetKind assetKindOf(const std::string &path);

    // Every file under a root's directory, sorted by path. A missing directory has none.
    std::vector<AssetEntry> listAssets(const std::string &root, const std::string &directory);

    // The folder an asset is in, written like a reference without the file: "game/sprites", or the root alone for a file at its top.
    std::string assetFolderOf(const AssetEntry &entry);

    // Every folder that holds an asset and the ones above it, sorted, and the roots whether or not they hold any, so that they can be entered.
    std::vector<std::string> assetFolders(const std::vector<AssetEntry> &entries);

    // The folders directly inside one, which for the empty folder are the roots.
    std::vector<std::string> childFolders(const std::vector<std::string> &folders, const std::string &folder);

    // Whether the asset is in the folder or in one below it. Every asset is in the empty folder.
    bool isUnderFolder(const AssetEntry &entry, const std::string &folder);

    // An asset reference as the inspector writes it: "game/sprite.png", and for a font "engine/NunitoSans.ttf@16". Null reads as nothing.
    std::string assetReferenceText(const nlohmann::json &reference, bool isFont);

    // The reverse. Empty text is the null reference, and text that is not a reference is nothing at all.
    std::optional<nlohmann::json> assetReferenceFromText(const std::string &text, bool isFont);

    // Reads "game/sprite.png" back into its root and path, and says whether it was one.
    bool parseAssetReference(const std::string &reference, std::string *root, std::string *path);
}
