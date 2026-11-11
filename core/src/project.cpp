#include "fried/editor/project.h"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace fried::editor
{
    namespace
    {
        using Json = nlohmann::json;

        std::string stringOf(const Json &object, const char *key)
        {
            auto found = object.find(key);
            return found != object.end() && found->is_string() ? found->get<std::string>() : std::string();
        }

        void overlay(const Json &object, DisplaySettings &display)
        {
            if (!object.is_object())
            {
                return;
            }
            for (const auto &[key, value] : object.items())
            {
                if (key == "mode" && value.is_string())
                {
                    display.m_mode = value.get<std::string>();
                }
                else if (key == "filter" && value.is_string())
                {
                    display.m_filter = value.get<std::string>();
                }
                else if (key == "width" && value.is_number_integer())
                {
                    display.m_width = value.get<int>();
                }
                else if (key == "height" && value.is_number_integer())
                {
                    display.m_height = value.get<int>();
                }
            }
        }
    }

    bool Project::load(const std::string &directory, std::string *error)
    {
        std::ifstream file(directory + "/" + s_fileName);
        if (!file)
        {
            *error = "There is no " + std::string(s_fileName) + " in " + directory + ".";
            return false;
        }
        std::stringstream text;
        text << file.rdbuf();
        return parse(directory, text.str(), error);
    }

    bool Project::parse(const std::string &directory, const std::string &text, std::string *error)
    {
        Json root = Json::parse(text, nullptr, false);
        if (!root.is_object())
        {
            *error = std::string(s_fileName) + " is not a JSON object.";
            return false;
        }

        m_directory = directory;
        m_name = stringOf(root, "name");
        m_organization = stringOf(root, "organization");
        m_version = stringOf(root, "version");

        for (std::size_t index = 0; index < s_platforms.size(); ++index)
        {
            DisplaySettings display;
            if (root.contains("display"))
            {
                overlay(root["display"], display);
            }
            auto section = root.find(platformKey(s_platforms[index]));
            if (section != root.end() && section->is_object() && section->contains("display"))
            {
                overlay((*section)["display"], display);
            }
            m_displays[index] = display;
        }
        return true;
    }

    DisplaySettings Project::display(Platform platform) const
    {
        for (std::size_t index = 0; index < s_platforms.size(); ++index)
        {
            if (s_platforms[index] == platform)
            {
                return m_displays[index];
            }
        }
        return DisplaySettings();
    }

    namespace
    {
        std::pair<int, int> nativeScreen(Platform platform)
        {
            switch (platform)
            {
            case Platform::Vita:
                return {960, 544};
            case Platform::Switch:
                return {1280, 720};
            case Platform::Nintendo3ds:
                return {400, 240};
            case Platform::NintendoDs:
                return {256, 192};
            case Platform::Cg50:
                return {396, 224};
            default:
                return {0, 0};
            }
        }
    }

    std::pair<int, int> Project::logicalSize(Platform platform) const
    {
        DisplaySettings settings = display(platform);
        std::pair<int, int> screen = nativeScreen(platform);
        bool hasDesign = settings.m_width > 0 && settings.m_height > 0;
        if (screen.first == 0)
        {
            return hasDesign ? std::make_pair(settings.m_width, settings.m_height) : std::make_pair(1280, 720);
        }
        if (settings.m_mode == "default" || !hasDesign)
        {
            return screen;
        }
        if (settings.m_mode == "expand")
        {
            double fit = std::min((double)screen.first / settings.m_width, (double)screen.second / settings.m_height);
            return {std::max(1, (int)std::lround(screen.first / fit)), std::max(1, (int)std::lround(screen.second / fit))};
        }
        if (settings.m_mode == "pixel")
        {
            double fit = std::min((double)screen.first / settings.m_width, (double)screen.second / settings.m_height);
            int scale = std::max(1, (int)std::floor(fit));
            return {std::max(1, screen.first / scale), std::max(1, screen.second / scale)};
        }
        return {settings.m_width, settings.m_height};
    }
}
