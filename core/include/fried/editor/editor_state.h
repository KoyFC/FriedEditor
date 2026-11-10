#pragma once

#include <string>

namespace fried::editor
{
    // What the editor remembers between runs, kept in the user's configuration folder, per project: the scene that was open.
    // FRIED_EDITOR_CONFIG names another folder, and "none" turns remembering off, which is what the tests use.
    class EditorState
    {
    public:
        static std::string lastScene(const std::string &projectDirectory);
        static void setLastScene(const std::string &projectDirectory, const std::string &sceneFile);

    private:
        static std::string filePath();
    };

    // The last lines of a text file, which is how a build's log is shown while it is still being written.
    std::string tailOfFile(const std::string &path, int lines);
}
