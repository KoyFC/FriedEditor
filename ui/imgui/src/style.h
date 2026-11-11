#pragma once

#include <imgui.h>

#include <string>

namespace imgui_backend
{
    // The editor's colors: the yolk is the one accent, the egg white is the text, and the surfaces are neutral greys.
    namespace palette
    {
        inline const ImVec4 yolk(1.00f, 0.78f, 0.24f, 1.00f);
        inline const ImVec4 yolkSoft(1.00f, 0.78f, 0.24f, 0.35f);
        inline const ImVec4 yolkHot(1.00f, 0.87f, 0.46f, 1.00f);
        inline const ImVec4 white(0.96f, 0.94f, 0.89f, 1.00f);
        inline const ImVec4 whiteDim(0.56f, 0.55f, 0.52f, 1.00f);
        inline const ImVec4 tomato(0.91f, 0.27f, 0.17f, 1.00f);
        inline const ImVec4 leaf(0.36f, 0.72f, 0.36f, 1.00f);
        inline const ImVec4 base(0.080f, 0.080f, 0.085f, 1.00f);
        inline const ImVec4 panel(0.115f, 0.115f, 0.120f, 1.00f);
        inline const ImVec4 raised(0.170f, 0.170f, 0.178f, 1.00f);
        inline const ImVec4 raisedHot(0.225f, 0.225f, 0.235f, 1.00f);
        inline const ImVec4 line(0.235f, 0.235f, 0.245f, 1.00f);

        inline ImVec4 yolkAt(float alpha)
        {
            return ImVec4(yolk.x, yolk.y, yolk.z, alpha);
        }
    }

    // A dark theme with the yolk accent, and the engine's own font when it can be found.
    void applyStyle(const std::string &fontPath);
}
