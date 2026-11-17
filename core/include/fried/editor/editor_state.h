#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace fried::editor
{
    // What the editor remembers between runs, kept in the user's configuration folder, per project: the scene that was open and how each platform is built.
    // FRIED_EDITOR_CONFIG names another folder, and "none" turns remembering off, which is what the tests use.
    class EditorState
    {
    public:
        static std::string lastScene(const std::string &projectDirectory);
        static void setLastScene(const std::string &projectDirectory, const std::string &sceneFile);

        // One remembered value of a project by its name, null when there is none.
        static nlohmann::json projectValue(const std::string &projectDirectory, const std::string &key);
        static void setProjectValue(const std::string &projectDirectory, const std::string &key, const nlohmann::json &value);

        // How much the interface is enlarged, 1 when nothing was chosen. It belongs to the user rather than to a project.
        static float uiScale();
        static void setUiScale(float value);

    private:
        static std::string filePath();
    };

    // The last lines of a text file, which is how a build's log is shown while it is still being written.
    std::string tailOfFile(const std::string &path, int lines);
}
