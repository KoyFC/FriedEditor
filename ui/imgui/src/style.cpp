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
        colors[ImGuiCol_Header] = raised;
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
        colors[ImGuiCol_TabSelectedOverline] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_TabDimmed] = base;
        colors[ImGuiCol_TabDimmedSelected] = panel;
        colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_DockingPreview] = raisedHot;
        colors[ImGuiCol_DockingEmptyBg] = base;
        colors[ImGuiCol_TextSelectedBg] = accentSoft;
        colors[ImGuiCol_DragDropTarget] = accent;
        colors[ImGuiCol_NavCursor] = whiteDim;
    }
}
