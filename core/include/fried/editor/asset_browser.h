#pragma once

#include <string>
#include <vector>

namespace fried::editor
{
    enum class AssetKind
    {
        Image,
        Font,
        Audio,
        Scene,
        Other
    };

    // A file under one of the two asset roots the engine reads.
    struct AssetEntry
    {
        // "game" or "engine", which is how a scene file names the root.
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

    // Reads "game/sprite.png" back into its root and path, and says whether it was one.
    bool parseAssetReference(const std::string &reference, std::string *root, std::string *path);
}
