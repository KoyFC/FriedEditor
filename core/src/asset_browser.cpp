#include "fried/editor/asset_browser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fried::editor
{
    namespace fs = std::filesystem;

    AssetKind assetKindOf(const std::string &path)
    {
        std::string lower = path;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        auto endsWith = [&](const char *suffix) {
            std::string tail = suffix;
            return lower.size() >= tail.size() && lower.compare(lower.size() - tail.size(), tail.size(), tail) == 0;
        };
        if (endsWith(".scene.json"))
        {
            return AssetKind::Scene;
        }
        if (endsWith(".png") || endsWith(".jpg") || endsWith(".jpeg"))
        {
            return AssetKind::Image;
        }
        if (endsWith(".ttf"))
        {
            return AssetKind::Font;
        }
        if (endsWith(".wav") || endsWith(".ogg") || endsWith(".mp3"))
        {
            return AssetKind::Audio;
        }
        return AssetKind::Other;
    }

    std::vector<AssetEntry> listAssets(const std::string &root, const std::string &directory)
    {
        std::vector<AssetEntry> entries;
        std::error_code ignored;
        for (auto entry = fs::recursive_directory_iterator(directory, ignored); entry != fs::recursive_directory_iterator(); entry.increment(ignored))
        {
            if (!entry->is_regular_file(ignored) || entry->path().filename().string().front() == '.')
            {
                continue;
            }
            std::string path = fs::relative(entry->path(), directory, ignored).generic_string();
            entries.push_back({root, path, assetKindOf(path)});
        }
        std::sort(entries.begin(), entries.end(), [](const AssetEntry &a, const AssetEntry &b) { return a.m_path < b.m_path; });
        return entries;
    }

    bool parseAssetReference(const std::string &reference, std::string *root, std::string *path)
    {
        std::size_t slash = reference.find('/');
        if (slash == std::string::npos || slash + 1 >= reference.size())
        {
            return false;
        }
        std::string name = reference.substr(0, slash);
        if (name != "game" && name != "engine")
        {
            return false;
        }
        *root = name;
        *path = reference.substr(slash + 1);
        return true;
    }
}

namespace fried::editor
{
    std::string assetReferenceText(const nlohmann::json &reference, bool isFont)
    {
        if (!reference.is_object())
        {
            return std::string();
        }
        std::string text = reference.value("root", "") + "/" + reference.value("path", "");
        return isFont ? text + "@" + std::to_string(reference.value("size", 16)) : text;
    }

    std::optional<nlohmann::json> assetReferenceFromText(const std::string &text, bool isFont)
    {
        std::size_t first = text.find_first_not_of(" \t");
        if (first == std::string::npos)
        {
            return nlohmann::json();
        }
        std::string trimmed = text.substr(first, text.find_last_not_of(" \t") - first + 1);

        int size = 16;
        std::size_t at = trimmed.rfind('@');
        if (isFont && at != std::string::npos)
        {
            try
            {
                size = std::stoi(trimmed.substr(at + 1));
                trimmed = trimmed.substr(0, at);
            }
            catch (const std::exception &)
            {
            }
        }

        std::string root, path;
        if (!parseAssetReference(trimmed, &root, &path))
        {
            return std::nullopt;
        }
        nlohmann::json reference = {{"root", root}, {"path", path}};
        if (isFont)
        {
            reference["size"] = size;
        }
        return reference;
    }
}
