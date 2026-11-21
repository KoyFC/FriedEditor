#include "panels.h"

#include "style.h"

#include "fried/editor/editor_state.h"

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
            return editor_ui::palette::yolk;
        case fried::core::LineKind::Success:
            return editor_ui::palette::leaf;
        case fried::core::LineKind::Command:
            return editor_ui::palette::yolkHot;
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
    void caption(const std::string &name)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", name.c_str());
        ImGui::SameLine(editor_ui::scaled(110.0f));
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
        using fried::editor::GameRunner;
        GameRunner::State game = m_editor.gameState();
        bool isGoing = game == GameRunner::State::Building || game == GameRunner::State::Running;
        ImGui::PushStyleColor(ImGuiCol_Button, isGoing ? palette::tomato : palette::leaf);
        if (Icons::button("play", isGoing ? Icon::Stop : Icon::Play, isGoing ? "Stop the game" : "Play the scene (F5)"))
        {
            isGoing ? m_editor.stopGame() : m_editor.play();
        }
        ImGui::PopStyleColor();
        if (game == GameRunner::State::Building)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("building...");
        }
        ImGui::SameLine(0, scaled(24));
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("View");
        ImGui::SameLine();
        for (fried::editor::Editor::ViewMode view : {fried::editor::Editor::ViewMode::TwoD, fried::editor::Editor::ViewMode::ThreeD})
        {
            bool isActive = m_editor.viewMode() == view;
            if (isActive)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                ImGui::PushStyleColor(ImGuiCol_Text, palette::yolk);
            }
            if (ImGui::Button(view == fried::editor::Editor::ViewMode::TwoD ? "2D" : "3D"))
            {
                m_editor.setViewMode(view);
            }
            if (isActive)
            {
                ImGui::PopStyleColor(2);
            }
            ImGui::SameLine();
        }

        ImGui::SameLine(0, scaled(24));
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Tool");
        ImGui::SameLine();
        struct Mode
        {
            const char *m_id;
            Icon m_icon;
            const char *m_tooltip;
            GizmoMode m_mode;
        };
        for (const Mode &mode : {Mode{"move", Icon::ToolMove, "Move (W)", GizmoMode::Move}, Mode{"rotate", Icon::ToolRotate, "Rotate (E)", GizmoMode::Rotate},
                                 Mode{"scale", Icon::ToolScale, "Scale (R)", GizmoMode::Scale}})
        {
            bool isDisabled = m_editor.viewMode() == fried::editor::Editor::ViewMode::ThreeD && mode.m_mode != GizmoMode::Move;
            if (isDisabled)
            {
                ImGui::BeginDisabled();
            }
            if (Icons::button(mode.m_id, mode.m_icon, mode.m_tooltip, m_editor.gizmoMode() == mode.m_mode))
            {
                m_editor.setGizmoMode(mode.m_mode);
            }
            if (isDisabled)
            {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
        }

        ImGui::SameLine(0, scaled(24));
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Platform");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(scaled(170));
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
        if (ImGui::Begin((progress.m_title + "###build").c_str(), progress.m_isFinished ? &isOpen : nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
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
                if (ImGui::Button("Show in folder"))
                {
                    revealFile(progress.m_builtFile);
                }
            }
            if (progress.m_lines != nullptr && !progress.m_lines->empty())
            {
                ImGui::SameLine();
                if (ImGui::Button("Copy output"))
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

    void Panels::statusBar()
    {
        auto &commands = m_editor.commands();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(m_editor.message().c_str());
        std::string right = (m_issues.empty() ? std::string() : std::to_string(m_issues.size()) + " asset issues   ") + (commands.isClean() ? "Saved" : "Unsaved changes");
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right.c_str()).x - scaled(20));
        ImGui::TextColored(commands.isClean() ? ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled) : palette::yolk, "%s", right.c_str());
    }

    bool Panels::takeValueDrag()
    {
        bool isDragged = s_isValueDragged;
        s_isValueDragged = false;
        return isDragged;
    }

    void Panels::viewport()
    {
        const fried::editor::Frame &frame = m_editor.frame();
        if (m_uploadedSerial != m_editor.frameSerial() && frame.m_rgba.size() == (size_t)frame.m_width * frame.m_height * 4)
        {
            m_frame.upload(frame.m_rgba.data(), frame.m_width, frame.m_height);
            m_uploadedSerial = m_editor.frameSerial();
        }

        ImVec2 region = ImGui::GetContentRegionAvail();
        m_editor.setViewSize((int)region.x, (int)region.y);
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##view", region, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
        bool isHovered = ImGui::IsItemHovered();

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
            m_panPosition = ImVec2(m_panPosition.x + m_heldMotion.x, m_panPosition.y + m_heldMotion.y);
        }
        else
        {
            m_panPosition = ImGui::GetIO().MousePos;
        }
        ImVec2 mouse = m_panPosition;
        double frameX = (mouse.x - corner.x) / scale;
        double frameY = (mouse.y - corner.y) / scale;

        if (isHovered)
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
    }

    namespace
    {
        constexpr const char *s_nodePayload = "fried-node";

        // The classes of nodes as a menu that follows the inheritance: a class with others under it opens a submenu that starts with itself.
        void classMenu(const std::vector<fried::editor::ClassSchema> &classes, const std::string &parent, const std::function<void(const std::string &)> &choose)
        {
            for (const fried::editor::ClassSchema &schema : classes)
            {
                if (schema.m_parent != parent)
                {
                    continue;
                }
                bool hasSubclasses = std::any_of(classes.begin(), classes.end(), [&](const fried::editor::ClassSchema &other) { return other.m_parent == schema.m_name; });
                if (!hasSubclasses)
                {
                    if (ImGui::MenuItem(schema.m_name.c_str()))
                    {
                        choose(schema.m_name);
                    }
                }
                else if (ImGui::BeginMenu(schema.m_name.c_str()))
                {
                    if (ImGui::MenuItem(schema.m_name.c_str()))
                    {
                        choose(schema.m_name);
                    }
                    ImGui::Separator();
                    classMenu(classes, schema.m_name, choose);
                    ImGui::EndMenu();
                }
            }
        }
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
            {"Node", Icon::Node},
            {"Node2D", Icon::Node2D},
            {"Node3D", Icon::Node3D},
            {"Sprite", Icon::Sprite2D},
            {"Camera", Icon::Camera2D},
            {"Camera3D", Icon::Camera3D},
            {"Collider", Icon::CollisionShape2D},
            {"KinematicBody", Icon::CharacterBody2D},
            {"MeshInstance", Icon::MeshInstance3D},
            {"DirectionalLight", Icon::DirectionalLight3D},
            {"Control", Icon::Control},
            {"Label", Icon::Label},
            {"Button", Icon::Button},
            {"TextureRect", Icon::TextureRect},
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

    void Panels::hierarchy()
    {
        if (Icons::button("add", Icon::Add, "Add a node"))
        {
            ImGui::OpenPopup("add-node");
        }
        if (ImGui::BeginPopup("add-node"))
        {
            ImGui::TextDisabled(m_editor.selected() ? "Add under the selected node" : "Add to the scene");
            classMenu(m_editor.classes(), "", [&](const std::string &type) { m_editor.addNode(type, m_editor.selected()); });
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (Icons::button("delete", Icon::Remove, "Delete the selected node"))
        {
            m_editor.removeSelected();
        }
        ImGui::SameLine();
        if (Icons::button("up", Icon::MoveUp, "Move the node up among its siblings"))
        {
            m_editor.moveSelected(-1);
        }
        ImGui::SameLine();
        if (Icons::button("down", Icon::MoveDown, "Move the node down among its siblings"))
        {
            m_editor.moveSelected(1);
        }
        ImGui::SameLine();
        if (Icons::button("camera", Icon::Camera2D, "Use the selected node as the scene's camera"))
        {
            m_editor.setCameraToSelected();
        }
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
        ImGui::Separator();

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
                if (containsIgnoringCase(row.m_name, m_hierarchyFilter) || containsIgnoringCase(row.m_type, m_hierarchyFilter))
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
            ImGui::TextDisabled("No node matches");
        }

        // Rows are as tall as a button and touch each other, so the stripes behind them tile.
        const float rowPadding = scaled(4);
        const float rowHeight = ImGui::GetFontSize() + rowPadding * 2;
        const float levelWidth = ImGui::GetTreeNodeToLabelSpacing();
        const float baseX = ImGui::GetCursorScreenPos().x;
        const ImVec2 windowPosition = ImGui::GetWindowPos();
        const float windowRight = windowPosition.x + ImGui::GetWindowWidth();

        ImGuiListClipper clipper;
        clipper.Begin((int)m_visibleRows.size(), rowHeight);
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

                ImGui::PushStyleColor(ImGuiCol_Header, palette::yolkAt(0.22f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, isSelected ? palette::yolkAt(0.30f) : ImVec4(1, 1, 1, 0.07f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, palette::yolkAt(0.36f));
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
                    list->AddRectFilled(ImVec2(windowPosition.x, top), ImVec2(windowPosition.x + scaled(2), top + rowHeight), ImGui::GetColorU32(palette::yolk));
                }
                Icons::draw(list, iconOf(row.m_type), ImVec2(contentX, top + (rowHeight - iconSize) / 2));
                list->AddText(ImVec2(contentX + iconSize + scaled(6), top + rowPadding), ImGui::GetColorU32(isFiltering && m_filterKeep[m_visibleRows[visible]] == 2 ? palette::yolk : ImGui::GetStyleColorVec4(ImGuiCol_Text)), row.m_name.c_str());
                if (row.m_isCamera)
                {
                    Icons::draw(list, Icon::Camera2D, ImVec2(windowRight - iconSize - scaled(10), top + (rowHeight - iconSize) / 2), ImGui::GetColorU32(palette::yolk));
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

                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                {
                    m_editor.select(row.m_id);
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
                {
                    m_renameId = row.m_id;
                    m_renameText = row.m_name;
                    m_isRenameAsked = true;
                }

                if (ImGui::BeginDragDropSource())
                {
                    ImGui::SetDragDropPayload(s_nodePayload, row.m_id.c_str(), row.m_id.size() + 1);
                    ImGui::TextUnformatted(row.m_name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget())
                {
                    ImVec2 low = ImGui::GetItemRectMin();
                    ImVec2 high = ImGui::GetItemRectMax();
                    float place = (ImGui::GetMousePos().y - low.y) / std::max(1.0f, high.y - low.y);
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(s_nodePayload, ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
                    {
                        std::string dropped((const char *)payload->Data);
                        const fried::editor::SceneDocument &document = m_editor.document();
                        if (place < 0.25f)
                        {
                            m_editor.reparent(dropped, document.parentOf(row.m_id), document.indexOf(row.m_id));
                        }
                        else if (place > 0.75f && !(row.m_hasChildren && isOpen))
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
                        if (place < 0.25f)
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
                    if (ImGui::BeginMenu("Add Child"))
                    {
                        classMenu(m_editor.classes(), "", [&](const std::string &type) { m_editor.addNode(type, row.m_id); });
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Change Type"))
                    {
                        classMenu(m_editor.classes(), "", [&](const std::string &type) { m_editor.changeType(row.m_id, type); });
                        ImGui::EndMenu();
                    }
                    if (ImGui::MenuItem("Rename"))
                    {
                        m_renameId = row.m_id;
                        m_renameText = row.m_name;
                        m_isRenameAsked = true;
                    }
                    if (ImGui::MenuItem("Use as the Camera"))
                    {
                        m_editor.setCameraToSelected();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
                    {
                        m_editor.duplicateSelected();
                    }
                    if (ImGui::MenuItem("Copy", "Ctrl+C"))
                    {
                        std::string text = m_editor.copySelected();
                        ImGui::SetClipboardText(text.c_str());
                    }
                    if (ImGui::MenuItem("Paste", "Ctrl+V"))
                    {
                        const char *text = ImGui::GetClipboardText();
                        m_editor.paste(text ? text : "");
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete"))
                    {
                        m_editor.removeSelected();
                    }
                    ImGui::EndPopup();
                }
            }
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

        if (m_isRenameAsked)
        {
            ImGui::OpenPopup("rename-node");
            m_isRenameAsked = false;
        }
        if (ImGui::BeginPopup("rename-node"))
        {
            ImGui::TextDisabled("Name");
            ImGui::SetKeyboardFocusHere();
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

    bool Panels::referenceField(const FieldView &field, bool isFont)
    {
        std::string text = fried::editor::assetReferenceText(field.m_value, isFont);
        bool changed = textField(("##" + field.m_schema.m_name).c_str(), &text);
        if (changed)
        {
            if (auto reference = fried::editor::assetReferenceFromText(text, isFont))
            {
                m_editor.setField(field.m_schema.m_name, *reference);
            }
        }

        // An asset dragged from the browser onto the field is the reference.
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(s_assetPayload))
            {
                if (auto reference = fried::editor::assetReferenceFromText(std::string((const char *)payload->Data, payload->DataSize - 1) + (isFont ? "@16" : ""), isFont))
                {
                    m_editor.setField(field.m_schema.m_name, *reference);
                }
            }
            ImGui::EndDragDropTarget();
        }
        return changed;
    }

    void Panels::fieldEditor(const FieldView &field)
    {
        const std::string &name = field.m_schema.m_name;
        const std::string &kind = field.m_schema.m_kind;
        ImGui::PushID(name.c_str());
        // The text of a Text is written as lines, so its name goes above the area and not beside it.
        bool isTextArea = kind == "string" && name == "text";
        if (isTextArea)
        {
            ImGui::TextDisabled("%s", name.c_str());
        }
        else
        {
            caption(name);
        }

        if (kind == "int")
        {
            int value = field.m_value.is_number() ? field.m_value.get<int>() : 0;
            if (dragInt("##v", &value))
            {
                m_editor.setField(name, Json(value));
            }
        }
        else if (kind == "float")
        {
            float value = field.m_value.is_number() ? field.m_value.get<float>() : 0.0f;
            float speed = name == "rotation" ? 0.5f : (name.find("scale") == 0 || name == "zoom" ? 0.01f : 1.0f);
            if (dragFloat("##v", &value, speed, 0, 0, "%.3f"))
            {
                m_editor.setField(name, Json((double)value));
            }
        }
        else if (kind == "bool")
        {
            bool value = field.m_value.is_boolean() && field.m_value.get<bool>();
            if (ImGui::Checkbox("##v", &value))
            {
                m_editor.setField(name, Json(value));
            }
        }
        else if (kind == "string")
        {
            std::string value = field.m_value.is_string() ? field.m_value.get<std::string>() : std::string();
            if (isTextArea ? textArea("##v", &value) : textField("##v", &value))
            {
                m_editor.setField(name, Json(value));
            }
        }
        else if (kind == "color")
        {
            ImVec4 color = vectorOf(field.m_value);
            if (ImGui::ColorEdit4("##v", &color.x, ImGuiColorEditFlags_AlphaPreviewHalf))
            {
                m_editor.setField(name,
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
                        m_editor.setField(name, Json(option));
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
                m_editor.setField(name, isUsed ? Json{{"x", 0}, {"y", 0}, {"width", 32}, {"height", 32}} : Json());
            }
            if (field.m_value.is_object())
            {
                int values[4] = {field.m_value.value("x", 0), field.m_value.value("y", 0), field.m_value.value("width", 0), field.m_value.value("height", 0)};
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (dragInt4("##region", values))
                {
                    m_editor.setField(name, Json{{"x", values[0]}, {"y", values[1]}, {"width", values[2]}, {"height", values[3]}});
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
                    m_editor.setField(name, Json());
                }
                for (const auto &row : rows)
                {
                    ImGui::PushID(row.m_id.c_str());
                    if (ImGui::Selectable(row.m_name.c_str(), row.m_id == current))
                    {
                        m_editor.setField(name, Json(row.m_id));
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            referenceField(field, kind == "font");
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
                m_editor.setField(name, Json((double)value));
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
        list->AddText(ImVec2(x + Icons::size() + scaled(6), (low.y + high.y - ImGui::GetFontSize()) / 2), ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
        return isOpen;
    }

    void Panels::inspector()
    {
        if (!m_editor.selected())
        {
            ImGui::TextDisabled("Nothing selected");
            return;
        }

        for (const auto &view : m_editor.inspector())
        {
            ImGui::PushID(view.m_class.c_str());
            ImGui::Spacing();
            bool isOpen = classHeader(view.m_class);
            if (isOpen && view.m_class == "Control")
            {
                controlEditor(view);
            }
            else if (isOpen)
            {
                for (const FieldView &field : view.m_fields)
                {
                    fieldEditor(field);
                }
            }
            ImGui::PopID();
        }
    }

    void Panels::assets()
    {
        static const char *filters[] = {"All", "Images", "Fonts", "Audio", "Scenes"};
        static const AssetKind kinds[] = {AssetKind::Other, AssetKind::Image, AssetKind::Font, AssetKind::Audio, AssetKind::Scene};
        for (int index = 0; index < 5; ++index)
        {
            if (index > 0)
            {
                ImGui::SameLine();
            }
            bool isActive = m_assetFilter == index;
            if (isActive)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                ImGui::PushStyleColor(ImGuiCol_Text, palette::yolk);
            }
            if (ImGui::Button(filters[index]))
            {
                m_assetFilter = index;
            }
            if (isActive)
            {
                ImGui::PopStyleColor(2);
            }
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##asset-search", "Search", &m_assetSearch);
        ImGui::Separator();

        const float cell = 84;
        float width = ImGui::GetContentRegionAvail().x;
        int columns = std::max(1, (int)(width / (cell + ImGui::GetStyle().ItemSpacing.x)));
        int index = 0;
        for (const auto &entry : m_editor.assets())
        {
            std::string reference = entry.reference();
            if ((m_assetFilter != 0 && entry.m_kind != kinds[m_assetFilter]) || (!m_assetSearch.empty() && !containsIgnoringCase(reference, m_assetSearch)))
            {
                continue;
            }
            if (index++ % columns != 0)
            {
                ImGui::SameLine();
            }

            ImGui::PushID(reference.c_str());
            ImGui::BeginGroup();
            const Texture *thumbnail = entry.m_kind == AssetKind::Image ? m_images.get(m_editor.assetFile(entry.m_root, entry.m_path)) : nullptr;
            if (thumbnail)
            {
                float ratio = std::min(64.0f / thumbnail->width(), 64.0f / thumbnail->height());
                ImVec2 size(thumbnail->width() * ratio, thumbnail->height() * ratio);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cell - size.x) / 2);
                ImGui::Image(thumbnail->id(), size);
            }
            else
            {
                const char *tag = entry.m_kind == AssetKind::Font ? "Aa" : entry.m_kind == AssetKind::Audio ? "wav" : entry.m_kind == AssetKind::Scene ? "scn" : "...";
                ImGui::Button(tag, ImVec2(cell - 10, 64));
            }
            std::string name = entry.m_path.substr(entry.m_path.rfind('/') == std::string::npos ? 0 : entry.m_path.rfind('/') + 1);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cell);
            ImGui::TextUnformatted(name.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndGroup();

            bool isScene = entry.m_kind == AssetKind::Scene;
            bool isHovered = ImGui::IsItemHovered();
            if (isScene && isOpenScene(m_editor.assetFile(entry.m_root, entry.m_path), m_editor.scenePath()))
            {
                // The scene that is being edited.
                ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGuiCol_CheckMark), 3.0f, 0, 2.0f);
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

            // A double click does what suits the file: a scene opens, and an image or a font is added to the scene.
            bool isUsable = isScene || entry.m_kind == AssetKind::Image || entry.m_kind == AssetKind::Font;
            bool isActivated = isUsable && isHovered && ImGui::IsMouseDoubleClicked(0);
            if (ImGui::BeginPopupContextItem("asset-menu"))
            {
                if (isUsable && ImGui::MenuItem(isScene ? "Open scene" : "Add to scene"))
                {
                    isActivated = true;
                }
                if (ImGui::MenuItem("Copy reference"))
                {
                    ImGui::SetClipboardText(reference.c_str());
                }
                if (ImGui::MenuItem("Show in folder"))
                {
                    std::string folder = std::filesystem::path(m_editor.assetFile(entry.m_root, entry.m_path)).parent_path().string();
                    SDL_OpenURL(("file://" + folder).c_str());
                }
                ImGui::EndPopup();
            }
            if (isActivated)
            {
                if (isScene)
                {
                    openSceneFile(m_editor.assetFile(entry.m_root, entry.m_path));
                }
                else
                {
                    m_editor.dropAsset(entry.m_root, entry.m_path, m_editor.viewport().frameWidth() / 2.0, m_editor.viewport().frameHeight() / 2.0);
                }
            }
            ImGui::PopID();
        }
        sceneChangeDialog();
    }

    // Opens the scene at once when the one being edited is saved, and otherwise asks what to do with its changes first.
    void Panels::openSceneFile(const std::string &path)
    {
        if (isOpenScene(path, m_editor.scenePath()))
        {
            return;
        }
        if (m_editor.commands().isClean())
        {
            std::string error;
            if (!m_editor.openScene(path, &error))
            {
                m_editor.setMessage(error);
            }
            return;
        }
        m_pendingScene = path;
        m_isSceneChangeAsked = true;
    }

    void Panels::sceneChangeDialog()
    {
        if (m_isSceneChangeAsked)
        {
            ImGui::OpenPopup("Unsaved changes##scene");
            m_isSceneChangeAsked = false;
        }
        if (!ImGui::BeginPopupModal("Unsaved changes##scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        ImGui::TextUnformatted("Save the scene before leaving it?");
        ImGui::Spacing();
        bool isLeaving = false;
        std::string error;
        if (ImGui::Button("Save"))
        {
            isLeaving = m_editor.save(&error);
            if (!isLeaving)
            {
                m_editor.setMessage(error);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
        {
            isLeaving = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        if (isLeaving && !m_editor.openScene(m_pendingScene, &error))
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
            m_issuesCheckedAt = now;
            m_issuesPlatform = m_editor.platform();
        }
    }

    void Panels::issues()
    {
        refreshIssues(ImGui::Button("Check again"));
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
            if (ImGui::Button("Fix all"))
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

        if (m_issues.empty())
        {
            ImGui::TextDisabled("The assets suit this platform.");
        }
        int index = 0;
        for (const auto &issue : m_issues)
        {
            ImGui::PushID(index++);
            bool isError = issue.m_severity == fried::editor::AssetIssue::Severity::Error;
            ImGui::TextColored(isError ? palette::tomato : palette::yolk, "%s", isError ? "Error" : "Warning");
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
