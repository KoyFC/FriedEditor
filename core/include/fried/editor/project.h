#pragma once

#include "fried/editor/platform.h"

#include <array>
#include <string>
#include <utility>

namespace fried::editor
{
    // The size is only meaningful when the mode is not "default", where the game takes its window's size.
    struct DisplaySettings
    {
        std::string m_mode = "default";
        int m_width = 0;
        int m_height = 0;
        std::string m_filter = "nearest";
    };

    // What the editor reads of a project's project.fried.
    class Project
    {
    public:
        static constexpr const char *s_fileName = "project.fried";

        // Returns false and says why in error when the directory has no readable project file.
        bool load(const std::string &directory, std::string *error);
        bool parse(const std::string &directory, const std::string &text, std::string *error);

        const std::string &directory() const { return m_directory; }
        std::string assetsDirectory() const { return m_directory + "/assets"; }
        std::string scenesDirectory() const { return m_directory + "/assets/scenes"; }

        const std::string &name() const { return m_name; }
        const std::string &organization() const { return m_organization; }
        const std::string &version() const { return m_version; }

        // The top-level "display", which every platform shares.
        const DisplaySettings &display() const { return m_display; }

        // The size the game draws in on the platform's own screen, which is what the camera shows: the screen itself in `default`, the design size in `fit`, `integer` and `stretch`, and the design size widened to cover the screen in `expand`.
        // A PC or web window has no fixed screen, so it opens at the design size.
        std::pair<int, int> logicalSize(Platform platform) const;

    private:
        std::string m_directory;
        std::string m_name;
        std::string m_organization;
        std::string m_version;
        DisplaySettings m_display;
    };
}
