#include "fried/editor/asset_browser.h"

#include "fried/editor/import_settings.h"

#include <algorithm>
#include <set>
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
        if (endsWith(".hx"))
        {
            return AssetKind::Script;
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

    std::string assetFolderOf(const AssetEntry &entry)
    {
        std::size_t slash = entry.m_path.rfind('/');
        return slash == std::string::npos ? entry.m_root : entry.m_root + "/" + entry.m_path.substr(0, slash);
    }

    std::vector<std::string> assetFolders(const std::vector<AssetEntry> &entries)
    {
        std::set<std::string> folders = {"game", "engine", "src"};
        for (const AssetEntry &entry : entries)
        {
            std::string folder = assetFolderOf(entry);
            while (true)
            {
                folders.insert(folder);
                std::size_t slash = folder.rfind('/');
                if (slash == std::string::npos)
                {
                    break;
                }
                folder.resize(slash);
            }
        }
        return std::vector<std::string>(folders.begin(), folders.end());
    }

    std::vector<std::string> childFolders(const std::vector<std::string> &folders, const std::string &folder)
    {
        std::vector<std::string> children;
        for (const std::string &candidate : folders)
        {
            std::size_t slash = candidate.rfind('/');
            std::string parent = slash == std::string::npos ? std::string() : candidate.substr(0, slash);
            if (parent == folder)
            {
                children.push_back(candidate);
            }
        }
        return children;
    }

    bool isUnderFolder(const AssetEntry &entry, const std::string &folder)
    {
        if (folder.empty())
        {
            return true;
        }
        std::string own = assetFolderOf(entry);
        return own == folder || (own.size() > folder.size() && own.compare(0, folder.size(), folder) == 0 && own[folder.size()] == '/');
    }

    std::vector<AssetEntry> listAssets(const std::string &root, const std::string &directory)
    {
        std::vector<AssetEntry> entries;
        std::error_code ignored;
        for (auto entry = fs::recursive_directory_iterator(directory, ignored); entry != fs::recursive_directory_iterator(); entry.increment(ignored))
        {
            if (!entry->is_regular_file(ignored) || entry->path().filename().string().front() == '.' || isImportSidecar(entry->path().filename().string()))
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
