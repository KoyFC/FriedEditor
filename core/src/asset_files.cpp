#include "fried/editor/asset_files.h"

#include "fried/editor/import_settings.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace fried::editor
{
    namespace
    {
        bool isUnder(const std::string &path, const std::string &folder)
        {
            return path == folder || (path.size() > folder.size() && path.compare(0, folder.size(), folder) == 0 && path[folder.size()] == '/');
        }

        std::string replacePrefix(const std::string &path, const std::string &from, const std::string &to)
        {
            return to + path.substr(from.size());
        }

        bool isReference(const Json &value)
        {
            return value.is_object() && value.value("root", "") == "game" && value.contains("path") && value["path"].is_string();
        }

        std::vector<fs::path> sceneFiles(const std::string &scenesDirectory)
        {
            std::vector<fs::path> files;
            std::error_code ignored;
            for (const auto &entry : fs::directory_iterator(scenesDirectory, ignored))
            {
                std::string name = entry.path().filename().string();
                if (entry.is_regular_file(ignored) && name.size() > 11 && name.compare(name.size() - 11, 11, ".scene.json") == 0)
                {
                    files.push_back(entry.path());
                }
            }
            std::sort(files.begin(), files.end());
            return files;
        }

        std::string readFile(const fs::path &path)
        {
            std::ifstream file(path, std::ios::binary);
            std::stringstream text;
            text << file.rdbuf();
            return text.str();
        }
    }

    bool isAssetName(const std::string &name)
    {
        return !name.empty() && name.find_first_of("/\\") == std::string::npos && name.front() != '.' && name.find_first_of(std::string("\0", 1)) == std::string::npos;
    }

    std::vector<std::string> listAssetDirectories(const std::string &root, const std::string &directory)
    {
        std::vector<std::string> folders;
        std::error_code ignored;
        for (auto entry = fs::recursive_directory_iterator(directory, ignored); entry != fs::recursive_directory_iterator(); entry.increment(ignored))
        {
            if (entry->is_directory(ignored) && entry->path().filename().string().front() != '.')
            {
                folders.push_back(root + "/" + fs::relative(entry->path(), directory, ignored).generic_string());
            }
        }
        std::sort(folders.begin(), folders.end());
        return folders;
    }

    int countAssetReferences(const Json &value, const std::string &path)
    {
        int count = 0;
        if (isReference(value) && isUnder(value["path"].get<std::string>(), path))
        {
            ++count;
        }
        if (value.is_object() || value.is_array())
        {
            for (const Json &child : value)
            {
                count += countAssetReferences(child, path);
            }
        }
        return count;
    }

    int rewriteAssetReferences(Json *value, const std::string &from, const std::string &to)
    {
        int count = 0;
        if (isReference(*value) && isUnder((*value)["path"].get<std::string>(), from))
        {
            (*value)["path"] = replacePrefix((*value)["path"].get<std::string>(), from, to);
            ++count;
        }
        if (value->is_object() || value->is_array())
        {
            for (Json &child : *value)
            {
                count += rewriteAssetReferences(&child, from, to);
            }
        }
        return count;
    }

    std::vector<std::string> scenesUsingAsset(const std::string &scenesDirectory, const std::string &path)
    {
        std::vector<std::string> names;
        for (const fs::path &file : sceneFiles(scenesDirectory))
        {
            Json data = Json::parse(readFile(file), nullptr, false);
            if (!data.is_discarded() && countAssetReferences(data, path) > 0)
            {
                names.push_back(file.filename().string());
            }
        }
        return names;
    }

    bool isSceneAssetPath(const std::string &path)
    {
        return isUnder(path, "scenes");
    }

    bool createAssetFolder(const std::string &assetsDirectory, const std::string &path, std::string *error)
    {
        std::string name = fs::path(path).filename().string();
        if (!isAssetName(name))
        {
            *error = "A folder needs a name that is a file name, with no folders in it.";
            return false;
        }
        std::error_code failure;
        fs::path target = fs::path(assetsDirectory) / path;
        if (fs::exists(target, failure))
        {
            *error = "There is already a file or a folder named " + name + " there.";
            return false;
        }
        if (!fs::create_directories(target, failure))
        {
            *error = "The folder could not be created: " + failure.message();
            return false;
        }
        return true;
    }

    bool copyAsset(const std::string &assetsDirectory, const std::string &from, const std::string &to, std::string *error)
    {
        std::error_code failure;
        fs::path target = fs::path(assetsDirectory) / to;
        if (fs::exists(target, failure))
        {
            *error = "There is already a file named " + target.filename().string() + " there.";
            return false;
        }
        fs::path source = fs::path(assetsDirectory) / from;
        fs::copy(source, target, fs::copy_options::recursive, failure);
        if (failure)
        {
            *error = "The copy failed: " + failure.message();
            return false;
        }
        // A folder takes its files' settings with it, and a file has to be given them.
        if (fs::is_regular_file(source, failure) && fs::exists(importSidecarPath(source.string()), failure))
        {
            fs::copy_file(importSidecarPath(source.string()), importSidecarPath(target.string()), fs::copy_options::overwrite_existing, failure);
        }
        return true;
    }

    bool deleteAsset(const std::string &assetsDirectory, const std::string &path, std::string *error)
    {
        if (path.empty() || isSceneAssetPath(path))
        {
            *error = "The scenes are not deleted from here.";
            return false;
        }
        std::error_code failure;
        fs::remove(importSidecarPath((fs::path(assetsDirectory) / path).string()), failure);
        fs::remove_all(fs::path(assetsDirectory) / path, failure);
        if (failure)
        {
            *error = "The delete failed: " + failure.message();
            return false;
        }
        return true;
    }

    bool moveAsset(const std::string &assetsDirectory, const std::string &scenesDirectory, const std::string &from, const std::string &to, int *rewritten, std::string *error)
    {
        *rewritten = 0;
        if (from.empty() || isSceneAssetPath(from) || isSceneAssetPath(to))
        {
            *error = "Scenes keep their place in the scenes folder, where the game finds them by name.";
            return false;
        }
        if (!isAssetName(fs::path(to).filename().string()))
        {
            *error = "A name is a file name, with no folders in it.";
            return false;
        }
        if (isUnder(to, from))
        {
            *error = "A folder cannot go inside itself.";
            return false;
        }
        std::error_code failure;
        fs::path source = fs::path(assetsDirectory) / from;
        fs::path target = fs::path(assetsDirectory) / to;
        if (!fs::exists(source, failure))
        {
            *error = "The file is not there any more.";
            return false;
        }
        if (fs::exists(target, failure))
        {
            *error = "There is already a file or a folder named " + target.filename().string() + " there.";
            return false;
        }
        fs::create_directories(target.parent_path(), failure);
        fs::rename(source, target, failure);
        if (failure)
        {
            *error = "The move failed: " + failure.message();
            return false;
        }
        // The settings of a file stay with it.
        if (fs::exists(importSidecarPath(source.string()), failure))
        {
            fs::rename(importSidecarPath(source.string()), importSidecarPath(target.string()), failure);
        }
        for (const fs::path &file : sceneFiles(scenesDirectory))
        {
            Json data = Json::parse(readFile(file), nullptr, false);
            if (data.is_discarded())
            {
                continue;
            }
            int count = rewriteAssetReferences(&data, from, to);
            if (count > 0)
            {
                std::ofstream out(file, std::ios::binary | std::ios::trunc);
                out << data.dump(1, '\t') << "\n";
                *rewritten += count;
            }
        }
        return true;
    }
}
