#include "fried/editor/editor_state.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace fried::editor
{
    std::string EditorState::filePath()
    {
        const char *override = std::getenv("FRIED_EDITOR_CONFIG");
        if (override && std::string(override) == "none")
        {
            return std::string();
        }
        fs::path directory;
        if (override && *override)
        {
            directory = override;
        }
        else if (const char *config = std::getenv("XDG_CONFIG_HOME"); config && *config)
        {
            directory = fs::path(config) / "fried-editor";
        }
        else if (const char *home = std::getenv("HOME"); home && *home)
        {
            directory = fs::path(home) / ".config" / "fried-editor";
        }
        else
        {
            return std::string();
        }
        return (directory / "state.json").string();
    }

    std::string EditorState::lastScene(const std::string &projectDirectory)
    {
        std::string path = filePath();
        if (path.empty())
        {
            return std::string();
        }
        std::ifstream file(path);
        if (!file)
        {
            return std::string();
        }
        Json state = Json::parse(file, nullptr, false);
        if (!state.is_object() || !state.contains("projects") || !state["projects"].is_object())
        {
            return std::string();
        }
        auto project = state["projects"].find(projectDirectory);
        if (project == state["projects"].end() || !project->is_object())
        {
            return std::string();
        }
        return project->value("lastScene", std::string());
    }

    void EditorState::setLastScene(const std::string &projectDirectory, const std::string &sceneFile)
    {
        std::string path = filePath();
        if (path.empty())
        {
            return;
        }
        Json state = Json::object();
        {
            std::ifstream file(path);
            if (file)
            {
                Json read = Json::parse(file, nullptr, false);
                if (read.is_object())
                {
                    state = read;
                }
            }
        }
        state["projects"][projectDirectory]["lastScene"] = sceneFile;
        std::error_code ignored;
        fs::create_directories(fs::path(path).parent_path(), ignored);
        std::ofstream(path) << state.dump(2) << "\n";
    }

    std::string tailOfFile(const std::string &path, int lines)
    {
        std::ifstream file(path);
        if (!file)
        {
            return std::string();
        }
        std::deque<std::string> last;
        std::string line;
        while (std::getline(file, line))
        {
            last.push_back(line);
            if ((int)last.size() > lines)
            {
                last.pop_front();
            }
        }
        std::ostringstream text;
        for (const std::string &kept : last)
        {
            text << kept << "\n";
        }
        return text.str();
    }
}
