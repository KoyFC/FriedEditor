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
    std::string configDirectory()
    {
        auto variable = [](const char *name) {
            const char *value = std::getenv(name);
            return value && *value ? std::string(value) : std::string();
        };

#ifdef _WIN32
        std::string appData = variable("APPDATA");
        return appData.empty() ? std::string() : (fs::path(appData) / "fried-editor").string();
#else
        std::string config = variable("XDG_CONFIG_HOME");
        if (!config.empty())
        {
            return (fs::path(config) / "fried-editor").string();
        }
        std::string home = variable("HOME");
        return home.empty() ? std::string() : (fs::path(home) / ".config" / "fried-editor").string();
#endif
    }

    std::string EditorState::filePath()
    {
        const char *override = std::getenv("FRIED_EDITOR_CONFIG");
        if (override && std::string(override) == "none")
        {
            return std::string();
        }
        std::string directory = override && *override ? std::string(override) : configDirectory();
        if (directory.empty())
        {
            return std::string();
        }
        return (fs::path(directory) / "state.json").string();
    }

    std::string EditorState::lastScene(const std::string &projectDirectory)
    {
        Json scene = projectValue(projectDirectory, "lastScene");
        return scene.is_string() ? scene.get<std::string>() : std::string();
    }

    void EditorState::setLastScene(const std::string &projectDirectory, const std::string &sceneFile)
    {
        setProjectValue(projectDirectory, "lastScene", sceneFile);
    }

    float EditorState::uiScale()
    {
        std::string path = filePath();
        std::ifstream file(path);
        if (path.empty() || !file)
        {
            return 1.0f;
        }
        Json state = Json::parse(file, nullptr, false);
        if (!state.is_object() || !state.contains("uiScale") || !state["uiScale"].is_number())
        {
            return 1.0f;
        }
        float scale = state["uiScale"].get<float>();
        return scale >= 0.5f && scale <= 4.0f ? scale : 1.0f;
    }

    void EditorState::setUiScale(float value)
    {
        std::string path = filePath();
        if (path.empty())
        {
            return;
        }
        Json state = Json::object();
        {
            std::ifstream file(path);
            Json read = file ? Json::parse(file, nullptr, false) : Json();
            if (read.is_object())
            {
                state = read;
            }
        }
        state["uiScale"] = value;
        std::error_code ignored;
        fs::create_directories(fs::path(path).parent_path(), ignored);
        std::ofstream(path) << state.dump(2) << "\n";
    }

    std::string EditorState::layout()
    {
        std::string path = filePath();
        std::ifstream file(path);
        if (path.empty() || !file)
        {
            return "default";
        }
        Json state = Json::parse(file, nullptr, false);
        if (!state.is_object() || !state.contains("layout") || !state["layout"].is_string())
        {
            return "default";
        }
        return state["layout"].get<std::string>();
    }

    void EditorState::setLayout(const std::string &value)
    {
        std::string path = filePath();
        if (path.empty())
        {
            return;
        }
        Json state = Json::object();
        {
            std::ifstream file(path);
            Json read = file ? Json::parse(file, nullptr, false) : Json();
            if (read.is_object())
            {
                state = read;
            }
        }
        state["layout"] = value;
        std::error_code ignored;
        fs::create_directories(fs::path(path).parent_path(), ignored);
        std::ofstream(path) << state.dump(2) << "\n";
    }

    Json EditorState::projectValue(const std::string &projectDirectory, const std::string &key)
    {
        std::string path = filePath();
        if (path.empty())
        {
            return Json();
        }
        std::ifstream file(path);
        if (!file)
        {
            return Json();
        }
        Json state = Json::parse(file, nullptr, false);
        if (!state.is_object() || !state.contains("projects") || !state["projects"].is_object())
        {
            return Json();
        }
        auto project = state["projects"].find(projectDirectory);
        if (project == state["projects"].end() || !project->is_object() || !project->contains(key))
        {
            return Json();
        }
        return (*project)[key];
    }

    void EditorState::setProjectValue(const std::string &projectDirectory, const std::string &key, const Json &value)
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
        state["projects"][projectDirectory][key] = value;
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
