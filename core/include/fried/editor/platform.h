#pragma once

#include <array>
#include <string>

namespace fried::editor
{
    enum class Platform
    {
        Pc,
        Vita,
        Switch,
        Nintendo3ds,
        NintendoDs,
        Cg50,
        Web
    };

    inline constexpr std::array<Platform, 7> s_platforms = {
        Platform::Pc, Platform::Vita, Platform::Switch, Platform::Nintendo3ds, Platform::NintendoDs, Platform::Cg50, Platform::Web};

    // The name the engine gives the platform in project.fried and in scene files.
    inline std::string platformKey(Platform platform)
    {
        switch (platform)
        {
        case Platform::Vita:
            return "vita";
        case Platform::Switch:
            return "switch";
        case Platform::Nintendo3ds:
            return "3ds";
        case Platform::NintendoDs:
            return "nds";
        case Platform::Cg50:
            return "cg50";
        case Platform::Web:
            return "web";
        default:
            return "pc";
        }
    }

    inline std::string platformLabel(Platform platform)
    {
        switch (platform)
        {
        case Platform::Vita:
            return "PlayStation Vita";
        case Platform::Switch:
            return "Nintendo Switch";
        case Platform::Nintendo3ds:
            return "Nintendo 3DS";
        case Platform::NintendoDs:
            return "Nintendo DS";
        case Platform::Cg50:
            return "Casio fx-CG50";
        case Platform::Web:
            return "Web";
        default:
            return "PC";
        }
    }
}
