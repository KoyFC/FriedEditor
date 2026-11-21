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
        style.FrameRounding = 3;
        style.PopupRounding = 3;
        style.ScrollbarRounding = 0;
        style.GrabRounding = 2;
        style.TabRounding = 0;
        style.WindowPadding = ImVec2(6, 6);
        style.FramePadding = ImVec2(6, 3);
        style.ItemSpacing = ImVec2(6, 4);
        style.ItemInnerSpacing = ImVec2(4, 4);
        style.IndentSpacing = 14;
        style.ScrollbarSize = 10;
        style.WindowBorderSize = 1;
        style.PopupBorderSize = 1;
        style.FrameBorderSize = 1;
        style.TabCloseButtonMinWidthSelected = 0;
        style.TabCloseButtonMinWidthUnselected = 0;
        style.TabBarBorderSize = 0;
        style.TabBarOverlineSize = 2;
        style.SeparatorTextBorderSize = 1;

        using namespace palette;
        const ImVec4 accentSoft = accentAt(0.55f);

        ImVec4 *colors = style.Colors;
        colors[ImGuiCol_Text] = white;
        colors[ImGuiCol_TextDisabled] = whiteDim;
        colors[ImGuiCol_WindowBg] = panel;
        colors[ImGuiCol_ChildBg] = base;
        colors[ImGuiCol_PopupBg] = ImVec4(0.165f, 0.165f, 0.165f, 0.99f);
        colors[ImGuiCol_Border] = line;
        colors[ImGuiCol_FrameBg] = field;
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_TitleBg] = base;
        colors[ImGuiCol_TitleBgActive] = base;
        colors[ImGuiCol_TitleBgCollapsed] = base;
        colors[ImGuiCol_MenuBarBg] = base;
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_ScrollbarGrab] = raisedHot;
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = whiteDim;
        colors[ImGuiCol_CheckMark] = accentHot;
        colors[ImGuiCol_SliderGrab] = accent;
        colors[ImGuiCol_SliderGrabActive] = accentHot;
        colors[ImGuiCol_Button] = raised;
        colors[ImGuiCol_ButtonHovered] = raisedHot;
        colors[ImGuiCol_ButtonActive] = line;
        colors[ImGuiCol_Header] = header;
        colors[ImGuiCol_HeaderHovered] = raisedHot;
        colors[ImGuiCol_HeaderActive] = line;
        colors[ImGuiCol_Separator] = line;
        colors[ImGuiCol_SeparatorHovered] = whiteDim;
        colors[ImGuiCol_SeparatorActive] = white;
        colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_ResizeGripHovered] = whiteDim;
        colors[ImGuiCol_ResizeGripActive] = white;
        colors[ImGuiCol_Tab] = base;
        colors[ImGuiCol_Border] = line;
        colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_TabHovered] = raised;
        colors[ImGuiCol_TabSelected] = panel;
        colors[ImGuiCol_TabSelectedOverline] = accentHot;
        colors[ImGuiCol_TabDimmed] = base;
        colors[ImGuiCol_TabDimmedSelected] = panel;
        colors[ImGuiCol_TabDimmedSelectedOverline] = accent;
        colors[ImGuiCol_DockingPreview] = raisedHot;
        colors[ImGuiCol_DockingEmptyBg] = base;
        colors[ImGuiCol_TextSelectedBg] = accentSoft;
        colors[ImGuiCol_DragDropTarget] = accentHot;
        colors[ImGuiCol_NavCursor] = whiteDim;
        s_unscaled = style;
    }
}
