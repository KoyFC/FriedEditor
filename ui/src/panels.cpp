#include "panels.h"
#include "fried/core/scene_files.h"

#include "fried/editor/script_files.h"

#include "shortcuts.h"
#include "style.h"

#include "fried/editor/display_name.h"
#include "fried/editor/editor_state.h"
#include "fried/editor/fuzzy.h"
#include "fried/editor/import_settings.h"

#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>

using fried::editor::AssetKind;
using fried::editor::DrawCommand;
using fried::editor::Editor;
using fried::editor::FieldView;
using fried::editor::GizmoMode;
using Json = nlohmann::json;

namespace
{
    bool containsIgnoringCase(const std::string &text, const std::string &part)
    {
        auto it = std::search(text.begin(), text.end(), part.begin(), part.end(), [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
        return it != text.end();
    }

    bool isOpenScene(const std::string &path, const std::string &openPath)
    {
        std::error_code ignored;
        return std::filesystem::equivalent(path, openPath, ignored);
    }

    // A value that is changed by dragging it shows the horizontal resize pointer while it can be, and while it is.
    bool s_isValueDragged = false;

    ImVec4 lineColor(fried::core::LineKind kind)
    {
        switch (kind)
        {
        case fried::core::LineKind::Error:
            return editor_ui::palette::tomato;
        case fried::core::LineKind::Warning:
            return editor_ui::palette::warning;
        case fried::core::LineKind::Success:
            return editor_ui::palette::leaf;
        case fried::core::LineKind::Command:
            return editor_ui::palette::accentText;
        case fried::core::LineKind::Progress:
            return editor_ui::palette::whiteDim;
        default:
            return editor_ui::palette::white;
        }
    }

    void showDragCursor()
    {
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f))
        {
            s_isValueDragged = true;
        }
    }

    // A button without a look of its own, so the pointer is what says it can be pressed.
    bool pickerButton(const char *label, ImVec2 size)
    {
        bool isPressed = ImGui::InvisibleButton(label, size);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        return isPressed;
    }

    bool dragInt(const char *label, int *value)
    {
        bool isChanged = ImGui::DragInt(label, value);
        showDragCursor();
        return isChanged;
    }

    bool dragInt4(const char *label, int *values)
    {
        bool isChanged = ImGui::DragInt4(label, values);
        showDragCursor();
        return isChanged;
    }

    bool dragFloat(const char *label, float *value, float speed, float low, float high, const char *format)
    {
        bool isChanged = ImGui::DragFloat(label, value, speed, low, high, format);
        showDragCursor();
        return isChanged;
    }

    // The pointer over the viewport says what a press there would do.
    ImGuiMouseCursor viewportCursor(const fried::editor::Editor::PointerTarget &target)
    {
        using Kind = fried::editor::Editor::PointerTarget::Kind;
        using Handle = fried::editor::GizmoHandle::Kind;
        switch (target.m_kind)
        {
        case Kind::Pan:
            return ImGuiMouseCursor_Hand;
        case Kind::Object:
            return ImGuiMouseCursor_ResizeAll;
        case Kind::Handle:
            switch (target.m_handle)
            {
            case Handle::MoveX:
            case Handle::ScaleX:
                return ImGuiMouseCursor_ResizeEW;
            case Handle::MoveY:
            case Handle::ScaleY:
                return ImGuiMouseCursor_ResizeNS;
            case Handle::MoveXY:
                return ImGuiMouseCursor_ResizeAll;
            case Handle::ScaleBoth:
                return ImGuiMouseCursor_ResizeNWSE;
            case Handle::Rotate:
                return ImGuiMouseCursor_Hand;
            }
            break;
        case Kind::None:
            break;
        }
        return ImGuiMouseCursor_Arrow;
    }


    ImU32 colorOf(unsigned int argb)
    {
        return IM_COL32((argb >> 16) & 0xff, (argb >> 8) & 0xff, argb & 0xff, (argb >> 24) & 0xff);
    }

    ImVec4 vectorOf(const Json &value)
    {
        if (!value.is_object())
        {
            return ImVec4(0, 0, 0, 1);
        }
        return ImVec4(value.value("r", 0) / 255.0f, value.value("g", 0) / 255.0f, value.value("b", 0) / 255.0f, value.value("a", 255) / 255.0f);
    }

    int channel(float value)
    {
        return std::clamp((int)std::lround(value * 255.0f), 0, 255);
    }

    // The label of a field is its name, and what follows ## only tells ImGui which widget is which.
    std::string label(const std::string &name)
    {
        return "##" + name;
    }

    using fried::editor::AnchorAxis;

    // Where an anchor is along one axis of a small picture of the screen, as a fraction of it.
    float anchorPlace(AnchorAxis axis, bool isMaximum)
    {
        switch (axis)
        {
        case AnchorAxis::Start:
            return 0.0f;
        case AnchorAxis::Center:
            return 0.5f;
        case AnchorAxis::End:
            return 1.0f;
        default:
            return isMaximum ? 1.0f : 0.0f;
        }
    }

    // The screen as a frame, and in it the area the anchors span, with a marker at each corner of that area.
    void drawAnchorIcon(ImDrawList *list, ImVec2 position, float size, float minX, float maxX, float minY, float maxY, ImU32 color)
    {
        const float margin = size * 0.18f;
        list->AddRect(position, ImVec2(position.x + size, position.y + size), IM_COL32(150, 150, 160, 255), 2.0f);
        float inner = size - margin * 2;
        auto place = [](float fraction) { return std::clamp(fraction, 0.0f, 1.0f); };
        ImVec2 low(position.x + margin + inner * place(minX), position.y + margin + inner * place(minY));
        ImVec2 high(position.x + margin + inner * place(maxX), position.y + margin + inner * place(maxY));
        if (low.x != high.x || low.y != high.y)
        {
            list->AddRect(low, high, color);
        }
        for (float x : {low.x, high.x})
        {
            for (float y : {low.y, high.y})
            {
                list->AddRectFilled(ImVec2(x - 2, y - 2), ImVec2(x + 2, y + 2), color);
            }
        }
    }

    // The name of a field, and the width left for its widget.
    // Fields called x, y and z, or a name ending in X, Y and Z such as scaleX, that follow each other and are numbers: the axes of one value.
    bool isAxisStart(const std::vector<fried::editor::FieldView> &fields, std::size_t first, std::vector<const fried::editor::FieldView *> *axes, std::string *label)
    {
        const std::string &name = fields[first].m_schema.m_name;
        if (fields[first].m_schema.m_kind != "float" || name.empty())
        {
            return false;
        }
        bool isLower = name == "x";
        bool isUpper = name.size() > 1 && name.back() == 'X';
        if (!isLower && !isUpper)
        {
            return false;
        }
        std::string base = isLower ? std::string() : name.substr(0, name.size() - 1);
        axes->clear();
        axes->push_back(&fields[first]);
        for (char axis : std::string(isLower ? "yz" : "YZ"))
        {
            std::size_t next = first + axes->size();
            if (next >= fields.size() || fields[next].m_schema.m_kind != "float" || fields[next].m_schema.m_name != base + axis)
            {
                break;
            }
            axes->push_back(&fields[next]);
        }
        if (axes->size() < 2)
        {
            return false;
        }
        *label = isLower ? "Position" : fried::editor::displayName(base);
        return true;
    }

    // The names of the properties take a share of the inspector's width, so they are neither cut in a narrow one nor lost in a wide one.
    float captionWidth()
    {
        return std::clamp(ImGui::GetWindowWidth() * 0.38f, editor_ui::scaled(90.0f), editor_ui::scaled(220.0f));
    }

    // A field the file sets to something other than the default has an arrow at the end of its name, which takes it back to the default.
    bool isRevertable(const fried::editor::FieldView &field)
    {
        if (!field.m_isSet)
        {
            return false;
        }
        return field.m_schema.m_default.is_null() ? !field.m_value.is_null() : field.m_value != field.m_schema.m_default;
    }

    // Draws the arrow in the row that starts at the position, and says whether it was clicked.
    bool revertArrow(ImVec2 rowStart)
    {
        float side = editor_ui::Icons::size();
        ImVec2 low(rowStart.x + captionWidth() - side - editor_ui::scaled(11), rowStart.y + (ImGui::GetFrameHeight() - side) / 2);
        ImVec2 high(low.x + side, low.y + side);
        bool isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseHoveringRect(low, high);
        editor_ui::Icons::draw(ImGui::GetWindowDrawList(), editor_ui::Icon::Reload, low, isHovered ? IM_COL32_WHITE : IM_COL32(255, 255, 255, 120));
        if (isHovered)
        {
            ImGui::SetTooltip("Revert to the default");
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        return isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    }

    void caption(const std::string &name)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(fried::editor::displayName(name).c_str());
        ImGui::SameLine(captionWidth());
        ImGui::SetNextItemWidth(-FLT_MIN);
    }
}

namespace
{
    std::string quoted(const std::string &text)
    {
        std::string result = "'";
        for (char letter : text)
        {
            result += letter == '\'' ? std::string("'\\''") : std::string(1, letter);
        }
        return result + "'";
    }

    std::string fileUrl(const std::string &path)
    {
        static const char *s_hex = "0123456789ABCDEF";
        std::string url = "file://";
        for (unsigned char letter : path)
        {
            if (std::isalnum(letter) || letter == '/' || letter == '-' || letter == '_' || letter == '.' || letter == '~')
            {
                url += (char)letter;
            }
            else
            {
                url += '%';
                url += s_hex[letter >> 4];
                url += s_hex[letter & 15];
            }
        }
        return url;
    }

    // Opens the file's folder in the file manager with the file selected, and just the folder when the manager cannot select.
    void revealFile(const std::string &path)
    {
#if defined(_WIN32)
        std::string command = "explorer /select,\"" + std::filesystem::path(path).make_preferred().string() + "\"";
        std::system(command.c_str());
#elif defined(__APPLE__)
        std::system(("open -R " + quoted(path)).c_str());
#else
        std::string command = "dbus-send --session --print-reply --dest=org.freedesktop.FileManager1 /org/freedesktop/FileManager1 "
                              "org.freedesktop.FileManager1.ShowItems array:string:" + quoted(fileUrl(path)) + " string:\"\" >/dev/null 2>&1";
        if (std::system(command.c_str()) != 0)
        {
            SDL_OpenURL(fileUrl(std::filesystem::path(path).parent_path().string()).c_str());
        }
#endif
    }
}

namespace editor_ui
{
    Panels::Panels(Editor &editor)
        : m_editor(editor)
    {
    }

    void Panels::toolbar()
    {
        using fried::editor::Editor;
        using fried::editor::GameRunner;
        const bool is2D = m_editor.viewMode() == Editor::ViewMode::TwoD;
        const float gap = scaled(16);
        if (m_editor.isPlaying())
        {
            ImVec2 position = ImGui::GetWindowPos();
            ImVec2 size = ImGui::GetWindowSize();
            ImGui::GetWindowDrawList()->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y), ImGui::GetColorU32(palette::accentAt(0.40f)));
        }

        struct Mode
        {
            const char *m_id;
            Icon m_icon;
            const char *m_name;
            Shortcut m_shortcut;
            GizmoMode m_mode;
        };
        bool isFirst = true;
        for (const Mode &mode : {Mode{"move", Icon::ToolMove, "Move", Shortcut::MoveTool, GizmoMode::Move}, Mode{"rotate", Icon::ToolRotate, "Rotate", Shortcut::RotateTool, GizmoMode::Rotate},
                                 Mode{"scale", Icon::ToolScale, "Scale", Shortcut::ScaleTool, GizmoMode::Scale}})
        {
            bool isDisabled = !is2D && mode.m_mode != GizmoMode::Move;
            if (isDisabled)
            {
                ImGui::BeginDisabled();
            }
            if (!isFirst)
            {
                ImGui::SameLine();
            }
            isFirst = false;
            if (Icons::button(mode.m_id, mode.m_icon, withShortcut(mode.m_name, mode.m_shortcut).c_str(), m_editor.gizmoMode() == mode.m_mode))
            {
                m_editor.setGizmoMode(mode.m_mode);
            }
            if (isDisabled)
            {
                ImGui::EndDisabled();
            }
        }

        ImGui::SameLine(0, gap);
        for (Editor::ViewMode view : {Editor::ViewMode::TwoD, Editor::ViewMode::ThreeD})
        {
            bool isActive = m_editor.viewMode() == view;
            if (isActive)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, palette::accent);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            }
            if (ImGui::Button(view == Editor::ViewMode::TwoD ? "2D" : "3D"))
            {
                m_editor.setViewMode(view);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", view == Editor::ViewMode::TwoD ? "Edit the scene in 2D" : "Edit the scene in 3D");
            }
            if (isActive)
            {
                ImGui::PopStyleColor(2);
            }
            if (view == Editor::ViewMode::TwoD)
            {
                ImGui::SameLine(0, scaled(2));
            }
        }

        if (is2D)
        {
            ImGui::SameLine(0, gap);
            Editor::GridSettings grid = m_editor.grid();
            bool isChanged = false;
            if (Icons::button("grid", Icon::Grid, "Show the grid", grid.m_isShown))
            {
                grid.m_isShown = !grid.m_isShown;
                isChanged = true;
            }
            ImGui::SameLine();
            if (Icons::button("snap", Icon::Snap, "Snap what is dragged to the grid. Hold Ctrl to do the opposite.", grid.m_isSnapping))
            {
                grid.m_isSnapping = !grid.m_isSnapping;
                isChanged = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(scaled(72));
            int step = (int)grid.m_step;
            if (ImGui::DragInt("##grid-step", &step, 0.5f, 1, 1024, "%d px"))
            {
                grid.m_step = step;
                isChanged = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("The size of a grid square, and of a snapped move");
            }
            if (isChanged)
            {
                m_editor.setGrid(grid);
            }
        }
        float leftEnd = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x;

        // The play button sits in the middle of the strip, as in Unity, and the strip is tinted while the game runs so that it is clear that edits will not be kept.
        GameRunner::State game = m_editor.gameState();
        bool isBuilding = game == GameRunner::State::Building;
        bool isPlaying = m_editor.isPlaying() || m_editor.isStartingPlay();
        float side = Icons::size() + ImGui::GetStyle().FramePadding.y * 2;
        float playWidth = side * 4 + ImGui::GetStyle().ItemSpacing.x * 3;
        float middle = (ImGui::GetWindowWidth() - playWidth) / 2;
        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(middle, leftEnd + gap));
        if (Icons::button("play", isPlaying ? Icon::Stop : Icon::Play, isPlaying ? withShortcut("Stop the game", Shortcut::Stop).c_str() : withShortcut("Play the open scene", Shortcut::PlayScene).c_str(), isPlaying))
        {
            isPlaying ? m_editor.stopPlay() : m_editor.startPlay(false);
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(isPlaying);
        if (Icons::button("play-start", Icon::PlayStart, withShortcut("Play the project from its start scene", Shortcut::PlayProject).c_str()))
        {
            m_editor.startPlay(true);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_editor.isPlaying());
        if (Icons::button("pause", Icon::Pause, withShortcut(m_editor.isPaused() ? "Resume the game" : "Pause the game", Shortcut::Pause).c_str(), m_editor.isPaused()))
        {
            m_editor.setPaused(!m_editor.isPaused());
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_editor.isPaused());
        if (Icons::button("next-frame", Icon::NextFrame, withShortcut("Run one frame of the paused game", Shortcut::NextFrame).c_str()))
        {
            m_editor.stepFrame();
        }
        ImGui::EndDisabled();
        if (m_editor.isPlaying())
        {
            // How fast the game's time goes compared to real time, from a sixteenth to sixteen times.
            static const double s_speeds[] = {0.0625, 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0};
            static const char *s_speedNames[] = {"1/16x", "1/8x", "1/4x", "1/2x", "1x", "2x", "4x", "8x", "16x"};
            int current = 4;
            for (int index = 0; index < 9; ++index)
            {
                if (s_speeds[index] == m_editor.playSpeed())
                {
                    current = index;
                }
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(scaled(70));
            if (ImGui::BeginCombo("##play-speed", s_speedNames[current]))
            {
                for (int index = 0; index < 9; ++index)
                {
                    if (ImGui::Selectable(s_speedNames[index], index == current))
                    {
                        m_editor.setPlaySpeed(s_speeds[index]);
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("How fast the game's time goes, on top of its own time scale");
            }
        }
        if (isBuilding)
        {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Building...");
        }

        float platformWidth = scaled(150);
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - platformWidth - ImGui::GetStyle().WindowPadding.x);
        ImGui::SetNextItemWidth(platformWidth);
        // The game draws at the screen size of the platform it was started with, so the platform is chosen before it runs.
        ImGui::BeginDisabled(m_editor.isPlaying());
        if (ImGui::BeginCombo("##platform", fried::editor::platformLabel(m_editor.platform()).c_str()))
        {
            for (fried::editor::Platform platform : fried::editor::s_platforms)
            {
                if (ImGui::Selectable(fried::editor::platformLabel(platform).c_str(), platform == m_editor.platform()))
                {
                    m_editor.setPlatform(platform);
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("%s", m_editor.isPlaying() ? "Stop the game to change the platform."
                                                         : "The platform whose screen and limits the editor previews. The game draws at its screen size but still runs on this PC.");
        }
    }

    void Panels::buildWindow()
    {
        fried::editor::BuildProgress progress = m_editor.buildProgress();
        if (!progress.m_isVisible)
        {
            return;
        }

        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(scaled(760), scaled(440)), ImGuiCond_Appearing);
        bool isOpen = true;
        ImGui::SetNextWindowClass(ownWindowClass());
        if (ImGui::Begin((progress.m_title + "###build").c_str(), progress.m_isFinished ? &isOpen : nullptr, ownWindowFlags("###build")))
        {
            if (progress.m_isFailed)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, palette::tomato);
                ImGui::TextWrapped("%s", progress.m_message.c_str());
                ImGui::PopStyleColor();
            }
            else if (progress.m_isFinished)
            {
                ImGui::TextColored(palette::leaf, "%s", progress.m_message.empty() ? "The build is done." : progress.m_message.c_str());
            }
            else
            {
                std::string step = progress.m_steps > 0 ? "Step " + std::to_string(progress.m_step) + " of " + std::to_string(progress.m_steps) + ": " + progress.m_stepLabel : "Starting";
                ImGui::TextUnformatted(step.c_str());
                // The steps have no length of their own to report, so the bar only advances when a step finishes.
                float done = progress.m_steps > 0 ? (float)(progress.m_step - 1) / progress.m_steps : 0.0f;
                ImGui::ProgressBar(done, ImVec2(-FLT_MIN, 0), "");
            }

            float footer = ImGui::GetFrameHeightWithSpacing() + 4;
            ImGui::BeginChild("##buildlog", ImVec2(0, -footer), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
            // Following the output is the point, but not at the cost of pulling the view away from someone who scrolled up to read something.
            bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1;
            if (progress.m_lines == nullptr || progress.m_lines->empty())
            {
                ImGui::TextDisabled("%s", progress.m_isFinished ? "Nothing was printed." : "Waiting for the first line of output");
            }
            else
            {
                ImGuiListClipper clipper;
                clipper.Begin((int)progress.m_lines->size());
                while (clipper.Step())
                {
                    for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
                    {
                        const fried::core::OutputLine &line = (*progress.m_lines)[index];
                        ImGui::PushStyleColor(ImGuiCol_Text, lineColor(line.m_kind));
                        ImGui::TextUnformatted(line.m_text.c_str());
                        ImGui::PopStyleColor();
                    }
                }
            }
            if (wasAtBottom)
            {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();

            if (progress.m_canStop && ImGui::Button("Stop"))
            {
                m_editor.stopBuild();
            }
            if (progress.m_isFinished && ImGui::Button("Close"))
            {
                isOpen = false;
            }
            if (!progress.m_builtFile.empty())
            {
                ImGui::SameLine();
                if (ImGui::Button("Show in Folder"))
                {
                    revealFile(progress.m_builtFile);
                }
            }
            if (progress.m_lines != nullptr && !progress.m_lines->empty())
            {
                ImGui::SameLine();
                if (ImGui::Button("Copy Output"))
                {
                    std::string text;
                    for (const fried::core::OutputLine &line : *progress.m_lines)
                    {
                        text += line.m_text + "\n";
                    }
                    ImGui::SetClipboardText(text.c_str());
                }
            }
        }
        ImGui::End();
        if (!isOpen)
        {
            m_editor.dismissBuild();
        }
    }

    void Panels::console()
    {
        fried::editor::LogTail &log = m_editor.gameLog();
        if (ImGui::Button("Clear"))
        {
            log.clear();
        }
        struct Toggle
        {
            const char *m_label;
            int m_count;
        };
        for (int kind = 0; kind < 3; ++kind)
        {
            Toggle toggle = kind == 0 ? Toggle{"Errors", log.errorCount()} : kind == 1 ? Toggle{"Warnings", log.warningCount()} : Toggle{"Output", -1};
            std::string label = toggle.m_count >= 0 ? std::string(toggle.m_label) + " " + std::to_string(toggle.m_count) : std::string(toggle.m_label);
            ImGui::SameLine();
            bool isOn = m_consoleShow[kind];
            if (isOn)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                ImGui::PushStyleColor(ImGuiCol_Text, kind == 0 ? palette::tomato : kind == 1 ? palette::warning : palette::white);
            }
            if (ImGui::Button(label.c_str()))
            {
                m_consoleShow[kind] = !isOn;
            }
            if (isOn)
            {
                ImGui::PopStyleColor(2);
            }
        }
        ImGui::SameLine();
        float copyWidth = ImGui::CalcTextSize("Copy").x + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(-copyWidth);
        ImGui::InputTextWithHint("##console-search", "Search", &m_consoleSearch);
        ImGui::SameLine();
        bool isCopyAsked = ImGui::Button("Copy");
        ImGui::Separator();

        // The lines that pass the toggles and the search, found again only when the log or the filter changed.
        const auto &lines = log.lines();
        int mask = (m_consoleShow[0] ? 1 : 0) | (m_consoleShow[1] ? 2 : 0) | (m_consoleShow[2] ? 4 : 0);
        bool isFiltered = mask != 7 || !m_consoleSearch.empty();
        if (isFiltered && (m_consoleKeyRevision != log.revision() || m_consoleKeyText != m_consoleSearch || m_consoleKeyMask != mask))
        {
            m_consoleKeyRevision = log.revision();
            m_consoleKeyText = m_consoleSearch;
            m_consoleKeyMask = mask;
            m_consoleRows.clear();
            for (std::size_t index = 0; index < lines.size(); ++index)
            {
                using fried::core::LineKind;
                int bit = lines[index].m_kind == LineKind::Error ? 1 : lines[index].m_kind == LineKind::Warning ? 2 : 4;
                if ((mask & bit) && (m_consoleSearch.empty() || containsIgnoringCase(lines[index].m_text, m_consoleSearch)))
                {
                    m_consoleRows.push_back(index);
                }
            }
        }
        int count = isFiltered ? (int)m_consoleRows.size() : (int)lines.size();

        if (isCopyAsked)
        {
            std::string text;
            for (int row = 0; row < count; ++row)
            {
                text += lines[isFiltered ? m_consoleRows[row] : (std::size_t)row].m_text + "\n";
            }
            ImGui::SetClipboardText(text.c_str());
        }

        ImGui::BeginChild("##console-lines", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
        // Following the output is the point, but not at the cost of pulling the view away from someone who scrolled up to read something.
        bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1;
        if (count == 0)
        {
            ImGui::TextDisabled("%s", lines.empty() ? "Nothing printed yet. Play the game to see its output." : "No line passes the filter");
        }
        ImGuiListClipper clipper;
        clipper.Begin(count);
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const fried::core::OutputLine &line = lines[isFiltered ? m_consoleRows[row] : (std::size_t)row];
                ImGui::PushStyleColor(ImGuiCol_Text, lineColor(line.m_kind));
                ImGui::TextUnformatted(line.m_text.c_str());
                ImGui::PopStyleColor();
            }
        }
        if (wasAtBottom)
        {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }

    void Panels::statusBar()
    {
        auto &commands = m_editor.commands();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(m_editor.message().c_str());
        std::size_t issues = m_issues.size() + m_sceneIssues.size() + m_inputIssues.size();
        std::string right = (issues == 0 ? std::string() : std::to_string(issues) + (issues == 1 ? " issue   " : " issues   ")) + (m_editor.hasModifiedScenes() ? "Unsaved changes" : "Saved");
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right.c_str()).x - scaled(20));
        ImGui::TextColored(m_editor.hasModifiedScenes() ? palette::warning : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "%s", right.c_str());
    }

    bool Panels::takeValueDrag()
    {
        bool isDragged = s_isValueDragged;
        s_isValueDragged = false;
        return isDragged;
    }

    bool Panels::takeGameDrawn()
    {
        bool isDrawn = m_isGameDrawn;
        m_isGameDrawn = false;
        if (!isDrawn)
        {
            m_isGameFocused = false;
        }
        return isDrawn;
    }

    bool Panels::takeSceneDrawn()
    {
        bool isDrawn = m_isSceneDrawn;
        m_isSceneDrawn = false;
        return isDrawn;
    }

    void Panels::releaseGame()
    {
        ImGui::SetWindowFocus("Scene###Viewport");
        m_isGameFocused = false;
    }

    void Panels::gameView()
    {
        m_isGameDrawn = true;
        m_isGameFocused = ImGui::IsWindowFocused() && m_editor.isPlaying();
        const fried::editor::GameFrame &frame = m_editor.gameFrame();
        const bool isPlaying = m_editor.isPlaying();
        if (isPlaying)
        {
            if ((m_isTextureThePreview || m_uploadedGameSerial != m_editor.gameFrameSerial()) && frame.m_width > 0
                && frame.m_rgba.size() == (size_t)frame.m_width * frame.m_height * 4)
            {
                m_gameFrame.upload(frame.m_rgba.data(), frame.m_width, frame.m_height);
                m_uploadedGameSerial = m_editor.gameFrameSerial();
                m_isTextureThePreview = false;
            }
        }
        else
        {
            // Without the game running the tab shows the scene as the game would draw it, as Unity's does.
            const fried::editor::Frame &preview = m_editor.previewFrame();
            if ((!m_isTextureThePreview || m_uploadedPreviewSerial != m_editor.previewFrameSerial()) && preview.m_width > 0
                && preview.m_rgba.size() == (size_t)preview.m_width * preview.m_height * 4)
            {
                m_gameFrame.upload(preview.m_rgba.data(), preview.m_width, preview.m_height);
                m_uploadedPreviewSerial = m_editor.previewFrameSerial();
                m_isTextureThePreview = true;
            }
        }
        // What is on screen is the game's own frame only once it has sent one.
        const bool isGameShown = isPlaying && !m_isTextureThePreview;

        ImGui::SetNextItemWidth(scaled(72));
        const char *scales[] = {"Fit", "1x", "2x", "3x"};
        if (ImGui::BeginCombo("##game-scale", scales[m_gameScale]))
        {
            for (int index = 0; index < 4; ++index)
            {
                if (ImGui::Selectable(scales[index], index == m_gameScale))
                {
                    m_gameScale = index;
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("How big the game is shown");
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(scaled(110));
        struct Limit
        {
            const char *m_label;
            int m_fps;
        };
        static const Limit s_limits[] = {{"Project rate", -1}, {"30 FPS", 30}, {"60 FPS", 60}, {"120 FPS", 120}, {"240 FPS", 240}, {"Unlimited", 0}};
        if (ImGui::BeginCombo("##game-limit", s_limits[m_gameLimit].m_label))
        {
            for (int index = 0; index < (int)(sizeof(s_limits) / sizeof(s_limits[0])); ++index)
            {
                if (ImGui::Selectable(s_limits[index].m_label, index == m_gameLimit))
                {
                    m_gameLimit = index;
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("How many frames a second the game runs at. The project's rate is its own default frame rate, or this screen's refresh rate when the project waits for vsync or has none. Unlimited shares the graphics card with the editor, which can then draw less smoothly.");
        }
        ImGui::SameLine();
        ImGui::Checkbox("Stats", &m_isStatsShown);
        if (isPlaying)
        {
            int refresh = 60;
            if (SDL_Window *window = SDL_GL_GetCurrentWindow())
            {
                SDL_DisplayMode mode;
                if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &mode) == 0 && mode.refresh_rate > 0)
                {
                    refresh = mode.refresh_rate;
                }
            }
            m_editor.gameSession().setFrameLimit(s_limits[m_gameLimit].m_fps, refresh);
        }

        ImVec2 region = ImGui::GetContentRegionAvail();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##game", region, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
        bool isHovered = ImGui::IsItemHovered();
        ImDrawList *draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, ImVec2(origin.x + region.x, origin.y + region.y), IM_COL32(12, 12, 14, 255));

        if (m_gameFrame.isEmpty())
        {
            m_frameTimes.clear();
            std::string message = m_editor.isStartingPlay() ? "Starting the game..." : std::string("Press ") + shortcutText(Shortcut::PlayScene) + " to run the game here";
            ImVec2 size = ImGui::CalcTextSize(message.c_str());
            draw->AddText(ImVec2(origin.x + (region.x - size.x) / 2, origin.y + (region.y - size.y) / 2), IM_COL32(150, 150, 160, 255), message.c_str());
            return;
        }

        double scale = m_gameScale == 0 ? std::min((double)region.x / m_gameFrame.width(), (double)region.y / m_gameFrame.height()) : (double)m_gameScale;
        ImVec2 corner(origin.x + (float)((region.x - m_gameFrame.width() * scale) / 2), origin.y + (float)((region.y - m_gameFrame.height() * scale) / 2));
        ImVec2 end(corner.x + (float)(m_gameFrame.width() * scale), corner.y + (float)(m_gameFrame.height() * scale));
        draw->PushClipRect(origin, ImVec2(origin.x + region.x, origin.y + region.y), true);
        draw->AddImage(m_gameFrame.id(), corner, end);

        // The game's own pixels are what it asks for, so the mouse is sent in them.
        if (!isGameShown)
        {
            if (!isPlaying)
            {
                std::string hint = std::string("Press ") + shortcutText(Shortcut::PlayScene) + " to run the game";
                ImVec2 extent = ImGui::CalcTextSize(hint.c_str());
                draw->AddText(ImVec2(origin.x + (region.x - extent.x) / 2, origin.y + region.y - extent.y - scaled(10)), IM_COL32(150, 150, 160, 255), hint.c_str());
            }
            draw->PopClipRect();
            return;
        }
        fried::editor::GameSession &session = m_editor.gameSession();
        ImGuiIO &io = ImGui::GetIO();
        int gameX = (int)std::floor((io.MousePos.x - corner.x) / scale);
        int gameY = (int)std::floor((io.MousePos.y - corner.y) / scale);
        bool isInside = isHovered && gameX >= 0 && gameY >= 0 && gameX < m_gameFrame.width() && gameY < m_gameFrame.height();
        if (isInside && (gameX != m_gameMouseX || gameY != m_gameMouseY))
        {
            m_gameMouseX = gameX;
            m_gameMouseY = gameY;
            session.mouseMove(gameX, gameY);
        }
        // ImGui's left, right and middle are the engine's 1, 3 and 2.
        const int engineButtons[3] = {1, 3, 2};
        for (int button = 0; button < 3; ++button)
        {
            if (isInside && ImGui::IsMouseClicked(button))
            {
                m_isGameButtonDown[button] = true;
                session.mouseButton(engineButtons[button], gameX, gameY, true);
            }
            else if (m_isGameButtonDown[button] && ImGui::IsMouseReleased(button))
            {
                m_isGameButtonDown[button] = false;
                session.mouseButton(engineButtons[button], std::clamp(gameX, 0, m_gameFrame.width() - 1), std::clamp(gameY, 0, m_gameFrame.height() - 1), false);
            }
        }
        if (isInside && (io.MouseWheel != 0 || io.MouseWheelH != 0))
        {
            session.mouseWheel(io.MouseWheelH, io.MouseWheel);
        }

        // The frame rate, large over the corner, colored by how close it is to a smooth 60.
        const fried::editor::GameStats &stats = frame.m_stats;
        double now = ImGui::GetTime();
        if (now - m_frameTimeSampledAt >= 0.5)
        {
            m_frameTimeSampledAt = now;
            m_frameTimes.push_back((float)stats.m_frameMs);
            if (m_frameTimes.size() > 60)
            {
                m_frameTimes.erase(m_frameTimes.begin());
            }
        }
        ImVec4 tint = stats.m_fps >= 55.0 ? palette::leaf : stats.m_fps >= 30.0 ? palette::warning : palette::tomato;
        ImFont *font = boldFont() ? boldFont() : ImGui::GetFont();
        float bigSize = ImGui::GetFontSize() * 1.7f;
        float pad = scaled(10);
        char text[64];
        float y = origin.y + pad;
        auto line = [&](ImFont *useFont, float size, ImU32 color, const char *words) {
            ImVec2 extent = useFont->CalcTextSizeA(size, FLT_MAX, 0.0f, words);
            float x = origin.x + region.x - pad - extent.x;
            draw->AddText(useFont, size, ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, 200), words);
            draw->AddText(useFont, size, ImVec2(x, y), color, words);
            y += extent.y + 2;
        };
        std::snprintf(text, sizeof(text), "%.0f FPS", stats.m_fps);
        line(font, bigSize, ImGui::GetColorU32(tint), text);
        std::snprintf(text, sizeof(text), "%.1f ms", stats.m_frameMs);
        line(ImGui::GetFont(), ImGui::GetFontSize(), IM_COL32(220, 220, 225, 255), text);
        if (m_isStatsShown)
        {
            std::snprintf(text, sizeof(text), "Update and draw %.2f ms", stats.m_workMs);
            line(ImGui::GetFont(), ImGui::GetFontSize(), IM_COL32(220, 220, 225, 255), text);
            std::snprintf(text, sizeof(text), "%d objects", stats.m_objects);
            line(ImGui::GetFont(), ImGui::GetFontSize(), IM_COL32(220, 220, 225, 255), text);
            // The sections the engine times, in the order of fried.Profile; the last one is the whole of the work already shown above.
            static const char *const sectionNames[] = {"Fixed steps", "Update", "Draw", "Sort", "Flush", "Batch", "Present", nullptr, "UI style", "UI layout"};
            for (size_t section = 0; section < stats.m_sections.size() && section < sizeof(sectionNames) / sizeof(sectionNames[0]); ++section)
            {
                if (sectionNames[section] == nullptr || (section >= 8 && stats.m_sections[section] == 0.0))
                {
                    continue;
                }
                std::snprintf(text, sizeof(text), "%s %.2f ms", sectionNames[section], stats.m_sections[section]);
                line(ImGui::GetFont(), ImGui::GetFontSize(), IM_COL32(180, 180, 190, 255), text);
            }
            std::snprintf(text, sizeof(text), "%d x %d", m_gameFrame.width(), m_gameFrame.height());
            line(ImGui::GetFont(), ImGui::GetFontSize(), IM_COL32(220, 220, 225, 255), text);

            // The last half minute, one bar for each half second, taller the longer the frame took.
            float graphWidth = scaled(180);
            float graphHeight = scaled(40);
            float left = origin.x + region.x - pad - graphWidth;
            draw->AddRectFilled(ImVec2(left, y), ImVec2(left + graphWidth, y + graphHeight), IM_COL32(0, 0, 0, 120));
            float top = 1000.0f / 30.0f;
            for (size_t index = 0; index < m_frameTimes.size(); ++index)
            {
                float height = std::min(m_frameTimes[index] / top, 1.0f) * graphHeight;
                float x = left + graphWidth - (float)(m_frameTimes.size() - index) * (graphWidth / 60.0f);
                draw->AddRectFilled(ImVec2(x, y + graphHeight - height), ImVec2(x + graphWidth / 60.0f - 1, y + graphHeight), ImGui::GetColorU32(palette::accentText));
            }
        }
        if (!m_isGameFocused)
        {
            const char *hint = "Click the game to give it the keyboard";
            ImVec2 extent = ImGui::CalcTextSize(hint);
            draw->AddText(ImVec2(origin.x + (region.x - extent.x) / 2, origin.y + region.y - extent.y - pad), IM_COL32(150, 150, 160, 255), hint);
        }
        draw->PopClipRect();
    }

    // One tab for each scene that is open, as Godot has them. The scene on screen follows the tab that is chosen.
    void Panels::sceneTabs()
    {
        const std::vector<std::string> &scenes = m_editor.openScenes();
        const bool isLocked = m_editor.isPlaying() || m_editor.isStartingPlay();
        const std::string active = m_editor.scenePath();
        const bool isActiveChanged = active != m_lastActiveScene;
        if (!ImGui::BeginTabBar("##scene-tabs", ImGuiTabBarFlags_FittingPolicyScroll))
        {
            return;
        }
        std::string toClose;
        std::string toOpen;
        for (const std::string &path : scenes)
        {
            std::string name = std::filesystem::path(path).filename().string();
            if (fried::core::isSceneFile(name))
            {
                name = fried::core::sceneName(name);
            }
            bool isModified = m_editor.isSceneModified(path);
            std::string label = name + (isModified ? " *" : "") + "###tab-" + path;
            ImGuiTabItemFlags flags = isModified ? ImGuiTabItemFlags_NoAssumedClosure : 0;
            if (isActiveChanged && path == active)
            {
                flags |= ImGuiTabItemFlags_SetSelected;
            }
            bool isOpen = true;
            bool isSelected = ImGui::BeginTabItem(label.c_str(), isLocked || scenes.size() == 1 ? nullptr : &isOpen, flags);
            if (isSelected)
            {
                // The tab the user chose is the one that shows, which is not the editor's yet.
                if (path != active && !isActiveChanged && !isLocked)
                {
                    toOpen = path;
                }
                ImGui::EndTabItem();
            }
            if (!isOpen)
            {
                toClose = path;
            }
        }
        ImGui::EndTabBar();
        m_lastActiveScene = active;
        if (!toOpen.empty())
        {
            openSceneFile(toOpen);
        }
        if (!toClose.empty())
        {
            closeSceneTab(toClose);
        }
    }

    void Panels::viewport()
    {
        m_isSceneDrawn = true;
        sceneTabs();
        const fried::editor::Frame &frame = m_editor.frame();
        if (m_uploadedSerial != m_editor.frameSerial() && frame.m_rgba.size() == (size_t)frame.m_width * frame.m_height * 4)
        {
            m_frame.upload(frame.m_rgba.data(), frame.m_width, frame.m_height);
            m_uploadedSerial = m_editor.frameSerial();
        }

        ImVec2 region = ImGui::GetContentRegionAvail();
        m_editor.setViewSize((int)region.x, (int)region.y);
        ImVec2 origin = ImGui::GetCursorScreenPos();
        // The buttons over the view are submitted after it, so it has to let them be hovered, and the view itself is not hovered where they are.
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##view", region, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
        bool isHovered = ImGui::IsItemHovered();
        const bool isTwoD = m_editor.viewMode() == Editor::ViewMode::TwoD;
        const float side = Icons::size() + ImGui::GetStyle().FramePadding.y * 2;
        const std::string zoomText = std::to_string((int)std::lround(m_editor.viewport().zoom() * 100)) + "%";
        const ImVec2 barLow(origin.x + scaled(8), origin.y + scaled(8));
        const ImVec2 barHigh(barLow.x + side * 2 + ImGui::GetStyle().ItemSpacing.x * 2 + (isTwoD ? ImGui::CalcTextSize(zoomText.c_str()).x + ImGui::GetStyle().FramePadding.x * 2 : 0),
                             barLow.y + side);
        if (ImGui::IsMouseHoveringRect(barLow, barHigh))
        {
            isHovered = false;
        }

        ImDrawList *draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, ImVec2(origin.x + region.x, origin.y + region.y), IM_COL32(20, 20, 24, 255));
        if (m_frame.isEmpty())
        {
            const std::string &message = m_editor.message();
            ImVec2 size = ImGui::CalcTextSize(message.c_str());
            draw->AddText(ImVec2(origin.x + (region.x - size.x) / 2, origin.y + (region.y - size.y) / 2), IM_COL32(150, 150, 160, 255), message.c_str());
            return;
        }

        double scale = std::min((double)region.x / m_frame.width(), (double)region.y / m_frame.height());
        ImVec2 corner(origin.x + (float)((region.x - m_frame.width() * scale) / 2), origin.y + (float)((region.y - m_frame.height() * scale) / 2));
        ImVec2 end(corner.x + (float)(m_frame.width() * scale), corner.y + (float)(m_frame.height() * scale));
        draw->AddImage(m_frame.id(), corner, end);

        draw->PushClipRect(corner, end, true);
        for (const DrawCommand &command : m_editor.overlay())
        {
            ImVec2 from(corner.x + (float)(command.m_x * scale), corner.y + (float)(command.m_y * scale));
            ImVec2 size((float)(command.m_width * scale), (float)(command.m_height * scale));
            ImU32 color = colorOf(command.m_rgba);
            switch (command.m_kind)
            {
            case DrawCommand::Kind::Line:
                draw->AddLine(from, ImVec2(from.x + size.x, from.y + size.y), color, 1.5f);
                break;
            case DrawCommand::Kind::FilledRect:
                draw->AddRectFilled(from, ImVec2(from.x + size.x, from.y + size.y), color, 2.0f);
                break;
            case DrawCommand::Kind::FilledTriangle:
                draw->AddTriangleFilled(from, ImVec2(corner.x + (float)(command.m_x2 * scale), corner.y + (float)(command.m_y2 * scale)),
                                        ImVec2(corner.x + (float)(command.m_x3 * scale), corner.y + (float)(command.m_y3 * scale)), color);
                break;
            default:
                draw->AddRect(from, ImVec2(from.x + size.x, from.y + size.y), color, 0, 0, 1.5f);
                break;
            }
        }
        draw->PopClipRect();

        // The editor works in the frame's own pixels.
        if (m_dragKind >= 2)
        {
            // Some sessions (SDL2 over SDL3 on Wayland) never report the relative motion, so the pointer's own movement stands in for it.
            ImVec2 motion = m_heldMotion;
            if (motion.x == 0 && motion.y == 0)
            {
                motion = ImGui::GetIO().MouseDelta;
            }
            m_panPosition = ImVec2(m_panPosition.x + motion.x, m_panPosition.y + motion.y);
        }
        else
        {
            m_panPosition = ImGui::GetIO().MousePos;
        }
        ImVec2 mouse = m_panPosition;
        double frameX = (mouse.x - corner.x) / scale;
        double frameY = (mouse.y - corner.y) / scale;

        ImGuiIO &modalIo = ImGui::GetIO();
        bool isModalFrame = m_editor.isModal();
        if (!isModalFrame && isTwoD && isHovered && m_dragKind == 0 && !modalIo.WantTextInput && !modalIo.KeyCtrl && !modalIo.KeyAlt && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            if (ImGui::IsKeyPressed(ImGuiKey_G, false))
            {
                m_editor.beginModal(Editor::Modal::Move, frameX, frameY);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_R, false))
            {
                m_editor.beginModal(Editor::Modal::Rotate, frameX, frameY);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_S, false))
            {
                m_editor.beginModal(Editor::Modal::Scale, frameX, frameY);
            }
            isModalFrame = m_editor.isModal();
        }
        else if (isModalFrame)
        {
            m_editor.modalMove(frameX, frameY);
            if (ImGui::IsKeyPressed(ImGuiKey_X, false))
            {
                m_editor.modalAxis('x');
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
            {
                m_editor.modalAxis('y');
            }
            for (int digit = 0; digit <= 9; ++digit)
            {
                if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_0 + digit)) || ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_Keypad0 + digit)))
                {
                    m_editor.modalType((char)('0' + digit));
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract))
            {
                m_editor.modalType('-');
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Period) || ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal))
            {
                m_editor.modalType('.');
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
            {
                m_editor.modalBackspace();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                m_editor.confirmModal();
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                m_editor.cancelModal();
            }
        }
        if (m_editor.isModal() || isModalFrame)
        {
            std::string text = m_editor.modalText();
            if (!text.empty())
            {
                draw->AddText(ImVec2(corner.x + scaled(10), end.y - scaled(26)), IM_COL32(255, 220, 120, 255), text.c_str());
            }
        }

        if (isHovered && !isModalFrame)
        {
            for (int button : {0, 1, 2})
            {
                if (m_dragKind == 0 && ImGui::IsMouseClicked(button))
                {
                    m_dragKind = button + 1;
                    m_editor.pointerDown(frameX, frameY, button == 0 ? Editor::Pointer::Primary : (button == 1 ? Editor::Pointer::Look : Editor::Pointer::Pan));
                }
            }
            if (ImGui::GetIO().MouseWheel != 0)
            {
                m_editor.wheel(frameX, frameY, ImGui::GetIO().MouseWheel);
            }
        }
        if (m_dragKind == 2)
        {
            // Holding the right button makes the camera a free one that the keys fly.
            auto held = [](ImGuiKey key) { return ImGui::IsKeyDown(key) ? 1.0 : 0.0; };
            double forward = held(ImGuiKey_W) - held(ImGuiKey_S);
            double right = held(ImGuiKey_D) - held(ImGuiKey_A);
            double up = held(ImGuiKey_E) - held(ImGuiKey_Q);
            if (forward != 0 || right != 0 || up != 0)
            {
                m_editor.fly(forward, right, up, ImGui::GetIO().DeltaTime, ImGui::GetIO().KeyShift);
            }
        }
        if (m_dragKind != 0)
        {
            int button = m_dragKind - 1;
            if (ImGui::IsMouseDown(button))
            {
                m_editor.pointerMove(frameX, frameY);
            }
            else
            {
                m_editor.pointerUp();
                m_dragKind = 0;
            }
        }

        if (isHovered || m_dragKind != 0)
        {
            ImGui::SetMouseCursor(viewportCursor(m_editor.pointerTarget(frameX, frameY)));
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(s_assetPayload))
            {
                std::string root, path;
                if (fried::editor::parseAssetReference(std::string((const char *)payload->Data, payload->DataSize - 1), &root, &path))
                {
                    m_editor.dropAsset(root, path, frameX, frameY);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // What the view offers: the selected node to the middle, the view as a scene first has it, and in 2D the zoom, which a click sets back to the game's own pixels.
        ImGui::SetCursorScreenPos(barLow);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.27f, 0.85f));
        if (Icons::button("frame", Icon::CenterView, withShortcut("Bring the selected object to the middle", Shortcut::FrameSelected).c_str()))
        {
            m_editor.focusSelected();
        }
        ImGui::SameLine();
        if (Icons::button("reset", Icon::ZoomReset, "Reset the view"))
        {
            m_editor.resetView();
        }
        if (isTwoD)
        {
            ImGui::SameLine();
            if (ImGui::Button(zoomText.c_str()))
            {
                m_editor.zoomTo(1);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("The zoom. Click for 100%%, the game's own pixels.");
            }
        }
        ImGui::PopStyleColor(2);
    }

    namespace
    {
        constexpr const char *s_nodePayload = "fried-node";

    }

    Icon Panels::iconOf(const std::string &type)
    {
        // The classes come with the player, so a new player may bring others.
        if (m_typeIconsSchema != m_editor.classes().size())
        {
            m_typeIcons.clear();
            m_typeIconsSchema = m_editor.classes().size();
        }
        auto cached = m_typeIcons.find(type);
        if (cached != m_typeIcons.end())
        {
            return cached->second;
        }
        static const std::map<std::string, Icon> s_builtIn = {
            {"Transform", Icon::Node},
            {"Transform2D", Icon::Node2D},
            {"Transform3D", Icon::Node3D},
            {"RectTransform", Icon::Control},
            {"SpriteRenderer", Icon::Sprite2D},
            {"Camera", Icon::Camera2D},
            {"Camera3D", Icon::Camera3D},
            {"Collider2D", Icon::CollisionShape2D},
            {"Body2D", Icon::CharacterBody2D},
            {"MeshRenderer", Icon::MeshInstance3D},
            {"Light", Icon::DirectionalLight3D},
            {"Canvas", Icon::Control},
            {"Label", Icon::Label},
            {"Button", Icon::Button},
            {"TextureRect", Icon::TextureRect},
            {"TouchButton", Icon::TouchButton},
            {"VirtualJoystick", Icon::VirtualJoystick},
        };
        Icon icon = Icon::Node;
        for (const std::string &name : m_editor.classChain(type))
        {
            auto found = s_builtIn.find(name);
            if (found != s_builtIn.end())
            {
                icon = found->second;
                break;
            }
        }
        m_typeIcons[type] = icon;
        return icon;
    }

    // The scenes of the project that can be instanced where the scene is being edited, the others shown dimmed with why.
    void Panels::instanceMenu(const std::optional<std::string> &parent)
    {
        for (const std::string &path : m_editor.scenePaths())
        {
            std::string name = std::filesystem::path(path).filename().string();
            name = fried::core::sceneName(name);
            std::string problem = m_editor.instanceProblem(name);
            if (ImGui::MenuItem(name.c_str(), nullptr, false, problem.empty()))
            {
                std::string error;
                if (!m_editor.addInstance(path, parent, &error))
                {
                    m_editor.setMessage(error);
                }
            }
            if (!problem.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("%s", problem.c_str());
            }
        }
    }

    void Panels::hierarchy()
    {
        if (Icons::button("add", Icon::Add, m_editor.selected() ? "Add an object under the selected one (Ctrl+A)" : "Add an object to the scene (Ctrl+A)"))
        {
            showCreateNode(m_editor.selected());
        }
        ImGui::SameLine();
        if (Icons::button("instance", Icon::PackedScene, "Instance a scene as a child of the selected object"))
        {
            ImGui::OpenPopup("add-node");
        }
        if (ImGui::BeginPopup("add-node"))
        {
            ImGui::TextDisabled(m_editor.selected() ? "Instance under the selected object" : "Instance in the scene");
            instanceMenu(m_editor.selected());
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        {
            // The icon sits inside the field, so the text starts after it.
            float room = Icons::size() + scaled(14);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(room, ImGui::GetStyle().FramePadding.y));
            ImGui::InputTextWithHint("##hierarchy-filter", "Search by name or type", &m_hierarchyFilter);
            ImGui::PopStyleVar();
            ImVec2 low = ImGui::GetItemRectMin();
            ImVec2 high = ImGui::GetItemRectMax();
            Icons::draw(ImGui::GetWindowDrawList(), Icon::Search, ImVec2(low.x + scaled(8), (low.y + high.y - Icons::size()) / 2), IM_COL32(255, 255, 255, 150));
        }
        // Which nodes are folded belongs to the scene, and is remembered with the view.
        std::string sceneName = std::filesystem::path(m_editor.scenePath()).filename().string();
        if (sceneName != m_collapsedScene)
        {
            m_collapsedScene = sceneName;
            m_collapsed.clear();
            Json remembered = fried::editor::EditorState::projectValue(m_editor.project().directory(), "collapsed");
            if (remembered.is_object() && remembered.contains(sceneName) && remembered[sceneName].is_array())
            {
                for (const Json &id : remembered[sceneName])
                {
                    if (id.is_string())
                    {
                        m_collapsed.insert(id.get<std::string>());
                    }
                }
            }
        }
        bool isCollapseChanged = false;

        const std::vector<fried::editor::HierarchyRow> &rows = m_editor.hierarchy();

        // The filter keeps the rows that match by name or type and the ones above them, so a match is seen in its place. The folds are ignored while it is on.
        bool isFiltering = !m_hierarchyFilter.empty();
        if (isFiltering && (m_filterKeyText != m_hierarchyFilter || m_filterKeyVersion != m_editor.document().version() || m_filterKeep.size() != rows.size()))
        {
            m_filterKeyText = m_hierarchyFilter;
            m_filterKeyVersion = m_editor.document().version();
            m_filterKeep.assign(rows.size(), 0);
            std::vector<std::size_t> ancestors;
            for (std::size_t position = 0; position < rows.size(); ++position)
            {
                const auto &row = rows[position];
                ancestors.resize(std::min<std::size_t>(ancestors.size(), (std::size_t)row.m_depth));
                ancestors.push_back(position);
                if (containsIgnoringCase(row.m_name, m_hierarchyFilter) || containsIgnoringCase(row.m_type, m_hierarchyFilter) || containsIgnoringCase(row.m_transformType, m_hierarchyFilter))
                {
                    m_filterKeep[position] = 2;
                    for (auto above = ancestors.rbegin() + 1; above != ancestors.rend() && m_filterKeep[*above] == 0; ++above)
                    {
                        m_filterKeep[*above] = 1;
                    }
                }
            }
        }

        // The rows under a collapsed node are left out before anything is drawn, and only the rows in view are drawn of the rest, so a scene of thousands of nodes costs what fits on the screen.
        m_visibleRows.clear();
        int hiddenBelowDepth = -1;
        for (std::size_t position = 0; position < rows.size(); ++position)
        {
            if (isFiltering)
            {
                if (m_filterKeep[position] != 0)
                {
                    m_visibleRows.push_back(position);
                }
                continue;
            }
            if (hiddenBelowDepth >= 0 && rows[position].m_depth > hiddenBelowDepth)
            {
                continue;
            }
            hiddenBelowDepth = -1;
            m_visibleRows.push_back(position);
            if (rows[position].m_hasChildren && m_collapsed.count(rows[position].m_id))
            {
                hiddenBelowDepth = rows[position].m_depth;
            }
        }
        if (isFiltering && m_visibleRows.empty())
        {
            ImGui::TextDisabled("No object matches");
        }

        // Rows are as tall as a button and touch each other, so the stripes behind them tile.
        const float rowPadding = scaled(4);
        const float rowHeight = ImGui::GetFontSize() + rowPadding * 2;
        const float levelWidth = ImGui::GetTreeNodeToLabelSpacing();
        const float baseX = ImGui::GetCursorScreenPos().x;
        const ImVec2 windowPosition = ImGui::GetWindowPos();
        const float windowRight = windowPosition.x + ImGui::GetWindowWidth();

        // Up and Down choose the row above or below, so the inspector follows, and the list scrolls to keep it in view.
        int scrollTo = -1;
        if (!m_visibleRows.empty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput)
        {
            bool isDown = ImGui::IsKeyPressed(ImGuiKey_DownArrow);
            if (isDown || ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                int current = -1;
                for (int visible = 0; visible < (int)m_visibleRows.size() && m_editor.selected(); ++visible)
                {
                    if (rows[m_visibleRows[visible]].m_id == *m_editor.selected())
                    {
                        current = visible;
                    }
                }
                int target = current < 0 ? (isDown ? 0 : (int)m_visibleRows.size() - 1) : std::clamp(current + (isDown ? 1 : -1), 0, (int)m_visibleRows.size() - 1);
                if (target != current)
                {
                    m_editor.select(rows[m_visibleRows[target]].m_id);
                    scrollTo = target;
                }
            }
            bool isRight = ImGui::IsKeyPressed(ImGuiKey_RightArrow);
            if (!isFiltering && m_editor.selected() && (isRight || ImGui::IsKeyPressed(ImGuiKey_LeftArrow)))
            {
                // Right opens a folded node and then goes into it, and Left folds an open one and then goes up to its parent.
                for (int visible = 0; visible < (int)m_visibleRows.size(); ++visible)
                {
                    const auto &row = rows[m_visibleRows[visible]];
                    if (row.m_id != *m_editor.selected())
                    {
                        continue;
                    }
                    bool isFolded = m_collapsed.count(row.m_id) > 0;
                    if (isRight && row.m_hasChildren && isFolded)
                    {
                        m_collapsed.erase(row.m_id);
                        isCollapseChanged = true;
                    }
                    else if (isRight && row.m_hasChildren && visible + 1 < (int)m_visibleRows.size())
                    {
                        m_editor.select(rows[m_visibleRows[visible + 1]].m_id);
                        scrollTo = visible + 1;
                    }
                    else if (!isRight && row.m_hasChildren && !isFolded)
                    {
                        m_collapsed.insert(row.m_id);
                        isCollapseChanged = true;
                    }
                    else if (!isRight)
                    {
                        for (int above = visible - 1; above >= 0; --above)
                        {
                            if (rows[m_visibleRows[above]].m_depth < row.m_depth)
                            {
                                m_editor.select(rows[m_visibleRows[above]].m_id);
                                scrollTo = above;
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }

        ImGuiListClipper clipper;
        clipper.Begin((int)m_visibleRows.size(), rowHeight);
        if (scrollTo >= 0)
        {
            clipper.IncludeItemByIndex(scrollTo);
        }
        while (clipper.Step())
        {
            for (int visible = clipper.DisplayStart; visible < clipper.DisplayEnd; ++visible)
            {
                const auto &row = rows[m_visibleRows[visible]];
                bool isSelected = m_editor.selected() && *m_editor.selected() == row.m_id;
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_FramePadding;
                flags |= isSelected ? ImGuiTreeNodeFlags_Selected : 0;
                flags |= row.m_hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf;
                float indent = row.m_depth * levelWidth;

                ImDrawList *list = ImGui::GetWindowDrawList();
                float top = ImGui::GetCursorScreenPos().y;
                if (visible % 2 == 1)
                {
                    list->AddRectFilled(ImVec2(windowPosition.x, top), ImVec2(windowRight, top + rowHeight), IM_COL32(255, 255, 255, 9));
                }
                for (int level = 0; level < row.m_depth; ++level)
                {
                    float x = std::floor(baseX + level * levelWidth + levelWidth / 2);
                    list->AddLine(ImVec2(x, top), ImVec2(x, top + rowHeight), IM_COL32(255, 255, 255, 28));
                }

                // The lock is at the end of the row, and a click on it does not select the row.
                const float lockSize = Icons::size();
                const ImVec2 lockLow(windowRight - lockSize - scaled(10), top + (rowHeight - lockSize) / 2);
                const bool isRowHovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(ImVec2(windowPosition.x, top), ImVec2(windowRight, top + rowHeight));
                const bool isLockHovered = isRowHovered && ImGui::IsMouseHoveringRect(ImVec2(lockLow.x - scaled(4), top), ImVec2(windowRight, top + rowHeight));
                const bool isLocked = m_editor.isLocked(row.m_id);
                if (isLockHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    m_editor.setLocked(row.m_id, !isLocked);
                }

                ImGui::PushStyleColor(ImGuiCol_Header, palette::accentAt(0.22f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, isSelected ? palette::accentAt(0.30f) : ImVec4(1, 1, 1, 0.07f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, palette::accentAt(0.36f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, rowPadding));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0));
                ImGui::Indent(indent);
                ImGui::SetNextItemOpen(isFiltering || !m_collapsed.count(row.m_id));
                bool isOpen = ImGui::TreeNodeEx(row.m_id.c_str(), flags, "%s", "");
                ImGui::Unindent(indent);
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(3);

                ImVec2 itemMin = ImGui::GetItemRectMin();
                float iconSize = Icons::size();
                float contentX = itemMin.x + levelWidth;
                if (isSelected)
                {
                    list->AddRectFilled(ImVec2(windowPosition.x, top), ImVec2(windowPosition.x + scaled(2), top + rowHeight), ImGui::GetColorU32(palette::accentHot));
                }
                Icons::draw(list, row.m_instance.empty() ? iconOf(row.m_type) : Icon::PackedScene, ImVec2(contentX, top + (rowHeight - iconSize) / 2));
                ImVec4 nameColor = isFiltering && m_filterKeep[m_visibleRows[visible]] == 2 ? palette::accentText : ImGui::GetStyleColorVec4(ImGuiCol_Text);
                if (!row.m_isActive)
                {
                    nameColor.w *= 0.45f;
                }
                list->AddText(ImVec2(contentX + iconSize + scaled(6), top + rowPadding), ImGui::GetColorU32(nameColor), row.m_name.c_str());
                if (isLocked)
                {
                    Icons::draw(list, Icon::Lock, lockLow, ImGui::GetColorU32(palette::accentText));
                }
                else if (isRowHovered)
                {
                    Icons::draw(list, Icon::Unlock, lockLow, IM_COL32(255, 255, 255, isLockHovered ? 200 : 70));
                }
                if (isLockHovered)
                {
                    ImGui::SetTooltip("%s", isLocked ? "Unlock: the object can be picked and moved in the view again" : "Lock: the object cannot be picked or moved in the view");
                }
                if (!row.m_instance.empty())
                {
                    ImVec2 openLow(lockLow.x - iconSize - scaled(6), lockLow.y);
                    bool isOpenHovered = isRowHovered && ImGui::IsMouseHoveringRect(openLow, ImVec2(openLow.x + iconSize, openLow.y + iconSize));
                    Icons::draw(list, Icon::PackedScene, openLow, IM_COL32(255, 255, 255, isOpenHovered ? 230 : 90));
                    if (isOpenHovered)
                    {
                        ImGui::SetTooltip("Open %s in its own tab", row.m_instance.c_str());
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        {
                            openSceneFile(m_editor.scenePathOf(row.m_instance));
                        }
                    }
                }
                if (row.m_isCamera)
                {
                    Icons::draw(list, Icon::Camera2D, ImVec2(lockLow.x - iconSize - scaled(6), lockLow.y), ImGui::GetColorU32(palette::accentText));
                }
                if (row.m_hasChildren && !isFiltering)
                {
                    if (isOpen)
                    {
                        isCollapseChanged |= m_collapsed.erase(row.m_id) > 0;
                    }
                    else
                    {
                        isCollapseChanged |= m_collapsed.insert(row.m_id).second;
                    }
                }

                if (visible == scrollTo)
                {
                    ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleEdgeY);
                }
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() && !isLockHovered)
                {
                    m_editor.select(row.m_id);
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen() && !isLockHovered)
                {
                    m_renameId = row.m_id;
                    m_renameText = row.m_name;
                    m_isRenameAsked = true;
                }

                const bool isRoot = row.m_id == m_editor.document().rootId();
                if (!isRoot && ImGui::BeginDragDropSource())
                {
                    ImGui::SetDragDropPayload(s_nodePayload, row.m_id.c_str(), row.m_id.size() + 1);
                    ImGui::TextUnformatted(row.m_name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *asset = ImGui::AcceptDragDropPayload(s_assetPayload))
                    {
                        std::string root, path;
                        if (fried::editor::parseAssetReference(std::string((const char *)asset->Data, asset->DataSize - 1), &root, &path))
                        {
                            m_editor.addAssetNode(root, path, row.m_id);
                        }
                    }
                    ImVec2 low = ImGui::GetItemRectMin();
                    ImVec2 high = ImGui::GetItemRectMax();
                    float place = (ImGui::GetMousePos().y - low.y) / std::max(1.0f, high.y - low.y);
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(s_nodePayload, ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
                    {
                        std::string dropped((const char *)payload->Data);
                        const fried::editor::SceneDocument &document = m_editor.document();
                        if (!isRoot && place < 0.25f)
                        {
                            m_editor.reparent(dropped, document.parentOf(row.m_id), document.indexOf(row.m_id));
                        }
                        else if (!isRoot && place > 0.75f && !(row.m_hasChildren && isOpen))
                        {
                            m_editor.reparent(dropped, document.parentOf(row.m_id), document.indexOf(row.m_id) + 1);
                        }
                        else
                        {
                            m_editor.reparent(dropped, row.m_id, document.childCount(row.m_id));
                        }
                    }
                    else if (const ImGuiPayload *held = ImGui::GetDragDropPayload(); held && held->IsDataType(s_nodePayload))
                    {
                        // Shows where the drop would go.
                        ImDrawList *list = ImGui::GetWindowDrawList();
                        ImU32 color = IM_COL32(255, 200, 0, 255);
                        if (isRoot)
                        {
                            list->AddRect(low, high, color);
                        }
                        else if (place < 0.25f)
                        {
                            list->AddLine(ImVec2(low.x, low.y), ImVec2(high.x, low.y), color, 2.0f);
                        }
                        else if (place > 0.75f)
                        {
                            list->AddLine(ImVec2(low.x, high.y), ImVec2(high.x, high.y), color, 2.0f);
                        }
                        else
                        {
                            list->AddRect(low, high, color);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (ImGui::BeginPopupContextItem(("node-menu-" + row.m_id).c_str()))
                {
                    m_editor.select(row.m_id);
                    if (ImGui::MenuItem("Add Child Object..."))
                    {
                        showCreateNode(row.m_id);
                    }
                    if (ImGui::MenuItem("Add Component..."))
                    {
                        showAddComponent(row.m_id);
                    }
                    if (ImGui::BeginMenu("Instance Child Scene"))
                    {
                        instanceMenu(row.m_id);
                        ImGui::EndMenu();
                    }
                    if (!row.m_instance.empty() && ImGui::MenuItem("Open Scene"))
                    {
                        openSceneFile(m_editor.scenePathOf(row.m_instance));
                    }
                    if (row.m_instance.empty() && ImGui::MenuItem("Change Transform Type..."))
                    {
                        showChangeType(row.m_id);
                    }
                    if (ImGui::MenuItem("Rename...", shortcutText(Shortcut::Rename)))
                    {
                        m_renameId = row.m_id;
                        m_renameText = row.m_name;
                        m_isRenameAsked = true;
                    }
                    if (ImGui::MenuItem(row.m_isActive ? "Deactivate" : "Activate"))
                    {
                        m_editor.setActive(row.m_id, !row.m_isActive);
                    }
                    bool hasCamera = false;
                    for (const fried::editor::ComponentRef &ref : m_editor.document().componentsOf(row.m_id))
                    {
                        hasCamera = hasCamera || ref.m_type == "Camera" || ref.m_type == "Camera3D";
                    }
                    if (ImGui::MenuItem("Use as Camera", nullptr, false, hasCamera))
                    {
                        m_editor.setCameraToSelected();
                    }
                    if (ImGui::MenuItem("Move Up", nullptr, false, !isRoot))
                    {
                        m_editor.moveSelected(-1);
                    }
                    if (ImGui::MenuItem("Move Down", nullptr, false, !isRoot))
                    {
                        m_editor.moveSelected(1);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Duplicate", shortcutText(Shortcut::Duplicate), false, !isRoot))
                    {
                        m_editor.duplicateSelected();
                    }
                    if (ImGui::MenuItem("Copy", shortcutText(Shortcut::Copy)))
                    {
                        std::string text = m_editor.copySelected();
                        ImGui::SetClipboardText(text.c_str());
                    }
                    if (ImGui::MenuItem("Paste", shortcutText(Shortcut::Paste)))
                    {
                        const char *text = ImGui::GetClipboardText();
                        m_editor.paste(text ? text : "");
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete", shortcutText(Shortcut::Delete), false, !isRoot))
                    {
                        m_editor.removeSelected();
                    }
                    ImGui::EndPopup();
                }
            }
        }

        // The space below the rows is the scene's top level, where an asset is dropped to add it there and a node to move it to the end.
        ImGui::InvisibleButton("##hierarchy-end", ImVec2(std::max(1.0f, ImGui::GetContentRegionAvail().x), std::max(scaled(40), ImGui::GetContentRegionAvail().y)));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *asset = ImGui::AcceptDragDropPayload(s_assetPayload))
            {
                std::string root, path;
                if (fried::editor::parseAssetReference(std::string((const char *)asset->Data, asset->DataSize - 1), &root, &path))
                {
                    m_editor.addAssetNode(root, path, "");
                }
            }
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(s_nodePayload))
            {
                std::string top = m_editor.document().rootId();
                m_editor.reparent(std::string((const char *)payload->Data), top, m_editor.document().childCount(top));
            }
            ImGui::EndDragDropTarget();
        }

        if (isCollapseChanged)
        {
            Json remembered = fried::editor::EditorState::projectValue(m_editor.project().directory(), "collapsed");
            if (!remembered.is_object())
            {
                remembered = Json::object();
            }
            remembered[sceneName] = Json(m_collapsed);
            fried::editor::EditorState::setProjectValue(m_editor.project().directory(), "collapsed", remembered);
        }

        if (m_editor.selected() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput && isShortcutPressed(Shortcut::Rename))
        {
            for (const fried::editor::HierarchyRow &row : rows)
            {
                if (row.m_id == *m_editor.selected())
                {
                    m_renameId = row.m_id;
                    m_renameText = row.m_name;
                    m_isRenameAsked = true;
                    break;
                }
            }
        }
        if (m_isRenameAsked)
        {
            ImGui::OpenPopup("rename-node");
            m_isRenameAsked = false;
        }
        if (ImGui::BeginPopup("rename-node"))
        {
            ImGui::TextDisabled("Name");
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetNextItemWidth(scaled(220));
            if (ImGui::InputText("##rename", &m_renameText, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                m_editor.rename(m_renameId, m_renameText);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    bool Panels::textField(const char *name, std::string *value)
    {
        ImGuiID id = ImGui::GetID(name);
        std::string &edit = m_textEdits[id];
        if (ImGui::GetCurrentContext()->ActiveId != id)
        {
            edit = *value;
        }
        ImGui::InputText(name, &edit);
        if (ImGui::IsItemDeactivatedAfterEdit() && edit != *value)
        {
            *value = edit;
            return true;
        }
        return false;
    }

    // A text area that shows the lines of the string, however many, with its name above it so it has the whole width. It is applied when the area loses focus, as a line is, so typing is one step of undo.
    bool Panels::textArea(const char *name, std::string *value)
    {
        ImGuiID id = ImGui::GetID(name);
        std::string &edit = m_textEdits[id];
        if (ImGui::GetCurrentContext()->ActiveId != id)
        {
            edit = *value;
        }
        // The height follows the lines as they are wrapped, which is how many fit in the width the area has.
        float wrapWidth = std::max(40.0f, ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x * 2 - ImGui::GetStyle().ScrollbarSize);
        int lines = 0;
        for (size_t start = 0; start <= edit.size();)
        {
            size_t end = edit.find('\n', start);
            std::string paragraph = edit.substr(start, end == std::string::npos ? std::string::npos : end - start);
            float width = ImGui::CalcTextSize(paragraph.c_str()).x;
            lines += std::max(1, (int)std::ceil(width / wrapWidth));
            if (end == std::string::npos)
            {
                break;
            }
            start = end + 1;
        }
        float height = ImGui::GetTextLineHeight() * (float)std::clamp(lines + 1, 3, 12) + ImGui::GetStyle().FramePadding.y * 2;
        ImGui::InputTextMultiline(name, &edit, ImVec2(-FLT_MIN, height), ImGuiInputTextFlags_WordWrap);
        if (ImGui::IsItemDeactivatedAfterEdit() && edit != *value)
        {
            *value = edit;
            return true;
        }
        return false;
    }

    bool Panels::referenceField(const FieldView &field, AssetKind wanted)
    {
        return referenceEditor(field.m_value, wanted, [&](const Json &value) { m_editor.setField(m_component, field.m_schema.m_name, value); });
    }

    bool Panels::referenceEditor(const Json &current, AssetKind wanted, const std::function<void(const Json &)> &commit)
    {
        const bool isFont = wanted == AssetKind::Font;
        const char *noun = isFont ? "font" : wanted == AssetKind::Model ? "model" : wanted == AssetKind::Html ? "HTML file" : wanted == AssetKind::Style ? "style sheet" : "image";
        const char *nouns = isFont ? "fonts" : wanted == AssetKind::Model ? "models" : wanted == AssetKind::Html ? "HTML files" : wanted == AssetKind::Style ? "style sheets" : "images";
        std::string text = fried::editor::assetReferenceText(current, isFont);
        const float side = Icons::size() + ImGui::GetStyle().FramePadding.y * 2;
        ImGui::SetNextItemWidth(-(side + ImGui::GetStyle().ItemSpacing.x));
        bool changed = textField("##reference", &text);
        if (changed)
        {
            if (auto reference = fried::editor::assetReferenceFromText(text, isFont))
            {
                commit(*reference);
            }
        }

        // An asset dragged from the browser onto the field is the reference.
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(s_assetPayload))
            {
                if (auto reference = fried::editor::assetReferenceFromText(std::string((const char *)payload->Data, payload->DataSize - 1) + (isFont ? "@16" : ""), isFont))
                {
                    commit(*reference);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // The picker lists the project's files of the kind the field takes, to choose one without typing its path.
        ImGui::SameLine();
        if (Icons::button("pick", Icon::Search, (std::string("Choose a ") + noun + " from the project").c_str()))
        {
            m_pickSearch.clear();
            ImGui::OpenPopup("pick-asset");
        }
        ImGui::SetNextWindowSize(ImVec2(scaled(340), scaled(380)), ImGuiCond_Appearing);
        if (ImGui::BeginPopup("pick-asset"))
        {
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##pick-search", (std::string("Search the ") + nouns).c_str(), &m_pickSearch);
            ImGui::Separator();

            std::vector<const fried::editor::AssetEntry *> matches;
            for (const fried::editor::AssetEntry &entry : m_editor.assets())
            {
                if (entry.m_kind == wanted && (m_pickSearch.empty() || containsIgnoringCase(entry.reference(), m_pickSearch)))
                {
                    matches.push_back(&entry);
                }
            }

            ImGui::BeginChild("##pick-list", ImVec2(0, 0));
            std::string currentText = fried::editor::assetReferenceText(current, false);
            const float row = std::max(Icons::size(), scaled(24)) + scaled(4);
            ImGuiListClipper clipper;
            clipper.Begin((int)matches.size() + 1, row);
            while (clipper.Step())
            {
                for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
                {
                    ImGui::PushID(index);
                    ImVec2 position = ImGui::GetCursorScreenPos();
                    const fried::editor::AssetEntry *entry = index == 0 ? nullptr : matches[index - 1];
                    bool isCurrent = entry ? entry->reference() == currentText : current.is_null();
                    bool isChosen = ImGui::Selectable("##choice", isCurrent, ImGuiSelectableFlags_None, ImVec2(0, row));
                    ImDrawList *list = ImGui::GetWindowDrawList();
                    float centre = position.y + row / 2;
                    if (!entry)
                    {
                        list->AddText(ImVec2(position.x + scaled(8), centre - ImGui::GetFontSize() / 2), ImGui::GetColorU32(ImGuiCol_TextDisabled), "None");
                    }
                    else
                    {
                        const Texture *thumbnail = wanted != AssetKind::Image ? nullptr : m_images.get(m_editor.assetFile(entry->m_root, entry->m_path));
                        float thumb = row - scaled(4);
                        if (thumbnail)
                        {
                            float ratio = std::min(thumb / thumbnail->width(), thumb / thumbnail->height());
                            ImVec2 size(thumbnail->width() * ratio, thumbnail->height() * ratio);
                            ImVec2 low(position.x + scaled(4) + (thumb - size.x) / 2, centre - size.y / 2);
                            list->AddImage(thumbnail->id(), low, ImVec2(low.x + size.x, low.y + size.y));
                        }
                        else
                        {
                            Icons::draw(list, isFont ? Icon::Font : wanted == AssetKind::Image ? Icon::Image : wanted == AssetKind::Model ? Icon::MeshInstance3D : Icon::File, ImVec2(position.x + scaled(4) + (thumb - Icons::size()) / 2, centre - Icons::size() / 2));
                        }
                        std::string name = entry->m_path.substr(entry->m_path.rfind('/') == std::string::npos ? 0 : entry->m_path.rfind('/') + 1);
                        float textX = position.x + thumb + scaled(12);
                        list->AddText(ImVec2(textX, centre - ImGui::GetFontSize() / 2), ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
                        list->AddText(ImVec2(textX + ImGui::CalcTextSize(name.c_str()).x + scaled(8), centre - ImGui::GetFontSize() / 2), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                                      fried::editor::assetFolderOf(*entry).c_str());
                    }
                    if (isChosen)
                    {
                        if (!entry)
                        {
                            commit(Json());
                        }
                        else
                        {
                            int size = current.is_object() ? current.value("size", 16) : 16;
                            if (auto reference = fried::editor::assetReferenceFromText(entry->reference() + (isFont ? "@" + std::to_string(size) : ""), isFont))
                            {
                                commit(*reference);
                            }
                        }
                        changed = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                }
            }
            if (matches.empty())
            {
                ImGui::TextDisabled("The project has no %s like that", noun);
            }
            ImGui::EndChild();
            ImGui::EndPopup();
        }
        return changed;
    }

    void Panels::resourceEditor(const Json &value, const std::string &category, bool allowsFile, const std::function<void(const Json &)> &commit)
    {
        const bool isFile = value.is_object() && !value.contains("type") && value.contains("path");
        const std::string typeName = value.is_object() && value.contains("type") && value["type"].is_string() ? value["type"].get<std::string>() : std::string();
        const std::string shown = isFile ? "File" : typeName.empty() ? "(none)" : typeName;

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##type", shown.c_str()))
        {
            if (ImGui::Selectable("(none)", shown == "(none)"))
            {
                commit(Json());
            }
            for (const fried::editor::ResourceSchema &resource : m_editor.resources())
            {
                if (resource.m_category != category)
                {
                    continue;
                }
                if (ImGui::Selectable(resource.m_name.c_str(), resource.m_name == typeName))
                {
                    Json fresh = {{"type", resource.m_name}};
                    if (category == "mesh" && value.is_object() && value.contains("material"))
                    {
                        fresh["material"] = value["material"];
                    }
                    commit(fresh);
                }
            }
            if (allowsFile && ImGui::Selectable("File...", isFile))
            {
                commit(Json{{"root", "game"}, {"path", ""}});
            }
            ImGui::EndCombo();
        }

        if (isFile)
        {
            referenceEditor(value, AssetKind::Model, commit);
            return;
        }
        const fried::editor::ResourceSchema *schema = typeName.empty() ? nullptr : m_editor.resourceNamed(typeName);
        if (!schema)
        {
            return;
        }

        ImGui::Indent();
        for (const fried::editor::FieldSchema &field : schema->m_fields)
        {
            const std::string &name = field.m_name;
            const Json current = value.contains(name) ? value[name] : field.m_default;
            auto set = [&](const Json &changed) {
                Json copy = value;
                if (changed.is_null())
                {
                    copy.erase(name);
                }
                else
                {
                    copy[name] = changed;
                }
                commit(copy);
            };
            ImGui::PushID(name.c_str());
            caption(name);
            if (field.m_kind == "int")
            {
                int edit = current.is_number() ? current.get<int>() : 0;
                if (dragInt("##v", &edit))
                {
                    set(Json(edit));
                }
            }
            else if (field.m_kind == "float")
            {
                float edit = current.is_number() ? current.get<float>() : 0.0f;
                if (dragFloat("##v", &edit, 0.01f, 0, 0, "%.3f"))
                {
                    set(Json((double)edit));
                }
            }
            else if (field.m_kind == "bool")
            {
                bool edit = current.is_boolean() && current.get<bool>();
                if (ImGui::Checkbox("##v", &edit))
                {
                    set(Json(edit));
                }
            }
            else if (field.m_kind == "color")
            {
                ImVec4 color = vectorOf(current);
                if (ImGui::ColorEdit4("##v", &color.x, ImGuiColorEditFlags_AlphaPreviewHalf))
                {
                    set(Json{{"r", channel(color.x)}, {"g", channel(color.y)}, {"b", channel(color.z)}, {"a", channel(color.w)}});
                }
            }
            else if (!field.m_values.empty())
            {
                std::string chosen = current.is_string() ? current.get<std::string>() : std::string();
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::BeginCombo("##v", chosen.c_str()))
                {
                    for (const std::string &option : field.m_values)
                    {
                        if (ImGui::Selectable(option.c_str(), option == chosen))
                        {
                            set(Json(option));
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            else if (field.m_kind == "texture")
            {
                referenceEditor(current, AssetKind::Image, set);
            }
            else if (field.m_kind == "material")
            {
                resourceEditor(current, "material", false, set);
            }
            ImGui::PopID();
        }
        ImGui::Unindent();
    }

    void Panels::fieldEditor(const FieldView &field)
    {
        const std::string &name = field.m_schema.m_name;
        const std::string &kind = field.m_schema.m_kind;
        ImGui::PushID(name.c_str());
        // The text of a Label and the cells of a Tilemap are written as lines, so their name goes above the area and not beside it.
        bool isTextArea = kind == "string" && (name == "text" || name == "cells");
        if (isTextArea)
        {
            ImGui::TextDisabled("%s", name.c_str());
        }
        else
        {
            ImVec2 rowStart = ImGui::GetCursorScreenPos();
            caption(name);
            if (isRevertable(field) && revertArrow(rowStart))
            {
                m_editor.revertFields(m_component, {name});
            }
        }

        if (kind == "int")
        {
            int value = field.m_value.is_number() ? field.m_value.get<int>() : 0;
            if (dragInt("##v", &value))
            {
                m_editor.setField(m_component, name, Json(value));
            }
        }
        else if (kind == "float")
        {
            float value = field.m_value.is_number() ? field.m_value.get<float>() : 0.0f;
            float speed = name == "rotation" ? 0.5f : (name.find("scale") == 0 || name == "zoom" ? 0.01f : 1.0f);
            if (dragFloat("##v", &value, speed, 0, 0, "%.3f"))
            {
                m_editor.setField(m_component, name, Json((double)value));
            }
        }
        else if (kind == "bool")
        {
            bool value = field.m_value.is_boolean() && field.m_value.get<bool>();
            if (ImGui::Checkbox("##v", &value))
            {
                m_editor.setField(m_component, name, Json(value));
            }
        }
        else if (kind == "string")
        {
            std::string value = field.m_value.is_string() ? field.m_value.get<std::string>() : std::string();
            if (isTextArea ? textArea("##v", &value) : textField("##v", &value))
            {
                m_editor.setField(m_component, name, Json(value));
            }
        }
        else if (kind == "color")
        {
            ImVec4 color = vectorOf(field.m_value);
            if (ImGui::ColorEdit4("##v", &color.x, ImGuiColorEditFlags_AlphaPreviewHalf))
            {
                m_editor.setField(m_component, name,
                                  Json{{"r", channel(color.x)}, {"g", channel(color.y)}, {"b", channel(color.z)}, {"a", channel(color.w)}});
            }
        }
        else if (!field.m_schema.m_values.empty())
        {
            std::string current = field.m_value.is_string() ? field.m_value.get<std::string>() : std::string();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##v", current.c_str()))
            {
                for (const std::string &option : field.m_schema.m_values)
                {
                    if (ImGui::Selectable(option.c_str(), option == current))
                    {
                        m_editor.setField(m_component, name, Json(option));
                    }
                }
                ImGui::EndCombo();
            }
        }
        else if (kind == "rect")
        {
            bool isUsed = field.m_value.is_object();
            if (ImGui::Checkbox("whole image", &isUsed) == true)
            {
                m_editor.setField(m_component, name, isUsed ? Json{{"x", 0}, {"y", 0}, {"width", 32}, {"height", 32}} : Json());
            }
            if (field.m_value.is_object())
            {
                int values[4] = {field.m_value.value("x", 0), field.m_value.value("y", 0), field.m_value.value("width", 0), field.m_value.value("height", 0)};
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (dragInt4("##region", values))
                {
                    m_editor.setField(m_component, name, Json{{"x", values[0]}, {"y", values[1]}, {"width", values[2]}, {"height", values[3]}});
                }
            }
        }
        else if (kind == "object")
        {
            std::string current = field.m_value.is_string() ? field.m_value.get<std::string>() : std::string();
            std::string shown = "(none)";
            const std::vector<fried::editor::HierarchyRow> &rows = m_editor.hierarchy();
            for (const auto &row : rows)
            {
                if (row.m_id == current)
                {
                    shown = row.m_name;
                }
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##v", shown.c_str()))
            {
                if (ImGui::Selectable("(none)", current.empty()))
                {
                    m_editor.setField(m_component, name, Json());
                }
                for (const auto &row : rows)
                {
                    ImGui::PushID(row.m_id.c_str());
                    if (ImGui::Selectable(row.m_name.c_str(), row.m_id == current))
                    {
                        m_editor.setField(m_component, name, Json(row.m_id));
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
        }
        else if (kind == "mesh" || kind == "material")
        {
            resourceEditor(field.m_value, kind, kind == "mesh", [&](const Json &value) { m_editor.setField(m_component, name, value); });
        }
        else
        {
            referenceField(field, kind == "font" ? AssetKind::Font : kind == "html" ? AssetKind::Html : kind == "stylesheet" ? AssetKind::Style : AssetKind::Image);
        }
        ImGui::PopID();
    }

    void Panels::vectorEditor(const std::string &label, const std::vector<const FieldView *> &axes)
    {
        static const ImVec4 s_axisColors[] = {ImVec4(0.89f, 0.35f, 0.35f, 1), ImVec4(0.45f, 0.78f, 0.35f, 1), ImVec4(0.35f, 0.58f, 0.92f, 1)};
        static const char *const s_axisNames[] = {"X", "Y", "Z"};
        ImGui::PushID(label.c_str());
        ImVec2 rowStart = ImGui::GetCursorScreenPos();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label.c_str());
        ImGui::SameLine(captionWidth());
        std::vector<std::string> revertable;
        for (const FieldView *axis : axes)
        {
            if (isRevertable(*axis))
            {
                revertable.push_back(axis->m_schema.m_name);
            }
        }
        if (!revertable.empty() && revertArrow(rowStart))
        {
            m_editor.revertFields(m_component, revertable);
        }
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float cell = (ImGui::GetContentRegionAvail().x - spacing * (axes.size() - 1)) / axes.size();
        for (std::size_t index = 0; index < axes.size(); ++index)
        {
            const FieldView &field = *axes[index];
            const std::string &name = field.m_schema.m_name;
            if (index > 0)
            {
                ImGui::SameLine();
            }
            ImGui::PushID(name.c_str());
            float letterWidth = ImGui::CalcTextSize("X").x + scaled(6);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(s_axisColors[index], "%s", s_axisNames[index]);
            ImGui::SameLine(0, scaled(2));
            ImGui::SetNextItemWidth(std::max(cell - letterWidth - scaled(2), scaled(30)));
            float value = field.m_value.is_number() ? field.m_value.get<float>() : 0.0f;
            float speed = name.find("otation") != std::string::npos ? 0.5f : name.find("cale") != std::string::npos ? 0.01f : 1.0f;
            if (dragFloat("##v", &value, speed, 0, 0, "%.3f"))
            {
                m_editor.setField(m_component, name, Json((double)value));
            }
            ImGui::PopID();
        }
        ImGui::PopID();
    }

    // A Control as pictures and the few numbers that matter: a preview of the object on the screen, the place it is anchored to, its pivot, and either its position or its distance to the edges it stretches between.
    void Panels::controlEditor(const fried::editor::ClassView &view)
    {
        auto find = [&](const char *name) -> const FieldView * {
            for (const FieldView &field : view.m_fields)
            {
                if (field.m_schema.m_name == name)
                {
                    return &field;
                }
            }
            return nullptr;
        };
        auto numberField = [&](const char *label, const char *name) {
            const FieldView *field = find(name);
            if (!field)
            {
                return;
            }
            ImGui::PushID(name);
            caption(label);
            float value = field->m_value.is_number() ? field->m_value.get<float>() : 0.0f;
            if (dragFloat("##v", &value, 1.0f, 0, 0, "%.0f"))
            {
                m_editor.setField(m_component, name, Json((double)value));
            }
            ImGui::PopID();
        };

        std::optional<fried::editor::RectPreview> preview = m_editor.rectPreview();
        const ImU32 anchorColor = IM_COL32(64, 192, 255, 255);
        if (preview)
        {
            ImDrawList *list = ImGui::GetWindowDrawList();

            // The anchor presets: the button shows the current one and opens the grid of all of them.
            ImVec2 buttonPosition = ImGui::GetCursorScreenPos();
            const float buttonSize = 44;
            if (pickerButton("##anchor-preset", ImVec2(buttonSize, buttonSize)))
            {
                ImGui::OpenPopup("anchor-presets");
            }
            drawAnchorIcon(list, buttonPosition, buttonSize, (float)preview->m_anchorMinX, (float)preview->m_anchorMaxX, (float)preview->m_anchorMinY,
                           (float)preview->m_anchorMaxY, anchorColor);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(preview->m_horizontal == AnchorAxis::Custom || preview->m_vertical == AnchorAxis::Custom ? "Anchor presets (custom anchors)" : "Anchor presets");
            }
            if (ImGui::BeginPopup("anchor-presets"))
            {
                static const AnchorAxis axes[] = {AnchorAxis::Start, AnchorAxis::Center, AnchorAxis::End, AnchorAxis::Stretch};
                static const char *columns[] = {"Left", "Center", "Right", "Stretch"};
                static const char *rows[] = {"Top", "Middle", "Bottom", "Stretch"};
                const float cell = 40;
                bool isCustom = preview->m_horizontal == AnchorAxis::Custom || preview->m_vertical == AnchorAxis::Custom;
                ImGui::TextDisabled(isCustom ? "Anchor to the parent (now custom)" : "Anchor to the parent");
                for (int row = 0; row < 4; ++row)
                {
                    for (int column = 0; column < 4; ++column)
                    {
                        if (column > 0)
                        {
                            ImGui::SameLine();
                        }
                        ImGui::PushID(row * 4 + column);
                        ImVec2 position = ImGui::GetCursorScreenPos();
                        bool isCurrent = preview->m_horizontal == axes[column] && preview->m_vertical == axes[row];
                        if (pickerButton("##preset", ImVec2(cell, cell)))
                        {
                            // Choosing the preset the object already has changes nothing.
                            if (!isCurrent)
                            {
                                m_editor.setAnchorPreset(axes[column], axes[row]);
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        bool isHovered = ImGui::IsItemHovered();
                        if (isHovered || isCurrent)
                        {
                            ImGui::GetWindowDrawList()->AddRectFilled(position, ImVec2(position.x + cell, position.y + cell), IM_COL32(255, 255, 255, isHovered ? 40 : 20), 2.0f);
                        }
                        drawAnchorIcon(ImGui::GetWindowDrawList(), ImVec2(position.x + 3, position.y + 3), cell - 6, anchorPlace(axes[column], false),
                                       anchorPlace(axes[column], true), anchorPlace(axes[row], false), anchorPlace(axes[row], true),
                                       isCurrent ? IM_COL32(255, 200, 0, 255) : anchorColor);
                        if (isHovered)
                        {
                            ImGui::SetTooltip("%s, %s", rows[row], columns[column]);
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::EndPopup();
            }

            // The pivot, which is the point of the object that sits on the anchor.
            ImGui::SameLine();
            ImVec2 pivotPosition = ImGui::GetCursorScreenPos();
            const float pivotCell = buttonSize / 3;
            pickerButton("##pivot", ImVec2(buttonSize, buttonSize));
            ImVec2 pointer = ImGui::GetIO().MousePos;
            bool isPivotHovered = ImGui::IsItemHovered();
            bool isPivotClicked = ImGui::IsItemClicked();
            list->AddRect(pivotPosition, ImVec2(pivotPosition.x + buttonSize, pivotPosition.y + buttonSize), IM_COL32(150, 150, 160, 255), 2.0f);
            for (int row = 0; row < 3; ++row)
            {
                for (int column = 0; column < 3; ++column)
                {
                    ImVec2 centre(pivotPosition.x + pivotCell * (column + 0.5f), pivotPosition.y + pivotCell * (row + 0.5f));
                    bool isCurrent = std::abs(preview->m_pivotX - column * 0.5) < 0.001 && std::abs(preview->m_pivotY - row * 0.5) < 0.001;
                    bool isOver = isPivotHovered && std::abs(pointer.x - centre.x) < pivotCell / 2 && std::abs(pointer.y - centre.y) < pivotCell / 2;
                    list->AddCircleFilled(centre, isCurrent ? 4.0f : 2.0f, isCurrent ? IM_COL32(255, 200, 0, 255) : IM_COL32(150, 150, 160, isOver ? 255 : 160));
                    if (isPivotClicked && isOver)
                    {
                        m_editor.setPivot(column * 0.5, row * 0.5);
                    }
                }
            }
            if (isPivotHovered)
            {
                ImGui::SetTooltip("Pivot");
            }

            // The control in its parent, with its anchors.
            ImGui::SameLine();
            float previewWidth = std::max(60.0f, ImGui::GetContentRegionAvail().x);
            float previewHeight = buttonSize;
            ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(previewWidth, previewHeight));
            // A parent that is not a control has no size, so its rectangle is a point and the anchors cannot spread across anything.
            float parentWidth = std::max(1.0f, (float)preview->m_viewWidth);
            float parentHeight = std::max(1.0f, (float)preview->m_viewHeight);
            float scale = std::min(previewWidth / parentWidth, previewHeight / parentHeight);
            ImVec2 screenSize(parentWidth * scale, parentHeight * scale);
            ImVec2 screenMin(origin.x + (previewWidth - screenSize.x) / 2, origin.y + (previewHeight - screenSize.y) / 2);
            auto toPreview = [&](double x, double y) { return ImVec2(screenMin.x + (float)x * scale, screenMin.y + (float)y * scale); };
            list->AddRectFilled(screenMin, ImVec2(screenMin.x + screenSize.x, screenMin.y + screenSize.y), IM_COL32(30, 30, 38, 255));
            list->AddRect(screenMin, ImVec2(screenMin.x + screenSize.x, screenMin.y + screenSize.y), IM_COL32(150, 150, 160, 255));
            double boxX = preview->m_x - preview->m_originX;
            double boxY = preview->m_y - preview->m_originY;
            list->AddRectFilled(toPreview(boxX, boxY), toPreview(boxX + preview->m_width, boxY + preview->m_height), IM_COL32(255, 200, 0, 90));
            list->AddRect(toPreview(boxX, boxY), toPreview(boxX + preview->m_width, boxY + preview->m_height), IM_COL32(255, 200, 0, 255));
            ImVec2 anchorLow = toPreview(preview->m_viewWidth * preview->m_anchorMinX, preview->m_viewHeight * preview->m_anchorMinY);
            ImVec2 anchorHigh = toPreview(preview->m_viewWidth * preview->m_anchorMaxX, preview->m_viewHeight * preview->m_anchorMaxY);
            if (anchorLow.x != anchorHigh.x || anchorLow.y != anchorHigh.y)
            {
                list->AddRect(anchorLow, anchorHigh, anchorColor);
            }
            for (float x : {anchorLow.x, anchorHigh.x})
            {
                for (float y : {anchorLow.y, anchorHigh.y})
                {
                    list->AddRectFilled(ImVec2(x - 2, y - 2), ImVec2(x + 2, y + 2), anchorColor);
                }
            }
            ImGui::Spacing();

            bool isStretchedX = preview->m_anchorMinX != preview->m_anchorMaxX;
            bool isStretchedY = preview->m_anchorMinY != preview->m_anchorMaxY;
            if (isStretchedX)
            {
                numberField("Left", "insetLeft");
                numberField("Right", "insetRight");
            }
            else
            {
                numberField("Pos X", "offsetX");
            }
            if (preview->m_isBelow)
            {
                numberField("Gap", "offsetY");
            }
            else if (isStretchedY)
            {
                numberField("Top", "insetTop");
                numberField("Bottom", "insetBottom");
            }
            else
            {
                numberField("Pos Y", "offsetY");
            }
        }
        else
        {
            ImGui::TextDisabled("The preview shows once the scene has been drawn.");
        }

        for (const FieldView &field : view.m_fields)
        {
            if (field.m_schema.m_name == "rotation" || field.m_schema.m_name == "scaleX" || field.m_schema.m_name == "scaleY")
            {
                fieldEditor(field);
            }
        }
        if (ImGui::TreeNode("Anchors and offsets as numbers"))
        {
            for (const FieldView &field : view.m_fields)
            {
                const std::string &name = field.m_schema.m_name;
                if (name != "rotation" && name != "scaleX" && name != "scaleY" && name != "enabled")
                {
                    fieldEditor(field);
                }
            }
            ImGui::TreePop();
        }
    }

    // The header of a transform or a component: its class with its icon, and for a component a cross at the end that asks for it to be removed.
    bool Panels::componentHeader(const fried::editor::ComponentView &component, bool *isRemoved)
    {
        bool isOpen = classHeader(component.m_type);
        if (component.m_ref.m_isTransform)
        {
            return isOpen;
        }
        ImVec2 low = ImGui::GetItemRectMin();
        ImVec2 high = ImGui::GetItemRectMax();
        float size = Icons::size();
        ImVec2 crossLow(high.x - size - scaled(8), low.y + (high.y - low.y - size) / 2);
        bool isHovered = ImGui::IsMouseHoveringRect(crossLow, ImVec2(crossLow.x + size, crossLow.y + size)) && ImGui::IsWindowHovered();
        Icons::draw(ImGui::GetWindowDrawList(), Icon::Remove, crossLow, IM_COL32(255, 255, 255, isHovered ? 230 : 90));
        if (isHovered)
        {
            ImGui::SetTooltip("Remove %s", component.m_type.c_str());
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                *isRemoved = true;
            }
        }
        return isOpen;
    }

    bool Panels::classHeader(const std::string &name)
    {
        // The header's own label stays empty so the icon and the name can be drawn after its arrow.
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, scaled(5)));
        bool isOpen = ImGui::TreeNodeEx("##class", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s", "");
        ImGui::PopStyleVar();
        ImVec2 low = ImGui::GetItemRectMin();
        ImVec2 high = ImGui::GetItemRectMax();
        ImDrawList *list = ImGui::GetWindowDrawList();
        float x = low.x + ImGui::GetTreeNodeToLabelSpacing() + ImGui::GetStyle().FramePadding.x;
        Icons::draw(list, iconOf(name), ImVec2(x, (low.y + high.y - Icons::size()) / 2));
        list->AddText(boldFont(), ImGui::GetFontSize(), ImVec2(x + Icons::size() + scaled(6), (low.y + high.y - ImGui::GetFontSize()) / 2), ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
        return isOpen;
    }

    void Panels::assetInspector()
    {
        using fried::editor::ImportSettings;
        std::string root, path;
        const bool isReference = fried::editor::parseAssetReference(m_assetSelected, &root, &path);
        const AssetKind kind = isReference ? fried::editor::assetKindOf(path) : AssetKind::Other;
        const std::string name = m_assetSelected.substr(m_assetSelected.rfind('/') + 1);
        Icon icon = kind == AssetKind::Image ? Icon::Image : kind == AssetKind::Audio ? Icon::AudioStreamWAV : kind == AssetKind::Font ? Icon::Font : kind == AssetKind::Model ? Icon::MeshInstance3D : kind == AssetKind::Scene ? Icon::PackedScene : Icon::File;
        Icons::draw(ImGui::GetWindowDrawList(), icon, ImGui::GetCursorScreenPos());
        ImGui::Dummy(ImVec2(Icons::size(), Icons::size()));
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(name.c_str());
        ImGui::Separator();

        if (kind == AssetKind::Font)
        {
            const std::string fontFile = m_editor.assetFile(root, path);
            fried::editor::FontInfo info = fried::editor::readFontInfo(fontFile);
            if (!std::filesystem::exists(fontFile))
            {
                ImGui::TextDisabled("The file is not there any more.");
                return;
            }
            ImGui::TextDisabled("%llu bytes", (unsigned long long)info.m_bytes);
            ImGui::Spacing();
            ImGui::TextWrapped("The DS and the calculator read the whole font file into memory, so building for them leaves out the tables the engine never reads (variation and glyph substitution), and every glyph draws the same.");
            if (info.m_isValid && info.m_bytesOnSoftwareText < info.m_bytes)
            {
                ImGui::Spacing();
                ImGui::Text("On those platforms: %llu bytes, %llu less.", (unsigned long long)info.m_bytesOnSoftwareText, (unsigned long long)(info.m_bytes - info.m_bytesOnSoftwareText));
            }
            else if (info.m_isValid)
            {
                ImGui::Spacing();
                ImGui::TextDisabled("There is nothing to leave out of this one.");
            }
            else
            {
                ImGui::Spacing();
                ImGui::TextDisabled("This is not a single TrueType font, so it is copied as it is.");
            }
            ImGui::Spacing();
            ImGui::TextDisabled("There is nothing to set: it is done on every build.");
            return;
        }
        if (kind != AssetKind::Image && kind != AssetKind::Audio)
        {
            ImGui::TextDisabled("This kind of file has no import settings.");
            return;
        }
        const std::string file = m_editor.assetFile(root, path);
        const bool isImage = kind == AssetKind::Image;
        const std::string group = isImage ? "image" : "audio";
        const bool isGame = root == "game";
        if (!std::filesystem::exists(file))
        {
            ImGui::TextDisabled("The file is not there any more.");
            return;
        }

        if (isImage)
        {
            fried::editor::ImageInfo info = fried::editor::readImageInfo(file);
            if (info.m_isValid)
            {
                ImGui::TextDisabled("%d x %d px, %llu bytes", info.m_width, info.m_height, (unsigned long long)info.m_bytes);
            }
        }
        else
        {
            fried::editor::SoundInfo info = fried::editor::readSoundInfo(file);
            if (info.m_isValid)
            {
                ImGui::TextDisabled("%d Hz, %s, %d bit, %.2f s, %llu bytes", info.m_sampleRate, info.m_channels == 1 ? "mono" : "stereo", info.m_bits, info.m_seconds, (unsigned long long)info.m_bytes);
            }
            else
            {
                ImGui::TextDisabled("Only PCM WAV files are converted.");
            }
        }
        if (!isGame)
        {
            ImGui::TextDisabled("The engine's assets are not changed from here.");
            return;
        }
        ImGui::Spacing();

        if (m_importFile != file)
        {
            m_importFile = file;
            m_importSettings = fried::editor::loadImportSettings(file);
        }

        struct Tab
        {
            std::string m_key;
            std::string m_label;
        };
        std::vector<Tab> tabs = {{"", "Default"}};
        std::vector<fried::core::Platform> declared = m_editor.project().declaredPlatforms();
        if (declared.empty())
        {
            declared.assign(fried::core::s_platforms.begin(), fried::core::s_platforms.end());
        }
        for (fried::core::Platform platform : declared)
        {
            const char *label = platform == fried::core::Platform::Pc ? "PC" : platform == fried::core::Platform::Vita ? "Vita" : platform == fried::core::Platform::Switch ? "Switch" : platform == fried::core::Platform::Nintendo3ds ? "3DS" : platform == fried::core::Platform::NintendoDs ? "DS" : platform == fried::core::Platform::Cg50 ? "CG50" : platform == fried::core::Platform::Android ? "Android" : "Web";
            tabs.push_back({fried::core::platformKey(platform), label});
        }

        bool isChanged = false;
        if (ImGui::BeginTabBar("##import-platform", ImGuiTabBarFlags_FittingPolicyScroll))
        {
            for (const Tab &tab : tabs)
            {
                if (!ImGui::BeginTabItem(tab.m_label.c_str()))
                {
                    continue;
                }
                const std::string &platform = tab.m_key;
                ImGui::PushID(tab.m_key.c_str());
                bool hasOverride = platform.empty() || fried::editor::hasImportOverride(m_importSettings, platform, group);
                if (!platform.empty())
                {
                    bool isOverriding = hasOverride;
                    if (ImGui::Checkbox(("Override for " + tab.m_label).c_str(), &isOverriding))
                    {
                        if (isOverriding)
                        {
                            fried::editor::startImportOverride(&m_importSettings, platform, group, fried::editor::importDefaults(group));
                        }
                        else
                        {
                            fried::editor::clearImportOverride(&m_importSettings, platform, group);
                        }
                        isChanged = true;
                        hasOverride = isOverriding;
                    }
                    if (!hasOverride)
                    {
                        ImGui::BeginDisabled();
                    }
                }

                const float labelColumn = ImGui::CalcTextSize("Sample rate").x + ImGui::GetStyle().ItemSpacing.x * 3;
                auto choice = [&](const char *label, const char *field, const std::vector<std::pair<nlohmann::json, std::string>> &options) {
                    nlohmann::json current = fried::editor::importValue(m_importSettings, platform, group, field, fried::editor::importDefaults(group)[field]);
                    std::string shown = current.dump();
                    for (const auto &option : options)
                    {
                        if (option.first == current)
                        {
                            shown = option.second;
                        }
                    }
                    // The names are on the left in a column as wide as the longest, and the selectors fill the rest.
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                    ImGui::SameLine(labelColumn);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::BeginCombo((std::string("##") + field).c_str(), shown.c_str()))
                    {
                        for (const auto &option : options)
                        {
                            if (ImGui::Selectable(option.second.c_str(), option.first == current))
                            {
                                fried::editor::setImportValue(&m_importSettings, platform, group, field, option.first);
                                isChanged = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                };

                if (isImage)
                {
                    choice("Max size", "maxSize",
                           {{0, "No limit"}, {32, "32"}, {64, "64"}, {128, "128"}, {256, "256"}, {512, "512"}, {1024, "1024"}, {2048, "2048"}, {4096, "4096"}});
                    choice("Filter", "filter", {{"default", "Project default"}, {"nearest", "Nearest"}, {"linear", "Linear"}});
                    choice("Format", "format", {{"rgba8", "RGBA 8888 (32 bit)"}, {"rgba4", "RGBA 4444 (16 bit)"}, {"rgb565", "RGB 565 (16 bit, no transparency)"}});
                }
                else
                {
                    choice("Channels", "mono", {{false, "Keep"}, {true, "Mix down to mono"}});
                    choice("Sample rate", "sampleRate",
                           {{0, "Keep"}, {8000, "8000 Hz"}, {11025, "11025 Hz"}, {16000, "16000 Hz"}, {22050, "22050 Hz"}, {32000, "32000 Hz"}, {44100, "44100 Hz"}, {48000, "48000 Hz"}});
                    choice("Bits", "bits", {{0, "Keep"}, {8, "8"}, {16, "16"}});
                }

                if (!platform.empty() && !hasOverride)
                {
                    ImGui::EndDisabled();
                }
                ImGui::PopID();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("Applied when the game is built for the platform. The editor shows the original file.");
        ImGui::PopStyleColor();

        if (isChanged)
        {
            std::string error;
            if (!fried::editor::saveImportSettings(file, m_importSettings, &error))
            {
                m_editor.setMessage(error);
            }
        }
    }

    namespace
    {
        // Where a component is left out because the platform uses another backend of its module, or one that only works in part there.
        struct LeftOut
        {
            std::string m_module;
            std::string m_backend;
            std::vector<fried::core::Platform> m_platforms;
            bool m_isEverywhere = false;
            bool m_isOnShown = false;
        };

        LeftOut leftOutOf(const fried::editor::Editor &editor, const std::string &type)
        {
            LeftOut result;
            const fried::editor::ClassSchema *owner = nullptr;
            std::string name = type;
            while (!name.empty() && owner == nullptr)
            {
                const fried::editor::ClassSchema *found = nullptr;
                for (const fried::editor::ClassSchema &schema : editor.classes())
                {
                    if (schema.m_name == name)
                    {
                        found = &schema;
                        break;
                    }
                }
                if (found == nullptr)
                {
                    break;
                }
                owner = found->m_module.empty() ? nullptr : found;
                name = found->m_parent;
            }
            if (owner == nullptr)
            {
                return result;
            }
            result.m_module = owner->m_module;
            result.m_backend = owner->m_backend;
            std::vector<fried::core::Platform> considered = editor.project().declaredPlatforms();
            if (considered.empty())
            {
                considered.assign(fried::core::s_platforms.begin(), fried::core::s_platforms.end());
            }
            for (fried::core::Platform platform : considered)
            {
                if (fried::editor::isLeftOut(editor.modules(), owner->m_module, owner->m_backend, editor.project(), platform))
                {
                    result.m_platforms.push_back(platform);
                }
            }
            result.m_isEverywhere = !result.m_platforms.empty() && result.m_platforms.size() == considered.size();
            result.m_isOnShown = fried::editor::isLeftOut(editor.modules(), owner->m_module, owner->m_backend, editor.project(), editor.platform());
            return result;
        }

        std::string platformList(const std::vector<fried::core::Platform> &platforms)
        {
            std::string text;
            for (fried::core::Platform platform : platforms)
            {
                text += (text.empty() ? "" : ", ") + fried::editor::platformLabel(platform);
            }
            return text;
        }
    }

    void Panels::inspector()
    {
        if (m_editor.selected() != m_lastInspectedNode)
        {
            m_lastInspectedNode = m_editor.selected();
            m_isAssetInspected = false;
        }
        if (m_isAssetInspected && !m_assetSelected.empty())
        {
            assetInspector();
            return;
        }
        if (!m_editor.selected())
        {
            ImGui::TextDisabled("Nothing selected");
            return;
        }

        std::string instance = m_editor.instanceOf(*m_editor.selected());
        if (!instance.empty())
        {
            Icons::draw(ImGui::GetWindowDrawList(), Icon::PackedScene, ImGui::GetCursorScreenPos());
            ImGui::Dummy(ImVec2(Icons::size(), Icons::size()));
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Instance of %s", instance.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Open"))
            {
                openSceneFile(m_editor.scenePathOf(instance));
            }
            ImGui::Separator();
        }

        const fried::editor::ObjectView &object = m_editor.inspector();
        m_component = fried::editor::ComponentRef();
        bool isActive = object.m_active.m_value.is_boolean() ? object.m_active.m_value.get<bool>() : true;
        if (ImGui::Checkbox("##object-active", &isActive))
        {
            m_editor.setActive(object.m_id, isActive);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Active: an inactive object and what is under it is neither updated nor drawn");
        }
        ImGui::SameLine();
        fieldEditor(object.m_name);
        ImGui::Separator();

        std::vector<fried::core::Platform> leaving;
        std::vector<LeftOut> leftOuts;
        for (const fried::editor::ComponentView &component : object.m_components)
        {
            leftOuts.push_back(leftOutOf(m_editor, component.m_type));
            for (fried::core::Platform platform : leftOuts.back().m_platforms)
            {
                if (std::find(leaving.begin(), leaving.end(), platform) == leaving.end())
                {
                    leaving.push_back(platform);
                }
            }
        }
        if (!leaving.empty())
        {
            std::sort(leaving.begin(), leaving.end());
            ImGui::PushStyleColor(ImGuiCol_Text, palette::warning);
            ImGui::TextWrapped("Some components are left out of the game on: %s.", platformList(leaving).c_str());
            ImGui::PopStyleColor();
        }

        std::vector<fried::core::Platform> considered = m_editor.project().declaredPlatforms();
        if (considered.empty())
        {
            considered.assign(fried::core::s_platforms.begin(), fried::core::s_platforms.end());
        }
        std::vector<fried::core::Platform> ignoring;
        for (const fried::editor::ComponentView &component : object.m_components)
        {
            for (const fried::editor::ClassView &view : component.m_classes)
            {
                for (const FieldView &field : view.m_fields)
                {
                    for (fried::core::Platform platform :
                         fried::editor::settingIgnoredOn(m_editor.modules(), m_editor.project(), view.m_class, field.m_schema.m_name, considered).m_platforms)
                    {
                        if (std::find(ignoring.begin(), ignoring.end(), platform) == ignoring.end())
                        {
                            ignoring.push_back(platform);
                        }
                    }
                }
            }
        }
        if (!ignoring.empty())
        {
            std::sort(ignoring.begin(), ignoring.end());
            ImGui::PushStyleColor(ImGuiCol_Text, palette::warning);
            ImGui::TextWrapped("Some settings are not used on: %s.", platformList(ignoring).c_str());
            ImGui::PopStyleColor();
        }
        if (!ignoring.empty() || m_isUnusedHidden)
        {
            ImGui::Checkbox("Hide unused settings", &m_isUnusedHidden);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Hides the settings the backend of the shown platform does not use. Components left out of a platform stay shown.");
            }
        }

        for (std::size_t position = 0; position < object.m_components.size(); ++position)
        {
            const fried::editor::ComponentView &component = object.m_components[position];
            const LeftOut &left = leftOuts[position];
            bool isGrey = left.m_isEverywhere || left.m_isOnShown;
            m_component = component.m_ref;
            ImGui::PushID((int)position);
            bool isRemoved = false;
            if (isGrey)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.55f);
            }
            bool isOpen = componentHeader(component, &isRemoved);
            if (isGrey)
            {
                ImGui::PopStyleVar();
            }
            if (!left.m_platforms.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
            {
                ImGui::SetTooltip("%s: %s is not used on %s, so this component is left out there.", left.m_module.c_str(), left.m_backend.c_str(), platformList(left.m_platforms).c_str());
            }
            if (isOpen && isGrey)
            {
                ImGui::BeginDisabled();
            }
            if (isOpen)
            {
                bool isRect = component.m_ref.m_isTransform && m_editor.isClassOf(component.m_type, "RectTransform");
                for (const fried::editor::ClassView &view : component.m_classes)
                {
                    if (isRect && view.m_class == "RectTransform")
                    {
                        controlEditor(view);
                        continue;
                    }
                    const std::vector<FieldView> &fields = view.m_fields;
                    for (std::size_t index = 0; index < fields.size(); ++index)
                    {
                        std::vector<const FieldView *> axes;
                        std::string label;
                        if (isAxisStart(fields, index, &axes, &label))
                        {
                            vectorEditor(label, axes);
                            index += axes.size() - 1;
                            continue;
                        }
                        fried::editor::IgnoredSetting ignored =
                            fried::editor::settingIgnoredOn(m_editor.modules(), m_editor.project(), view.m_class, fields[index].m_schema.m_name, considered);
                        bool isUnused = !ignored.m_platforms.empty()
                                        && (ignored.m_platforms.size() == considered.size()
                                            || std::find(ignored.m_platforms.begin(), ignored.m_platforms.end(), m_editor.platform()) != ignored.m_platforms.end());
                        if (isUnused && m_isUnusedHidden)
                        {
                            continue;
                        }
                        if (isUnused)
                        {
                            ImGui::BeginDisabled();
                        }
                        fieldEditor(fields[index]);
                        if (isUnused)
                        {
                            ImGui::EndDisabled();
                        }
                        if (!ignored.m_platforms.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                        {
                            ImGui::SetTooltip("%s: %s does not use this setting on %s.", ignored.m_module.c_str(), ignored.m_backend.c_str(),
                                              platformList(ignored.m_platforms).c_str());
                        }
                    }
                }
                ImGui::Dummy(ImVec2(0, scaled(2)));
            }
            if (isOpen && isGrey)
            {
                ImGui::EndDisabled();
            }
            ImGui::PopID();
            if (isRemoved)
            {
                m_editor.removeComponent(object.m_id, component.m_ref);
                break;
            }
        }
        m_component = fried::editor::ComponentRef();
        ImGui::Spacing();
        if (ImGui::Button("Add Component", ImVec2(-FLT_MIN, 0)))
        {
            showAddComponent(object.m_id);
        }
        if (object.m_instance.empty() && ImGui::Button("Change Transform Type...", ImVec2(-FLT_MIN, 0)))
        {
            showChangeType(object.m_id);
        }
    }

    // The rows of Ctrl+P's `>` and `@` forms: a label each, ranked by what follows the first character, and run by Enter or a click.
    void Panels::quickOpenList(bool isCommands)
    {
        std::vector<std::string> names;
        std::vector<std::function<void()>> actions;
        if (isCommands)
        {
            for (const PaletteCommand &command : m_paletteCommands)
            {
                if (!command.m_isEnabled || command.m_isEnabled())
                {
                    names.push_back(command.m_label);
                    actions.push_back(command.m_run);
                }
            }
        }
        else
        {
            std::vector<std::pair<const Json *, std::string>> pending;
            const Json &roots = m_editor.document().data()["objects"];
            for (const Json &root : roots)
            {
                pending.push_back({&root, ""});
            }
            while (!pending.empty())
            {
                auto [node, path] = pending.back();
                pending.pop_back();
                std::string name = node->value("name", node->value("id", std::string()));
                std::string id = node->value("id", std::string());
                names.push_back(path.empty() ? name : name + "  " + path);
                actions.push_back([this, id] { m_editor.select(id); });
                if (node->contains("children") && (*node)["children"].is_array())
                {
                    for (const Json &child : (*node)["children"])
                    {
                        pending.push_back({&child, path.empty() ? name : path + "/" + name});
                    }
                }
            }
        }
        std::vector<std::size_t> found = fried::editor::fuzzyRank(m_quickText.substr(1), names);
        int shown = (int)std::min<std::size_t>(found.size(), 12);
        if (shown > 0)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                m_quickIndex = (m_quickIndex + 1) % shown;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                m_quickIndex = (m_quickIndex + shown - 1) % shown;
            }
        }
        m_quickIndex = std::min(m_quickIndex, std::max(shown - 1, 0));
        ImGui::Separator();
        int chosen = -1;
        for (int row = 0; row < shown; ++row)
        {
            ImGui::PushID(row);
            if (ImGui::Selectable(names[found[row]].c_str(), row == m_quickIndex))
            {
                chosen = row;
            }
            ImGui::PopID();
        }
        if (shown == 0)
        {
            ImGui::TextDisabled("Nothing matches");
        }
        if (shown > 0 && ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            chosen = m_quickIndex;
        }
        if (chosen >= 0)
        {
            ImGui::CloseCurrentPopup();
            actions[found[chosen]]();
        }
    }

    void Panels::quickOpen()
    {
        if (m_isQuickOpenAsked)
        {
            m_isQuickOpenAsked = false;
            m_quickText.clear();
            m_quickIndex = 0;
            ImGui::OpenPopup("Quick open");
        }
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->GetCenter().x, viewport->Pos.y + viewport->Size.y * 0.18f), ImGuiCond_Always, ImVec2(0.5f, 0));
        ImGui::SetNextWindowSize(ImVec2(scaled(560), 0));
        ImGui::SetNextWindowViewport(viewport->ID);
        if (!ImGui::BeginPopup("Quick open", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
        {
            return;
        }

        // The scenes come first, the latest opened before the rest, then the other files.
        const std::vector<fried::editor::AssetEntry> &entries = m_editor.assets();
        std::vector<std::string> recent = m_editor.recentScenes();
        std::vector<const fried::editor::AssetEntry *> ordered;
        for (const std::string &path : recent)
        {
            for (const fried::editor::AssetEntry &entry : entries)
            {
                if (entry.m_kind == AssetKind::Scene && std::filesystem::path(m_editor.assetFile(entry.m_root, entry.m_path)) == std::filesystem::path(path))
                {
                    ordered.push_back(&entry);
                }
            }
        }
        for (int pass = 0; pass < 2; ++pass)
        {
            for (const fried::editor::AssetEntry &entry : entries)
            {
                bool isScene = entry.m_kind == AssetKind::Scene;
                if (isScene == (pass == 0) && std::find(ordered.begin(), ordered.end(), &entry) == ordered.end())
                {
                    ordered.push_back(&entry);
                }
            }
        }
        std::vector<std::string> names;
        for (const fried::editor::AssetEntry *entry : ordered)
        {
            names.push_back(entry->reference());
        }

        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputTextWithHint("##quick", "Open a scene or find an asset, > for a command, @ for an object", &m_quickText))
        {
            m_quickIndex = 0;
        }
        if (!m_quickText.empty() && (m_quickText[0] == '>' || m_quickText[0] == '@'))
        {
            quickOpenList(m_quickText[0] == '>');
            ImGui::EndPopup();
            return;
        }
        std::vector<std::size_t> found = fried::editor::fuzzyRank(m_quickText, names);
        int shown = (int)std::min<std::size_t>(found.size(), 12);
        if (shown > 0)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                m_quickIndex = (m_quickIndex + 1) % shown;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                m_quickIndex = (m_quickIndex + shown - 1) % shown;
            }
        }

        int chosen = -1;
        ImGui::Separator();
        for (int row = 0; row < shown; ++row)
        {
            const fried::editor::AssetEntry &entry = *ordered[found[row]];
            ImGui::PushID(row);
            ImVec2 position = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##row", row == m_quickIndex, ImGuiSelectableFlags_None, ImVec2(0, Icons::size() + scaled(6))))
            {
                chosen = row;
            }
            if (row == m_quickIndex && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow)))
            {
                ImGui::SetScrollHereY();
            }
            Icon icon = entry.m_kind == AssetKind::Image  ? Icon::Image
                        : entry.m_kind == AssetKind::Font  ? Icon::Font
                        : entry.m_kind == AssetKind::Audio ? Icon::AudioStreamWAV
                        : entry.m_kind == AssetKind::Model ? Icon::MeshInstance3D
                        : entry.m_kind == AssetKind::Scene ? Icon::PackedScene
                                                           : Icon::File;
            ImDrawList *list = ImGui::GetWindowDrawList();
            float centre = position.y + (Icons::size() + scaled(6)) / 2;
            Icons::draw(list, icon, ImVec2(position.x + scaled(4), centre - Icons::size() / 2));
            std::string name = entry.m_path.substr(entry.m_path.rfind('/') == std::string::npos ? 0 : entry.m_path.rfind('/') + 1);
            list->AddText(ImVec2(position.x + Icons::size() + scaled(12), centre - ImGui::GetFontSize() / 2), ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
            float folderX = position.x + Icons::size() + scaled(24) + ImGui::CalcTextSize(name.c_str()).x;
            list->AddText(ImVec2(folderX, centre - ImGui::GetFontSize() / 2), ImGui::GetColorU32(ImGuiCol_TextDisabled), fried::editor::assetFolderOf(entry).c_str());
            ImGui::PopID();
        }
        if (shown == 0)
        {
            ImGui::TextDisabled("Nothing matches");
        }
        if (shown > 0 && ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            chosen = m_quickIndex;
        }

        if (chosen >= 0)
        {
            const fried::editor::AssetEntry &entry = *ordered[found[chosen]];
            if (entry.m_kind == AssetKind::Scene)
            {
                openSceneFile(m_editor.assetFile(entry.m_root, entry.m_path));
            }
            else
            {
                enterAssetFolder(fried::editor::assetFolderOf(entry));
                m_assetFilter = 0;
                m_assetSearch.clear();
                ImGui::SetWindowFocus("Assets");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void Panels::enterAssetFolder(const std::string &folder)
    {
        m_assetFolder = folder;
        fried::editor::EditorState::setProjectValue(m_editor.project().directory(), "assetFolder", folder);
    }

    bool Panels::moveDroppedAsset(const std::string &folder)
    {
        bool isMoved = false;
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(s_assetPayload))
            {
                std::string reference((const char *)payload->Data, payload->DataSize - 1);
                std::string error;
                if (m_editor.moveAsset(reference, folder, std::string(), &error))
                {
                    isMoved = true;
                }
                else
                {
                    m_editor.setMessage(error);
                }
            }
            ImGui::EndDragDropTarget();
        }
        return isMoved;
    }

    // The windows the asset files ask for: a new name, a new folder and the confirmation of a delete.
    void Panels::assetQuestions()
    {
        auto folderOf = [](const std::string &reference) { return reference.substr(0, reference.rfind('/')); };
        auto text = [&](const char *label, std::string *value) {
            ImGui::SetNextItemWidth(scaled(260));
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }
            bool isEntered = ImGui::InputText(label, value, ImGuiInputTextFlags_EnterReturnsTrue);
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                ImGui::CloseCurrentPopup();
            }
            if (!m_assetQuestionError.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
                ImGui::TextWrapped("%s", m_assetQuestionError.c_str());
                ImGui::PopStyleColor();
            }
            return isEntered;
        };

        if (m_isAssetRenameAsked)
        {
            m_assetQuestionError.clear();
            ImGui::OpenPopup("asset-rename");
            m_isAssetRenameAsked = false;
        }
        if (ImGui::BeginPopup("asset-rename"))
        {
            ImGui::TextDisabled("New name");
            if (text("##asset-rename-text", &m_assetRenameText))
            {
                if (m_editor.moveAsset(m_assetRenameReference, folderOf(m_assetRenameReference), m_assetRenameText, &m_assetQuestionError))
                {
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }

        if (m_isNewScriptAsked)
        {
            m_assetQuestionError.clear();
            ImGui::OpenPopup("asset-new-script");
            m_isNewScriptAsked = false;
        }
        if (ImGui::BeginPopup("asset-new-script"))
        {
            ImGui::TextDisabled("New script in %s", m_newScriptFolder.c_str());
            bool isEntered = text("##asset-script-name", &m_newScriptName);
            ImGui::TextDisabled("Extends");
            ImGui::SetNextItemWidth(scaled(260));
            if (ImGui::BeginCombo("##asset-script-base", m_newScriptBase.c_str()))
            {
                for (const fried::editor::ClassSchema &schema : m_editor.classes())
                {
                    if (schema.m_kind != "component")
                    {
                        continue;
                    }
                    if (ImGui::Selectable(schema.m_name.c_str(), schema.m_name == m_newScriptBase))
                    {
                        m_newScriptBase = schema.m_name;
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::Button("Create") || isEntered)
            {
                if (m_editor.createScript(m_newScriptFolder, m_newScriptName, m_newScriptBase, &m_assetQuestionError))
                {
                    enterAssetFolder(m_newScriptFolder);
                    m_assetSelected = m_newScriptFolder + "/" + m_newScriptName + ".hx";
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }

        if (m_isNewFolderAsked)
        {
            m_assetQuestionError.clear();
            ImGui::OpenPopup("asset-new-folder");
            m_isNewFolderAsked = false;
        }
        if (ImGui::BeginPopup("asset-new-folder"))
        {
            ImGui::TextDisabled("Folder name");
            if (text("##asset-folder-text", &m_newFolderText))
            {
                if (m_editor.createAssetFolder(m_newFolderParent, m_newFolderText, &m_assetQuestionError))
                {
                    enterAssetFolder(m_newFolderParent + "/" + m_newFolderText);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }

        if (m_isAssetDeleteAsked)
        {
            ImGui::OpenPopup("Delete Asset##confirm");
            m_isAssetDeleteAsked = false;
        }
        if (ImGui::BeginPopupModal("Delete Asset##confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Delete %s?", m_assetDeleteReference.c_str());
            ImGui::TextDisabled("It goes to the system's trash, where it can be restored from.");
            if (!m_assetDeleteUsers.empty())
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "These scenes still use it:");
                for (const std::string &scene : m_assetDeleteUsers)
                {
                    ImGui::BulletText("%s", scene.c_str());
                }
            }
            ImGui::Spacing();
            if (ImGui::Button("Delete"))
            {
                std::string error;
                if (!m_editor.deleteAsset(m_assetDeleteReference, &error))
                {
                    m_editor.setMessage(error);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void Panels::assets()
    {
        static const char *filters[] = {"All Types", "Images", "Fonts", "Audio", "Models", "Scenes"};
        static const AssetKind kinds[] = {AssetKind::Other, AssetKind::Image, AssetKind::Font, AssetKind::Audio, AssetKind::Model, AssetKind::Scene};
        const std::vector<fried::editor::AssetEntry> &entries = m_editor.assets();

        // The folders are found again only when the files changed.
        std::size_t key = entries.size();
        for (const fried::editor::AssetEntry &entry : entries)
        {
            key = key * 31 + std::hash<std::string>()(entry.m_path);
        }
        const std::vector<std::string> &directories = m_editor.assetDirectories();
        for (const std::string &directory : directories)
        {
            key = key * 31 + std::hash<std::string>()(directory);
        }
        if (key != m_foldersKey || m_folders.empty())
        {
            m_foldersKey = key;
            // The folders that hold files, and the empty ones a folder made in the editor is until something is put in it.
            m_folders = fried::editor::assetFolders(entries);
            m_folders.insert(m_folders.end(), directories.begin(), directories.end());
            std::sort(m_folders.begin(), m_folders.end());
            m_folders.erase(std::unique(m_folders.begin(), m_folders.end()), m_folders.end());
        }
        if (!m_isAssetFolderLoaded)
        {
            m_isAssetFolderLoaded = true;
            Json remembered = fried::editor::EditorState::projectValue(m_editor.project().directory(), "assetFolder");
            m_assetFolder = remembered.is_string() ? remembered.get<std::string>() : std::string("game");
        }
        if (!m_assetFolder.empty() && !std::binary_search(m_folders.begin(), m_folders.end(), m_assetFolder))
        {
            m_assetFolder.clear();
        }

        // Where the view is, one name for each folder on the way, to go back to any of them.
        if (ImGui::Selectable("Assets", false, ImGuiSelectableFlags_None, ImVec2(ImGui::CalcTextSize("Assets").x, 0)))
        {
            enterAssetFolder("");
        }
        for (std::size_t start = 0; start < m_assetFolder.size();)
        {
            std::size_t end = m_assetFolder.find('/', start);
            end = end == std::string::npos ? m_assetFolder.size() : end;
            std::string name = m_assetFolder.substr(start, end - start);
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
            ImGui::PushID((int)start);
            if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_None, ImVec2(ImGui::CalcTextSize(name.c_str()).x, 0)))
            {
                enterAssetFolder(m_assetFolder.substr(0, end));
            }
            ImGui::PopID();
            start = end + 1;
        }

        // The search, the kind of file, the size of the thumbnails and the list switch sit at the right end of the same row.
        const ImGuiStyle &style = ImGui::GetStyle();
        float searchWidth = scaled(170);
        float kindWidth = scaled(96);
        float sliderWidth = scaled(80);
        float buttonSide = Icons::size() + style.FramePadding.y * 2;
        float controls = searchWidth + kindWidth + sliderWidth + buttonSide + style.ItemSpacing.x * 3;
        // A panel too narrow for the breadcrumb and the controls side by side puts the controls on a line of their own.
        if (ImGui::GetCursorPosX() + scaled(12) + controls + style.WindowPadding.x <= ImGui::GetWindowWidth())
        {
            ImGui::SameLine();
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX() + scaled(12), ImGui::GetWindowWidth() - controls - style.WindowPadding.x));
        }
        else
        {
            searchWidth = std::max(scaled(80), ImGui::GetWindowWidth() - style.WindowPadding.x * 2 - kindWidth - sliderWidth - buttonSide - style.ItemSpacing.x * 3);
        }
        ImGui::SetNextItemWidth(searchWidth);
        ImGui::InputTextWithHint("##asset-search", "Search", &m_assetSearch);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(kindWidth);
        if (ImGui::BeginCombo("##asset-kind", filters[m_assetFilter]))
        {
            for (int index = 0; index < 5; ++index)
            {
                if (ImGui::Selectable(filters[index], m_assetFilter == index))
                {
                    m_assetFilter = index;
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Show only one kind of file");
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(sliderWidth);
        ImGui::SliderFloat("##asset-size", &m_assetCell, 56, 160, "");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("The size of the thumbnails");
        }
        ImGui::SameLine();
        if (Icons::button("asset-options", Icon::FileList, "How the panel is laid out"))
        {
            ImGui::OpenPopup("asset-options");
        }
        if (ImGui::BeginPopup("asset-options"))
        {
            ImGui::TextDisabled("Folders");
            ImGui::PushID("columns");
            if (ImGui::RadioButton("Automatic", m_assetColumns == 0)) { m_assetColumns = 0; }
            if (ImGui::RadioButton("Tree beside the files", m_assetColumns == 1)) { m_assetColumns = 1; }
            if (ImGui::RadioButton("Files only", m_assetColumns == 2)) { m_assetColumns = 2; }
            ImGui::PopID();
            ImGui::Separator();
            ImGui::TextDisabled("Files");
            ImGui::PushID("view");
            if (ImGui::RadioButton("Automatic", m_assetFileView == 0)) { m_assetFileView = 0; }
            if (ImGui::RadioButton("Grid", m_assetFileView == 1)) { m_assetFileView = 1; }
            if (ImGui::RadioButton("List", m_assetFileView == 2)) { m_assetFileView = 2; }
            ImGui::PopID();
            ImGui::EndPopup();
        }
        ImGui::Separator();

        // A narrow panel has no room for two columns, so it shows the files as a list under the breadcrumb unless the person chose otherwise.
        const bool isWide = ImGui::GetWindowWidth() >= scaled(620);
        const bool hasTree = m_assetColumns == 1 || (m_assetColumns == 0 && isWide);
        bool isList = m_assetFileView == 2 || (m_assetFileView == 0 && !isWide);

        // The folders on the left, and what is in the one chosen on the right.
        ImVec2 available = ImGui::GetContentRegionAvail();
        if (hasTree)
        {
        ImGui::BeginChild("##asset-tree", ImVec2(scaled(m_assetTreeWidth), available.y), ImGuiChildFlags_None);
        }
        std::function<void(const std::string &)> folderNode = [&](const std::string &folder) {
            std::vector<std::string> children = fried::editor::childFolders(m_folders, folder);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
            flags |= children.empty() ? ImGuiTreeNodeFlags_Leaf : 0;
            flags |= folder == m_assetFolder ? ImGuiTreeNodeFlags_Selected : 0;
            flags |= folder.find('/') == std::string::npos ? ImGuiTreeNodeFlags_DefaultOpen : 0;
            ImGui::PushStyleColor(ImGuiCol_Header, palette::accentAt(0.22f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1, 1, 1, 0.07f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, scaled(3)));
            bool isOpen = ImGui::TreeNodeEx(folder.c_str(), flags, "%s", "");
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                enterAssetFolder(folder);
            }
            moveDroppedAsset(folder);
            ImVec2 low = ImGui::GetItemRectMin();
            ImVec2 high = ImGui::GetItemRectMax();
            float x = low.x + ImGui::GetTreeNodeToLabelSpacing();
            ImDrawList *list = ImGui::GetWindowDrawList();
            Icons::draw(list, Icon::Folder, ImVec2(x, (low.y + high.y - Icons::size()) / 2), ImGui::GetColorU32(palette::accentText));
            std::string name = folder.substr(folder.rfind('/') == std::string::npos ? 0 : folder.rfind('/') + 1);
            list->AddText(ImVec2(x + Icons::size() + scaled(6), (low.y + high.y - ImGui::GetFontSize()) / 2), ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
            if (isOpen)
            {
                for (const std::string &child : children)
                {
                    folderNode(child);
                }
                ImGui::TreePop();
            }
        };
        if (hasTree)
        {
            for (const std::string &root : fried::editor::childFolders(m_folders, ""))
            {
                folderNode(root);
            }
            ImGui::EndChild();

            ImGui::SameLine(0, 0);
            ImGui::InvisibleButton("##asset-split", ImVec2(scaled(5), available.y));
            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
            if (ImGui::IsItemActive())
            {
                m_assetTreeWidth = std::clamp(m_assetTreeWidth + ImGui::GetIO().MouseDelta.x / uiScale(), 100.0f, 400.0f);
            }
            ImGui::SameLine(0, 0);
        }

        // What the content shows: the folders in the one chosen and the files that pass the filter, or, with a search, every file below it that matches.
        struct Item
        {
            std::string m_folder;
            const fried::editor::AssetEntry *m_entry = nullptr;
            int m_depth = 0;
            bool m_isOpen = false;
            bool m_hasChildren = false;
        };
        std::vector<Item> items;
        bool isSearching = !m_assetSearch.empty();

        // With no room for the folder tree beside the files, the files are shown in one tree, as Godot's FileSystem does: each folder opens to its folders and its files.
        const bool isTree = !hasTree && !isSearching && m_assetFilter == 0;
        if (isTree)
        {
            isList = true;
            if (!m_isAssetOpenSet)
            {
                m_isAssetOpenSet = true;
                m_assetOpen.insert("game");
            }
            std::function<void(const std::string &, int)> walk = [&](const std::string &folder, int depth) {
                std::vector<std::string> children = fried::editor::childFolders(m_folders, folder);
                std::vector<const fried::editor::AssetEntry *> files;
                for (const fried::editor::AssetEntry &entry : entries)
                {
                    if (fried::editor::assetFolderOf(entry) == folder)
                    {
                        files.push_back(&entry);
                    }
                }
                bool isOpen = m_assetOpen.count(folder) > 0;
                items.push_back({folder, nullptr, depth, isOpen, !children.empty() || !files.empty()});
                if (isOpen)
                {
                    for (const std::string &child : children)
                    {
                        walk(child, depth + 1);
                    }
                    for (const fried::editor::AssetEntry *file : files)
                    {
                        items.push_back({std::string(), file, depth + 1, false, false});
                    }
                }
            };
            for (const std::string &root : fried::editor::childFolders(m_folders, ""))
            {
                walk(root, 0);
            }
        }
        else if (!isSearching && m_assetFilter == 0)
        {
            for (const std::string &folder : fried::editor::childFolders(m_folders, m_assetFolder))
            {
                items.push_back({folder, nullptr});
            }
        }
        for (const fried::editor::AssetEntry &entry : entries)
        {
            if (isTree)
            {
                break;
            }
            if (m_assetFilter != 0 && entry.m_kind != kinds[m_assetFilter])
            {
                continue;
            }
            // A kind or a search looks through everything below the folder, and without them only the folder's own files are shown.
            bool isShown = isSearching || m_assetFilter != 0 ? fried::editor::isUnderFolder(entry, m_assetFolder) && (!isSearching || containsIgnoringCase(entry.reference(), m_assetSearch))
                                                            : fried::editor::assetFolderOf(entry) == m_assetFolder;
            if (isShown)
            {
                items.push_back({std::string(), &entry});
            }
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild("##asset-content", ImVec2(0, available.y), ImGuiChildFlags_None);
        ImGui::PopStyleColor();
        if (items.empty())
        {
            ImGui::TextDisabled("%s", isSearching ? "No file matches" : "Nothing here");
        }
        const float cell = scaled(m_assetCell);
        const float box = cell - scaled(14);
        float width = ImGui::GetContentRegionAvail().x;
        int columns = std::max(1, (int)(width / (cell + ImGui::GetStyle().ItemSpacing.x)));
        int placed = 0;
        auto isEditableReference = [](const std::string &reference) {
            return reference.compare(0, 4, "src/") == 0 || (reference.compare(0, 5, "game/") == 0 && !(reference == "game/scenes" || reference.compare(0, 12, "game/scenes/") == 0));
        };
        auto isOwnedReference = [](const std::string &reference) { return reference == "game" || reference == "src" || reference.compare(0, 5, "game/") == 0 || reference.compare(0, 4, "src/") == 0; };

        for (const Item &item : items)
        {
            bool isFolder = item.m_entry == nullptr;
            std::string reference = isFolder ? item.m_folder : item.m_entry->reference();
            std::string name = reference.substr(reference.rfind('/') == std::string::npos ? 0 : reference.rfind('/') + 1);
            if (!isList && placed++ % columns != 0)
            {
                ImGui::SameLine();
            }

            ImGui::PushID(reference.c_str());
            bool isHovered = false;
            bool isScene = !isFolder && item.m_entry->m_kind == AssetKind::Scene;
            ImDrawList *list = ImGui::GetWindowDrawList();
            if (isList)
            {
                ImVec2 row = ImGui::GetCursorScreenPos();
                ImGui::Selectable("##row", reference == m_assetSelected || (!isFolder && isScene && isOpenScene(m_editor.assetFile(item.m_entry->m_root, item.m_entry->m_path), m_editor.scenePath())), ImGuiSelectableFlags_AllowDoubleClick);
                isHovered = ImGui::IsItemHovered();
                Icon icon = isFolder ? Icon::Folder
                            : item.m_entry->m_kind == AssetKind::Image ? Icon::Image
                            : item.m_entry->m_kind == AssetKind::Font  ? Icon::Font
                            : item.m_entry->m_kind == AssetKind::Audio ? Icon::AudioStreamWAV
                            : item.m_entry->m_kind == AssetKind::Model ? Icon::MeshInstance3D
                            : isScene                                  ? Icon::PackedScene
                                                                       : Icon::File;
                float centre = row.y + ImGui::GetItemRectSize().y / 2;
                float indent = isTree ? item.m_depth * scaled(16) + scaled(16) : 0;
                if (isTree && isFolder)
                {
                    float arrow = row.x + indent - scaled(10);
                    ImU32 arrowColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
                    float side = scaled(4);
                    if (item.m_hasChildren)
                    {
                        if (item.m_isOpen)
                        {
                            list->AddTriangleFilled(ImVec2(arrow - side, centre - side / 2), ImVec2(arrow + side, centre - side / 2), ImVec2(arrow, centre + side), arrowColor);
                        }
                        else
                        {
                            list->AddTriangleFilled(ImVec2(arrow - side / 2, centre - side), ImVec2(arrow - side / 2, centre + side), ImVec2(arrow + side, centre), arrowColor);
                        }
                    }
                    if (ImGui::IsItemClicked(0))
                    {
                        if (item.m_isOpen)
                        {
                            m_assetOpen.erase(item.m_folder);
                        }
                        else
                        {
                            m_assetOpen.insert(item.m_folder);
                        }
                        m_assetFolder = item.m_folder;
                    }
                }
                Icons::draw(list, icon, ImVec2(row.x + scaled(4) + indent, centre - Icons::size() / 2), ImGui::GetColorU32(isFolder ? palette::accentText : palette::white));
                list->AddText(ImVec2(row.x + Icons::size() + scaled(12) + indent, centre - ImGui::GetFontSize() / 2), ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
                if (isSearching && !isFolder)
                {
                    float x = row.x + Icons::size() + scaled(24) + ImGui::CalcTextSize(name.c_str()).x;
                    list->AddText(ImVec2(x, centre - ImGui::GetFontSize() / 2), ImGui::GetColorU32(ImGuiCol_TextDisabled), reference.c_str());
                }
            }
            else
            {
                ImGui::BeginGroup();
                ImVec2 origin = ImGui::GetCursorScreenPos();
                ImGui::Dummy(ImVec2(cell, box));
                const Texture *thumbnail = !isFolder && item.m_entry->m_kind == AssetKind::Image ? m_images.get(m_editor.assetFile(item.m_entry->m_root, item.m_entry->m_path)) : nullptr;
                if (thumbnail)
                {
                    float ratio = std::min(box / thumbnail->width(), box / thumbnail->height());
                    ImVec2 size(thumbnail->width() * ratio, thumbnail->height() * ratio);
                    ImVec2 low(origin.x + (cell - size.x) / 2, origin.y + (box - size.y) / 2);
                    list->AddImage(thumbnail->id(), low, ImVec2(low.x + size.x, low.y + size.y));
                }
                else
                {
                    ImVec2 low(origin.x + (cell - box) / 2, origin.y);
                    list->AddRectFilled(low, ImVec2(low.x + box, low.y + box), IM_COL32(255, 255, 255, 10), 4.0f);
                    float inner = box * 0.8f;
                    ImVec2 iconLow(low.x + (box - inner) / 2, low.y + (box - inner) / 2);
                    if (isFolder)
                    {
                        Icons::drawBig(list, BigIcon::FolderBigThumb, iconLow, inner, ImGui::GetColorU32(palette::accentText));
                    }
                    else
                    {
                        AssetKind kind = item.m_entry->m_kind;
                        Icon icon = kind == AssetKind::Font ? Icon::Font : kind == AssetKind::Audio ? Icon::AudioStreamWAV : kind == AssetKind::Model ? Icon::MeshInstance3D : kind == AssetKind::Scene ? Icon::PackedScene : kind == AssetKind::Image ? Icon::Image : Icon::File;
                        Icons::drawBig(list, BigIcon::FileBigThumb, iconLow, inner, IM_COL32(150, 150, 160, 255));
                        Icons::draw(list, icon, ImVec2(low.x + (box - Icons::size()) / 2, low.y + (box - Icons::size()) / 2 + inner * 0.08f));
                    }
                }
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cell);
                ImGui::TextUnformatted(name.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndGroup();
                // The cell is covered by a button so it has an item of its own: it takes the click and the drag, and the context menu and the double click read it.
                ImVec2 cellLow = ImGui::GetItemRectMin();
                ImVec2 cellSize = ImGui::GetItemRectSize();
                ImGui::SetCursorScreenPos(cellLow);
                ImGui::InvisibleButton("##cell", cellSize);
                isHovered = ImGui::IsItemHovered();
                if (reference == m_assetSelected)
                {
                    list->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(palette::accentAt(0.3f)), 3.0f);
                }
                if (isScene && isOpenScene(m_editor.assetFile(item.m_entry->m_root, item.m_entry->m_path), m_editor.scenePath()))
                {
                    // The scene that is being edited.
                    list->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGuiCol_CheckMark), 3.0f, 0, 2.0f);
                }
            }

            if (isHovered)
            {
                ImGui::SetTooltip("%s", reference.c_str());
            }
            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload(s_assetPayload, reference.c_str(), reference.size() + 1);
                ImGui::TextUnformatted(name.c_str());
                ImGui::EndDragDropSource();
            }
            if (isFolder && moveDroppedAsset(item.m_folder))
            {
                ImGui::PopID();
                break;
            }
            const bool isEditable = isEditableReference(reference);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
            {
                m_assetSelected = reference;
                m_isAssetInspected = true;
            }

            // A double click does what suits the file: a folder is entered, a scene opens, and an image or a font is added to the scene.
            const bool isScript = !isFolder && item.m_entry->m_kind == AssetKind::Script;
            bool isUsable = isFolder || isScene || isScript || item.m_entry->m_kind == AssetKind::Image || item.m_entry->m_kind == AssetKind::Font;
            bool isActivated = isUsable && isHovered && ImGui::IsMouseDoubleClicked(0);
            if (ImGui::BeginPopupContextItem("asset-menu"))
            {
                if (isUsable && ImGui::MenuItem(isFolder ? "Open Folder" : isScene ? "Open Scene" : isScript ? "Open Script" : "Add to Scene"))
                {
                    isActivated = true;
                }
                if (isScene && ImGui::MenuItem("Instance in Scene"))
                {
                    m_editor.dropAsset(item.m_entry->m_root, item.m_entry->m_path, 0, 0);
                }
                if (ImGui::MenuItem("Copy Reference"))
                {
                    ImGui::SetClipboardText(reference.c_str());
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Rename...", shortcutText(Shortcut::Rename), false, isEditable))
                {
                    m_assetRenameReference = reference;
                    m_assetRenameText = name;
                    m_isAssetRenameAsked = true;
                }
                if (ImGui::MenuItem("Duplicate", shortcutText(Shortcut::Duplicate), false, isEditable))
                {
                    std::string error;
                    if (!m_editor.duplicateAsset(reference, &error))
                    {
                        m_editor.setMessage(error);
                    }
                }
                if (ImGui::MenuItem("Delete", shortcutText(Shortcut::Delete), false, isEditable))
                {
                    m_assetDeleteReference = reference;
                    m_assetDeleteUsers = m_editor.assetUsers(reference);
                    m_isAssetDeleteAsked = true;
                }
                if (ImGui::MenuItem("New Folder...", nullptr, false, isOwnedReference(reference)))
                {
                    m_newFolderParent = isFolder ? reference : reference.substr(0, reference.rfind('/'));
                    m_newFolderText = "New Folder";
                    m_isNewFolderAsked = true;
                }
                if (ImGui::MenuItem("New Script..."))
                {
                    showNewScript(isFolder ? reference : reference.substr(0, reference.rfind('/')));
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Show in Folder"))
                {
                    std::string file;
                    if (isFolder)
                    {
                        std::size_t slash = reference.find('/');
                        file = m_editor.assetFile(reference.substr(0, slash), slash == std::string::npos ? std::string() : reference.substr(slash + 1));
                    }
                    else
                    {
                        file = std::filesystem::path(m_editor.assetFile(item.m_entry->m_root, item.m_entry->m_path)).parent_path().string();
                    }
                    SDL_OpenURL(("file://" + file).c_str());
                }
                ImGui::EndPopup();
            }
            if (isActivated)
            {
                if (isFolder)
                {
                    if (!isTree)
                    {
                        enterAssetFolder(item.m_folder);
                        ImGui::PopID();
                        break;
                    }
                }
                else if (isScene)
                {
                    openSceneFile(m_editor.assetFile(item.m_entry->m_root, item.m_entry->m_path));
                }
                else if (isScript)
                {
                    // The folder of the project opens with the script in it, as Unity does, and a computer with no VS Code gives the file to the system.
                    std::string file = m_editor.assetFile(item.m_entry->m_root, item.m_entry->m_path);
                    if (!fried::editor::openInVsCode(m_editor.project().directory(), file))
                    {
                        SDL_OpenURL(("file://" + file).c_str());
                    }
                }
                else
                {
                    m_editor.dropAsset(item.m_entry->m_root, item.m_entry->m_path, m_editor.viewport().frameWidth() / 2.0, m_editor.viewport().frameHeight() / 2.0);
                }
            }
            ImGui::PopID();
        }
        if (ImGui::BeginPopupContextWindow("asset-area", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            const bool isOwnedFolder = m_assetFolder.compare(0, 4, "game") == 0 || m_assetFolder.compare(0, 3, "src") == 0;
            if (ImGui::MenuItem("New Folder...", nullptr, false, isOwnedFolder))
            {
                m_newFolderParent = m_assetFolder;
                m_newFolderText = "New Folder";
                m_isNewFolderAsked = true;
            }
            if (ImGui::MenuItem("New Script..."))
            {
                showNewScript(m_assetFolder);
            }
            ImGui::EndPopup();
        }
        // F2, Delete and Ctrl+D act on the chosen file while the panel has the keyboard and no text is being typed, as they do on nodes in the hierarchy.
        const bool hasAssetKeys = !m_assetSelected.empty() && isEditableReference(m_assetSelected) && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput;
        if (hasAssetKeys && isShortcutPressed(Shortcut::Rename))
        {
            m_assetRenameReference = m_assetSelected;
            m_assetRenameText = m_assetSelected.substr(m_assetSelected.rfind('/') + 1);
            m_isAssetRenameAsked = true;
        }
        else if (hasAssetKeys && isShortcutPressed(Shortcut::Delete))
        {
            m_assetDeleteReference = m_assetSelected;
            m_assetDeleteUsers = m_editor.assetUsers(m_assetSelected);
            m_isAssetDeleteAsked = true;
        }
        else if (hasAssetKeys && isShortcutPressed(Shortcut::Duplicate))
        {
            std::string error;
            if (!m_editor.duplicateAsset(m_assetSelected, &error))
            {
                m_editor.setMessage(error);
            }
        }
        assetQuestions();
        ImGui::EndChild();
    }

    void Panels::showNewScript(const std::string &folder)
    {
        // Scenes find the components that are written under src/components, so that is where a script goes.
        m_newScriptFolder = folder == "src/components" || folder.compare(0, 15, "src/components/") == 0 ? folder : "src/components";
        m_newScriptName = "NewComponent";
        m_newScriptBase = "Component";
        m_isNewScriptAsked = true;
    }

    void Panels::openSceneFile(const std::string &path)
    {
        std::string error;
        if (!m_editor.openScene(path, &error))
        {
            m_editor.setMessage(error);
        }
    }

    void Panels::closeSceneTab(const std::string &path)
    {
        std::string error;
        if (!m_editor.isSceneModified(path))
        {
            if (!m_editor.closeScene(path, &error))
            {
                m_editor.setMessage(error);
            }
            return;
        }
        m_pendingScene = path;
        m_isSceneChangeAsked = true;
    }

    void Panels::showCreateNode(const std::optional<std::string> &parent)
    {
        m_isCreateNodeAsked = true;
        m_isChangingType = false;
        m_isAddingComponent = false;
        m_createParent = parent;
    }

    void Panels::showChangeType(const std::string &id)
    {
        m_isCreateNodeAsked = true;
        m_isChangingType = true;
        m_isAddingComponent = false;
        m_changeTypeId = id;
    }

    void Panels::showAddComponent(const std::string &id)
    {
        m_isCreateNodeAsked = true;
        m_isChangingType = false;
        m_isAddingComponent = true;
        m_addComponentId = id;
    }

    void Panels::createNodeDialog()
    {
        if (m_isCreateNodeAsked)
        {
            m_isCreateNodeAsked = false;
            m_createFilter.clear();
            m_createChosen = m_isChangingType ? m_editor.document().transformTypeOf(m_changeTypeId) : m_isAddingComponent ? std::string() : std::string("Empty");
            ImGui::OpenPopup("Create New Object");
        }
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(scaled(520), scaled(560)), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("Create New Object", nullptr, ImGuiWindowFlags_NoSavedSettings))
        {
            return;
        }
        // What can be chosen: the transforms when one is changed, and else the components that fit, with the empty objects first when an object is made.
        static const std::vector<std::pair<std::string, std::string>> s_empties = {{"Empty", ""}, {"Empty 2D", "Transform2D"}, {"Empty 3D", "Transform3D"}, {"Empty UI", "RectTransform"}};
        auto isAbstract = [](const std::string &name) { return name == "Component" || name == "Renderer2D" || name == "Transform"; };
        std::vector<fried::editor::ClassSchema> classes;
        for (const fried::editor::ClassSchema &schema : m_editor.classes())
        {
            bool isFit = m_isChangingType ? schema.m_kind == "transform" || schema.m_name == "Transform"
                                          : schema.m_kind == "component" && !isAbstract(schema.m_name) && (!m_isAddingComponent || m_editor.canHaveComponent(m_addComponentId, schema.m_name));
            if (isFit)
            {
                fried::editor::ClassSchema entry = schema;
                entry.m_parent.clear();
                classes.push_back(std::move(entry));
            }
        }
        bool isCreating = false;
        if (m_isChangingType)
        {
            ImGui::TextDisabled("Change the transform of %s", m_changeTypeId.c_str());
        }
        else if (m_isAddingComponent)
        {
            ImGui::TextDisabled("Add a component to %s", m_addComponentId.c_str());
        }
        else
        {
            ImGui::TextDisabled(m_createParent ? "Create an object under %s" : "Create an object in the scene", m_createParent ? m_createParent->c_str() : "");
        }
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##create-filter", m_isChangingType ? "Search transforms" : m_isAddingComponent ? "Search components" : "Search components", &m_createFilter);

        ImGui::BeginChild("##create-classes", ImVec2(0, -scaled(38)), ImGuiChildFlags_Borders);
        const bool isFiltering = !m_createFilter.empty();
        std::vector<std::string> listed;
        // A component whose backend no platform of the project uses is listed and cannot be added.
        auto blockedReason = [&](const std::string &name) {
            LeftOut left = leftOutOf(m_editor, name);
            return left.m_isEverywhere ? "Needs " + left.m_module + ": " + left.m_backend + " on some platform. Choose it in Player Settings." : std::string();
        };
        auto row = [&](const std::string &name, bool isLeaf, const std::string &note) {
            std::string blocked = blockedReason(name);
            listed.push_back(name);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DefaultOpen;
            flags |= isLeaf ? ImGuiTreeNodeFlags_Leaf : 0;
            flags |= name == m_createChosen ? ImGuiTreeNodeFlags_Selected : 0;
            ImGui::PushStyleColor(ImGuiCol_Header, palette::accentAt(0.45f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1, 1, 1, 0.07f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, scaled(3)));
            bool isOpen = ImGui::TreeNodeEx(name.c_str(), flags, "%s", "");
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            if (blocked.empty() && ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                m_createChosen = name;
            }
            if (blocked.empty() && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_createChosen = name;
                isCreating = true;
            }
            if (!blocked.empty() && ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", blocked.c_str());
            }
            if (m_isCreateScrollAsked && name == m_createChosen)
            {
                ImGui::SetScrollHereY();
                m_isCreateScrollAsked = false;
            }
            ImVec2 low = ImGui::GetItemRectMin();
            ImVec2 high = ImGui::GetItemRectMax();
            float x = low.x + ImGui::GetTreeNodeToLabelSpacing();
            ImDrawList *list = ImGui::GetWindowDrawList();
            Icons::draw(list, iconOf(name), ImVec2(x, (low.y + high.y - Icons::size()) / 2));
            float textX = x + Icons::size() + scaled(6);
            list->AddText(ImVec2(textX, (low.y + high.y - ImGui::GetFontSize()) / 2), ImGui::GetColorU32(blocked.empty() ? ImGuiCol_Text : ImGuiCol_TextDisabled), name.c_str());
            if (!note.empty())
            {
                list->AddText(ImVec2(textX + ImGui::CalcTextSize(name.c_str()).x + scaled(8), (low.y + high.y - ImGui::GetFontSize()) / 2), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                              note.c_str());
            }
            return isOpen;
        };
        if (isFiltering)
        {
            for (const fried::editor::ClassSchema &schema : classes)
            {
                if (containsIgnoringCase(schema.m_name, m_createFilter))
                {
                    if (row(schema.m_name, true, std::string()))
                    {
                        ImGui::TreePop();
                    }
                }
            }
        }
        else
        {
            if (!m_isChangingType && !m_isAddingComponent)
            {
                for (const auto &empty : s_empties)
                {
                    if (row(empty.first, true, empty.second.empty() ? "a plain transform" : empty.second))
                    {
                        ImGui::TreePop();
                    }
                }
            }
            if (m_isChangingType && row("Transform", true, "no position"))
            {
                ImGui::TreePop();
            }
            for (const fried::editor::ClassSchema &schema : classes)
            {
                if (m_isChangingType && schema.m_name == "Transform")
                {
                    continue;
                }
                if (row(schema.m_name, true, std::string()))
                {
                    ImGui::TreePop();
                }
            }
        }
        ImGui::EndChild();

        // The arrows move the choice through the rows that are showing, and Enter creates what is chosen.
        bool isDown = ImGui::IsKeyPressed(ImGuiKey_DownArrow);
        if ((isDown || ImGui::IsKeyPressed(ImGuiKey_UpArrow)) && !listed.empty())
        {
            auto at = std::find(listed.begin(), listed.end(), m_createChosen);
            std::size_t index = at == listed.end() ? (isDown ? 0 : listed.size() - 1) : (std::size_t)(at - listed.begin());
            if (at != listed.end())
            {
                index = isDown ? std::min(index + 1, listed.size() - 1) : (index == 0 ? 0 : index - 1);
            }
            m_createChosen = listed[index];
            m_isCreateScrollAsked = true;
        }

        bool isEmpty = !m_isChangingType && !m_isAddingComponent && std::any_of(s_empties.begin(), s_empties.end(), [&](const auto &empty) { return empty.first == m_createChosen; });
        bool isKnown = isEmpty || (m_isChangingType && m_createChosen == "Transform")
                       || std::any_of(classes.begin(), classes.end(), [&](const fried::editor::ClassSchema &schema) { return schema.m_name == m_createChosen; });
        isKnown = isKnown && blockedReason(m_createChosen).empty();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", isKnown ? m_createChosen.c_str() : "Nothing chosen");
        float buttons = scaled(170);
        ImGui::SameLine(ImGui::GetWindowWidth() - buttons - ImGui::GetStyle().WindowPadding.x);
        ImGui::BeginDisabled(!isKnown);
        if (ImGui::Button(m_isChangingType ? "Change" : m_isAddingComponent ? "Add" : "Create", ImVec2(scaled(80), 0)) || (isKnown && ImGui::IsKeyPressed(ImGuiKey_Enter)))
        {
            isCreating = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        bool isCancelled = ImGui::Button("Cancel", ImVec2(scaled(80), 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape);
        if (isCreating && isKnown)
        {
            if (m_isChangingType)
            {
                m_editor.changeTransformType(m_changeTypeId, m_createChosen == "Transform" ? std::string() : m_createChosen);
            }
            else if (m_isAddingComponent)
            {
                m_editor.addComponent(m_addComponentId, m_createChosen);
            }
            else if (isEmpty)
            {
                auto empty = std::find_if(s_empties.begin(), s_empties.end(), [&](const auto &entry) { return entry.first == m_createChosen; });
                m_editor.addObject(empty->second, std::string(), m_createParent);
            }
            else
            {
                m_editor.addObject(std::string(), m_createChosen, m_createParent);
            }
            ImGui::CloseCurrentPopup();
        }
        else if (isCancelled)
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void Panels::newSceneDialog()
    {
        if (m_isNewSceneAsked)
        {
            m_newSceneName = "NewScene";
            m_newSceneType = "Transform2D";
            m_newSceneError.clear();
            ImGui::OpenPopup("New Scene");
            m_isNewSceneAsked = false;
        }
        if (!ImGui::BeginPopupModal("New Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        ImGui::TextDisabled("Root object");
        const struct
        {
            const char *m_label;
            const char *m_type;
        } presets[] = {{"2D Scene", "Transform2D"}, {"3D Scene", "Transform3D"}, {"User Interface", "RectTransform"}, {"Empty", "Transform"}};
        bool isPreset = false;
        for (const auto &preset : presets)
        {
            bool isChosen = m_newSceneType == preset.m_type;
            isPreset = isPreset || isChosen;
            if (isChosen)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, palette::accentAt(0.45f));
            }
            if (ImGui::Button(preset.m_label))
            {
                m_newSceneType = preset.m_type;
            }
            if (isChosen)
            {
                ImGui::PopStyleColor();
            }
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::Spacing();
        ImGui::TextDisabled("Name");
        ImGui::SetNextItemWidth(scaled(320));
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        bool isEntered = ImGui::InputText("##new-scene-name", &m_newSceneName, ImGuiInputTextFlags_EnterReturnsTrue);
        if (!m_newSceneError.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
            ImGui::TextWrapped("%s", m_newSceneError.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
        if (ImGui::Button("Create") || isEntered)
        {
            if (m_editor.newScene(m_newSceneName, m_newSceneType, &m_newSceneError))
            {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void Panels::saveSceneAsDialog()
    {
        if (m_isSaveAsAsked)
        {
            std::string name = std::filesystem::path(m_editor.scenePath()).filename().string();
            if (fried::core::isSceneFile(name))
            {
                name = fried::core::sceneName(name);
            }
            m_saveAsName = name;
            m_saveAsError.clear();
            ImGui::OpenPopup("Save Scene As");
            m_isSaveAsAsked = false;
        }
        if (!ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        ImGui::TextDisabled("Name");
        ImGui::SetNextItemWidth(scaled(320));
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        bool isEntered = ImGui::InputText("##save-as-name", &m_saveAsName, ImGuiInputTextFlags_EnterReturnsTrue);
        if (!m_saveAsError.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
            ImGui::TextWrapped("%s", m_saveAsError.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
        if (ImGui::Button("Save") || isEntered)
        {
            if (m_editor.saveSceneAs(m_saveAsName, &m_saveAsError))
            {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void Panels::yamlMigrationDialog()
    {
        if (!m_isMigrationChecked)
        {
            m_isMigrationChecked = true;
            if (m_editor.isYamlMigrationOffered())
            {
                ImGui::OpenPopup("Convert Project to YAML");
            }
        }
        if (!ImGui::BeginPopupModal("Convert Project to YAML", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        if (m_migrationReport.empty())
        {
            ImGui::PushTextWrapPos(scaled(460));
            ImGui::TextWrapped("The project file, scenes and import settings of this project are written in JSON. The engine reads YAML, which has comments and is easier to edit by hand, and a JSON file still works until it is converted. The originals are copied to .fried/json-backup first.");
            ImGui::PopTextWrapPos();
            ImGui::Spacing();
            if (ImGui::Button("Convert"))
            {
                m_editor.migrateToYaml(&m_migrationReport);
            }
            ImGui::SameLine();
            if (ImGui::Button("Not Now") || ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                m_editor.declineYamlMigration();
                ImGui::CloseCurrentPopup();
            }
        }
        else
        {
            ImGui::PushTextWrapPos(scaled(460));
            ImGui::TextWrapped("%s", m_migrationReport.c_str());
            ImGui::PopTextWrapPos();
            ImGui::Spacing();
            if (ImGui::Button("Close"))
            {
                m_migrationReport.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    void Panels::askPlayChanges()
    {
        if (m_editor.hasPlayChanges())
        {
            m_playChangeKeep.assign(m_editor.playChangeLabels().size(), true);
            m_isPlayChangesAsked = true;
        }
    }

    void Panels::playChangesDialog()
    {
        if (m_isPlayChangesAsked)
        {
            ImGui::OpenPopup("Play Mode Changes##play");
            m_isPlayChangesAsked = false;
        }
        if (!ImGui::BeginPopupModal("Play Mode Changes##play", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        ImGui::TextDisabled("The scene went back to what it was. Keep these edits made while the game ran?");
        ImGui::Spacing();
        std::vector<std::string> labels = m_editor.playChangeLabels();
        m_playChangeKeep.resize(labels.size(), true);
        for (std::size_t step = 0; step < labels.size(); ++step)
        {
            bool isKept = m_playChangeKeep[step];
            ImGui::PushID((int)step);
            if (ImGui::Checkbox(labels[step].c_str(), &isKept))
            {
                m_playChangeKeep[step] = isKept;
            }
            ImGui::PopID();
        }
        ImGui::Spacing();
        if (ImGui::Button("Keep Selected"))
        {
            int failed = m_editor.keepPlayChanges(m_playChangeKeep);
            if (failed > 0)
            {
                m_editor.setMessage(std::to_string(failed) + (failed == 1 ? " edit" : " edits") + " could not be made again");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard All") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            m_editor.discardPlayChanges();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void Panels::undoHistory()
    {
        fried::editor::CommandStack &stack = m_editor.commands();
        if (stack.stepCount() == stack.firstStep())
        {
            ImGui::TextDisabled("Nothing has been edited.");
            return;
        }
        // The state the scene was opened in, then every step, the one done last marked. A click goes to that state, undoing or redoing what is between.
        if (ImGui::Selectable("Original", stack.position() == stack.firstStep()))
        {
            stack.jumpTo(stack.firstStep());
        }
        for (std::size_t step = stack.firstStep(); step < stack.stepCount(); ++step)
        {
            ImGui::PushID((int)step);
            bool isDone = step < stack.position();
            if (!isDone)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            }
            if (ImGui::Selectable(stack.stepLabel(step).c_str(), step + 1 == stack.position()))
            {
                stack.jumpTo(step + 1);
            }
            if (!isDone)
            {
                ImGui::PopStyleColor();
            }
            ImGui::PopID();
        }
    }

    void Panels::sceneChangeDialog()
    {
        if (m_isSceneChangeAsked)
        {
            ImGui::OpenPopup("Unsaved Changes##scene");
            m_isSceneChangeAsked = false;
        }
        if (!ImGui::BeginPopupModal("Unsaved Changes##scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        ImGui::Text("Save %s before closing it?", std::filesystem::path(m_pendingScene).filename().string().c_str());
        ImGui::Spacing();
        bool isClosing = false;
        std::string error;
        if (ImGui::Button("Save"))
        {
            isClosing = m_editor.saveScene(m_pendingScene, &error);
            if (!isClosing)
            {
                m_editor.setMessage(error);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
        {
            isClosing = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        if (isClosing && !m_editor.closeScene(m_pendingScene, &error))
        {
            m_editor.setMessage(error);
        }
    }
}

namespace editor_ui
{
    // Reading every asset is not free, so the list is made again when the platform changes, after a fix, and every few seconds.
    void Panels::refreshIssues(bool isForced)
    {
        double now = ImGui::GetTime();
        bool isStale = now - m_issuesCheckedAt > 3.0 || m_issuesPlatform != m_editor.platform();
        if (isForced || isStale)
        {
            m_issues = m_editor.issues();
            m_sceneIssues = m_editor.sceneIssues();
            m_inputIssues = m_editor.inputIssues();
            m_issuesCheckedAt = now;
            m_issuesPlatform = m_editor.platform();
        }
    }

    void Panels::issues()
    {
        refreshIssues(ImGui::Button("Check Again"));
        ImGui::SameLine();
        ImGui::TextDisabled("for %s", fried::editor::platformLabel(m_editor.platform()).c_str());

        bool hasFixes = false;
        for (const auto &issue : m_issues)
        {
            hasFixes = hasFixes || issue.m_fix != fried::editor::AssetIssue::Fix::None;
        }
        if (hasFixes)
        {
            ImGui::SameLine();
            if (ImGui::Button("Fix All"))
            {
                for (const auto &issue : m_issues)
                {
                    std::string error;
                    if (issue.m_fix != fried::editor::AssetIssue::Fix::None && !m_editor.fixIssue(issue, &error))
                    {
                        m_issuesMessage = error;
                    }
                }
                m_issuesCheckedAt = -100;
            }
        }
        ImGui::Separator();

        if (m_issues.empty() && m_sceneIssues.empty() && m_inputIssues.empty())
        {
            ImGui::TextDisabled("The assets suit this platform.");
        }
        for (std::size_t position = 0; position < m_inputIssues.size(); ++position)
        {
            ImGui::PushID((int)position + 200000);
            ImGui::TextColored(palette::tomato, "Error");
            ImGui::SameLine();
            ImGui::TextUnformatted("Input Map");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", m_inputIssues[position].c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Open Input Map"))
            {
                m_isInputMapAsked = true;
            }
            ImGui::PopID();
        }
        for (std::size_t position = 0; position < m_sceneIssues.size(); ++position)
        {
            const fried::editor::SceneIssue issue = m_sceneIssues[position];
            ImGui::PushID((int)position + 100000);
            ImGui::TextColored(palette::tomato, "Error");
            ImGui::SameLine();
            ImGui::TextUnformatted(issue.m_node.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", issue.m_message.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove Instance"))
            {
                m_editor.select(issue.m_node);
                m_editor.removeSelected();
                m_issuesCheckedAt = -100;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Choose Another Scene..."))
            {
                m_issueNode = issue.m_node;
                ImGui::OpenPopup("choose-instance");
            }
            ImGui::PopID();
        }
        if (ImGui::BeginPopup("choose-instance"))
        {
            for (const std::string &path : m_editor.scenePaths())
            {
                std::string name = std::filesystem::path(path).filename().string();
                name = fried::core::sceneName(name);
                bool isUsable = m_editor.instanceProblem(name).empty();
                if (ImGui::MenuItem(name.c_str(), nullptr, false, isUsable))
                {
                    m_editor.chooseInstanceScene(m_issueNode, name);
                    m_issuesCheckedAt = -100;
                }
            }
            ImGui::EndPopup();
        }
        int index = 0;
        for (const auto &issue : m_issues)
        {
            ImGui::PushID(index++);
            bool isError = issue.m_severity == fried::editor::AssetIssue::Severity::Error;
            ImGui::TextColored(isError ? palette::tomato : palette::warning, "%s", isError ? "Error" : "Warning");
            ImGui::SameLine();
            if (!issue.m_asset.empty())
            {
                ImGui::TextUnformatted(issue.m_asset.c_str());
                ImGui::SameLine();
            }
            ImGui::TextDisabled("%s", issue.m_message.c_str());
            if (issue.m_fix != fried::editor::AssetIssue::Fix::None)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton(issue.m_fixLabel.c_str()))
                {
                    std::string error;
                    if (!m_editor.fixIssue(issue, &error))
                    {
                        m_issuesMessage = error;
                    }
                    m_issuesCheckedAt = -100;
                }
            }
            ImGui::PopID();
        }
        if (!m_issuesMessage.empty())
        {
            ImGui::Separator();
            ImGui::TextColored(palette::tomato, "%s", m_issuesMessage.c_str());
        }
    }
}
