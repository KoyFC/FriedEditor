#include "style.h"

#include <imgui.h>

#include <filesystem>

namespace imgui_backend
{
    void applyStyle(const std::string &fontPath)
    {
        ImGuiIO &io = ImGui::GetIO();
        if (!fontPath.empty() && std::filesystem::exists(fontPath))
        {
            io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
        }
        else
        {
            io.Fonts->AddFontDefault();
        }

        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding = 0;
        style.ChildRounding = 4;
        style.FrameRounding = 4;
        style.PopupRounding = 6;
        style.ScrollbarRounding = 6;
        style.GrabRounding = 4;
        style.TabRounding = 4;
        style.WindowPadding = ImVec2(10, 8);
        style.FramePadding = ImVec2(8, 5);
        style.ItemSpacing = ImVec2(8, 6);
        style.ItemInnerSpacing = ImVec2(6, 4);
        style.IndentSpacing = 16;
        style.ScrollbarSize = 12;
        style.WindowBorderSize = 0;
        style.FrameBorderSize = 0;
        style.TabBarBorderSize = 0;
        style.SeparatorTextBorderSize = 1;

        const ImVec4 accent(1.00f, 0.56f, 0.18f, 1.00f);
        const ImVec4 accentSoft(1.00f, 0.56f, 0.18f, 0.35f);
        const ImVec4 accentHot(1.00f, 0.66f, 0.32f, 1.00f);
        const ImVec4 base(0.085f, 0.085f, 0.105f, 1.00f);
        const ImVec4 panel(0.115f, 0.115f, 0.140f, 1.00f);
        const ImVec4 raised(0.165f, 0.165f, 0.200f, 1.00f);
        const ImVec4 raisedHot(0.215f, 0.215f, 0.260f, 1.00f);

        ImVec4 *colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.93f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.56f, 1.00f);
        colors[ImGuiCol_WindowBg] = panel;
        colors[ImGuiCol_ChildBg] = base;
        colors[ImGuiCol_PopupBg] = ImVec4(0.13f, 0.13f, 0.16f, 0.98f);
        colors[ImGuiCol_Border] = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);
        colors[ImGuiCol_FrameBg] = raised;
        colors[ImGuiCol_FrameBgHovered] = raisedHot;
        colors[ImGuiCol_FrameBgActive] = accentSoft;
        colors[ImGuiCol_TitleBg] = base;
        colors[ImGuiCol_TitleBgActive] = base;
        colors[ImGuiCol_TitleBgCollapsed] = base;
        colors[ImGuiCol_MenuBarBg] = base;
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_ScrollbarGrab] = raisedHot;
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.36f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = accent;
        colors[ImGuiCol_CheckMark] = accent;
        colors[ImGuiCol_SliderGrab] = accent;
        colors[ImGuiCol_SliderGrabActive] = accentHot;
        colors[ImGuiCol_Button] = raised;
        colors[ImGuiCol_ButtonHovered] = raisedHot;
        colors[ImGuiCol_ButtonActive] = accentSoft;
        colors[ImGuiCol_Header] = ImVec4(1.00f, 0.56f, 0.18f, 0.22f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(1.00f, 0.56f, 0.18f, 0.35f);
        colors[ImGuiCol_HeaderActive] = ImVec4(1.00f, 0.56f, 0.18f, 0.50f);
        colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);
        colors[ImGuiCol_SeparatorHovered] = accent;
        colors[ImGuiCol_SeparatorActive] = accentHot;
        colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_ResizeGripHovered] = accentSoft;
        colors[ImGuiCol_ResizeGripActive] = accent;
        colors[ImGuiCol_Tab] = base;
        colors[ImGuiCol_TabHovered] = ImVec4(1.00f, 0.56f, 0.18f, 0.40f);
        colors[ImGuiCol_TabSelected] = panel;
        colors[ImGuiCol_TabSelectedOverline] = accent;
        colors[ImGuiCol_TabDimmed] = base;
        colors[ImGuiCol_TabDimmedSelected] = panel;
        colors[ImGuiCol_TabDimmedSelectedOverline] = accentSoft;
        colors[ImGuiCol_DockingPreview] = accentSoft;
        colors[ImGuiCol_DockingEmptyBg] = base;
        colors[ImGuiCol_TextSelectedBg] = accentSoft;
        colors[ImGuiCol_DragDropTarget] = accent;
        colors[ImGuiCol_NavCursor] = accent;
    }
}
