#include "style.h"

#include "icons.h"

#include <imgui.h>

namespace editor_ui
{
    extern const unsigned char s_Inter_Regular[];
    extern const unsigned int s_Inter_RegularSize;
    extern const unsigned char s_Inter_SemiBold[];
    extern const unsigned int s_Inter_SemiBoldSize;

    namespace
    {
        ImGuiStyle s_unscaled;
        float s_scale = 1.0f;
        ImFont *s_bold = nullptr;
    }

    const ImGuiWindowClass *ownWindowClass()
    {
        static ImGuiWindowClass s_class = [] {
            ImGuiWindowClass windowClass;
            windowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
            return windowClass;
        }();
        return &s_class;
    }

    void setUiScale(float scale)
    {
        s_scale = scale;
        ImGuiStyle &style = ImGui::GetStyle();
        style = s_unscaled;
        style.ScaleAllSizes(scale);
        style.FontScaleMain = scale;
        Icons::rebuild(scale);
    }

    float uiScale()
    {
        return s_scale;
    }

    ImFont *boldFont()
    {
        return s_bold;
    }

    void applyStyle()
    {
        ImGuiIO &io = ImGui::GetIO();
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char *>(s_Inter_Regular), (int)s_Inter_RegularSize, 14.0f, &config);
        s_bold = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char *>(s_Inter_SemiBold), (int)s_Inter_SemiBoldSize, 14.0f, &config);

        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding = 0;
        style.ChildRounding = 0;
        style.FrameRounding = 2;
        style.PopupRounding = 2;
        style.ScrollbarRounding = 0;
        style.GrabRounding = 2;
        style.TabRounding = 0;
        style.WindowPadding = ImVec2(10, 8);
        style.FramePadding = ImVec2(8, 5);
        style.ItemSpacing = ImVec2(8, 6);
        style.ItemInnerSpacing = ImVec2(6, 4);
        style.IndentSpacing = 16;
        style.ScrollbarSize = 12;
        style.WindowBorderSize = 0;
        style.FrameBorderSize = 0;
        style.TabBarBorderSize = 0;
        style.TabBarOverlineSize = 2;
        style.SeparatorTextBorderSize = 1;

        using namespace palette;
        const ImVec4 accent = yolk;
        const ImVec4 accentSoft = yolkSoft;
        const ImVec4 accentHot = yolkHot;

        ImVec4 *colors = style.Colors;
        colors[ImGuiCol_Text] = white;
        colors[ImGuiCol_TextDisabled] = whiteDim;
        colors[ImGuiCol_WindowBg] = panel;
        colors[ImGuiCol_ChildBg] = base;
        colors[ImGuiCol_PopupBg] = ImVec4(0.135f, 0.135f, 0.142f, 0.98f);
        colors[ImGuiCol_Border] = line;
        colors[ImGuiCol_FrameBg] = raised;
        colors[ImGuiCol_FrameBgHovered] = raisedHot;
        colors[ImGuiCol_FrameBgActive] = raisedHot;
        colors[ImGuiCol_TitleBg] = base;
        colors[ImGuiCol_TitleBgActive] = base;
        colors[ImGuiCol_TitleBgCollapsed] = base;
        colors[ImGuiCol_MenuBarBg] = base;
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_ScrollbarGrab] = raisedHot;
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.31f, 0.31f, 0.32f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = whiteDim;
        colors[ImGuiCol_CheckMark] = accent;
        colors[ImGuiCol_SliderGrab] = accent;
        colors[ImGuiCol_SliderGrabActive] = accentHot;
        colors[ImGuiCol_Button] = raised;
        colors[ImGuiCol_ButtonHovered] = raisedHot;
        colors[ImGuiCol_ButtonActive] = line;
        colors[ImGuiCol_Header] = ImVec4(0.150f, 0.150f, 0.158f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = raisedHot;
        colors[ImGuiCol_HeaderActive] = line;
        colors[ImGuiCol_Separator] = line;
        colors[ImGuiCol_SeparatorHovered] = whiteDim;
        colors[ImGuiCol_SeparatorActive] = white;
        colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_ResizeGripHovered] = whiteDim;
        colors[ImGuiCol_ResizeGripActive] = white;
        colors[ImGuiCol_Tab] = base;
        colors[ImGuiCol_TabHovered] = raised;
        colors[ImGuiCol_TabSelected] = panel;
        colors[ImGuiCol_TabSelectedOverline] = accent;
        colors[ImGuiCol_TabDimmed] = base;
        colors[ImGuiCol_TabDimmedSelected] = panel;
        colors[ImGuiCol_TabDimmedSelectedOverline] = accentSoft;
        colors[ImGuiCol_DockingPreview] = raisedHot;
        colors[ImGuiCol_DockingEmptyBg] = base;
        colors[ImGuiCol_TextSelectedBg] = accentSoft;
        colors[ImGuiCol_DragDropTarget] = accent;
        colors[ImGuiCol_NavCursor] = whiteDim;
        s_unscaled = style;
    }
}
