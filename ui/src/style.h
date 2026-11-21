#pragma once

#include <imgui.h>

#include <string>

namespace editor_ui
{
    // The editor's colors: blue is the one accent, and the surfaces are neutral greys.
    namespace palette
    {
        inline const ImVec4 accent(0.173f, 0.365f, 0.529f, 1.00f);
        inline const ImVec4 accentHot(0.227f, 0.475f, 0.733f, 1.00f);
        inline const ImVec4 accentText(0.435f, 0.659f, 0.878f, 1.00f);
        inline const ImVec4 white(0.824f, 0.824f, 0.824f, 1.00f);
        inline const ImVec4 whiteDim(0.541f, 0.541f, 0.541f, 1.00f);
        inline const ImVec4 tomato(0.91f, 0.33f, 0.27f, 1.00f);
        inline const ImVec4 warning(0.89f, 0.68f, 0.24f, 1.00f);
        inline const ImVec4 leaf(0.36f, 0.72f, 0.36f, 1.00f);
        inline const ImVec4 base(0.098f, 0.098f, 0.098f, 1.00f);
        inline const ImVec4 panel(0.220f, 0.220f, 0.220f, 1.00f);
        inline const ImVec4 field(0.165f, 0.165f, 0.165f, 1.00f);
        inline const ImVec4 header(0.243f, 0.243f, 0.243f, 1.00f);
        inline const ImVec4 raised(0.290f, 0.290f, 0.290f, 1.00f);
        inline const ImVec4 raisedHot(0.337f, 0.337f, 0.337f, 1.00f);
        inline const ImVec4 line(0.137f, 0.137f, 0.137f, 1.00f);

        inline ImVec4 accentAt(float alpha)
        {
            return ImVec4(accent.x, accent.y, accent.z, alpha);
        }
    }

    // A dark theme, with the Inter font that is compiled into the editor.
    void applyStyle();

    // The heavier weight of the font, for the names that head a group.
    ImFont *boldFont();

    // For a window that is to be an operating system window of its own, such as the settings, and stay one instead of merging back into the editor's when they overlap.
    const ImGuiWindowClass *ownWindowClass();

    // Enlarges the text and every size of the style from the unscaled one, so it can be called again with another value.
    void setUiScale(float scale);
    float uiScale();

    // A size in pixels at the scale the interface is shown at.
    inline float scaled(float pixels)
    {
        return pixels * uiScale();
    }
}
