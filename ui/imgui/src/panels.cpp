#include "panels.h"

#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cmath>

using fried::editor::AssetKind;
using fried::editor::DrawCommand;
using fried::editor::Editor;
using fried::editor::FieldView;
using fried::editor::GizmoMode;
using Json = nlohmann::json;

namespace
{
    constexpr const char *s_assetPayload = "FRIED_ASSET";

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

    // The name of a field, and the width left for its widget. An overridden field is orange and keeps room for its reset button.
    void caption(const std::string &name, bool isOverridden = false)
    {
        ImGui::AlignTextToFramePadding();
        if (isOverridden)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.56f, 0.18f, 1.0f), "%s", name.c_str());
        }
        else
        {
            ImGui::TextDisabled("%s", name.c_str());
        }
        ImGui::SameLine(110.0f);
        ImGui::SetNextItemWidth(isOverridden ? -52.0f : -FLT_MIN);
    }
}

namespace imgui_backend
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
        ImGui::PushStyleColor(ImGuiCol_Button, isGoing ? ImVec4(0.75f, 0.25f, 0.2f, 1.0f) : ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
        if (ImGui::Button(isGoing ? "Stop" : "Play  F5", ImVec2(76, 0)))
        {
            isGoing ? m_editor.stopGame() : m_editor.play();
        }
        ImGui::PopStyleColor();
        if (game == GameRunner::State::Building)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("building...");
        }
        ImGui::SameLine(0, 24);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Tool");
        ImGui::SameLine();
        struct Mode
        {
            const char *m_label;
            GizmoMode m_mode;
        };
        for (const Mode &mode : {Mode{"Move  W", GizmoMode::Move}, Mode{"Rotate  E", GizmoMode::Rotate}, Mode{"Scale  R", GizmoMode::Scale}})
        {
            bool isActive = m_editor.gizmoMode() == mode.m_mode;
            if (isActive)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }
            if (ImGui::Button(mode.m_label))
            {
                m_editor.setGizmoMode(mode.m_mode);
            }
            if (isActive)
            {
                ImGui::PopStyleColor();
            }
            ImGui::SameLine();
        }

        ImGui::SameLine(0, 24);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Platform");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(170);
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
        ImGui::SetNextWindowSize(ImVec2(760, 440), ImGuiCond_Appearing);
        bool isOpen = true;
        if (ImGui::Begin((progress.m_title + "###build").c_str(), progress.m_isFailed ? &isOpen : nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
        {
            if (progress.m_isFailed)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.35f, 1.0f));
                ImGui::TextWrapped("%s", progress.m_message.c_str());
                ImGui::PopStyleColor();
            }
            else
            {
                std::string step = progress.m_steps > 0 ? "Step " + std::to_string(progress.m_step) + " of " + std::to_string(progress.m_steps) + ": " + progress.m_stepLabel : "Starting";
                ImGui::TextUnformatted(step.c_str());
                // The steps have no length of their own to report, so the bar shows how far through them it is and keeps moving inside a step.
                float done = progress.m_steps > 0 ? (float)(progress.m_step - 1) / progress.m_steps : 0.0f;
                float inside = progress.m_steps > 0 ? 0.9f / progress.m_steps * (0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 3.0f)) : 0.0f;
                ImGui::ProgressBar(done + inside, ImVec2(-FLT_MIN, 0), "");
            }

            float footer = ImGui::GetFrameHeightWithSpacing() + 4;
            ImGui::BeginChild("##buildlog", ImVec2(0, -footer), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(progress.m_log.empty() ? "Waiting for the first line of output" : progress.m_log.c_str());
            if (!progress.m_isFailed || ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1)
            {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();

            if (progress.m_canStop && ImGui::Button("Stop"))
            {
                m_editor.stopGame();
            }
            if (progress.m_isFailed && ImGui::Button("Close"))
            {
                isOpen = false;
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
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right.c_str()).x - 20);
        ImGui::TextColored(commands.isClean() ? ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled) : ImVec4(1.0f, 0.56f, 0.18f, 1.0f), "%s", right.c_str());
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
            default:
                draw->AddRect(from, ImVec2(from.x + size.x, from.y + size.y), color, 0, 0, 1.5f);
                break;
            }
        }
        draw->PopClipRect();

        // The editor works in the frame's own pixels.
        ImVec2 mouse = ImGui::GetIO().MousePos;
        double frameX = (mouse.x - corner.x) / scale;
        double frameY = (mouse.y - corner.y) / scale;

        if (isHovered)
        {
            for (int button : {0, 1, 2})
            {
                if (m_dragKind == 0 && ImGui::IsMouseClicked(button))
                {
                    m_dragKind = button == 0 ? 1 : 2;
                    m_editor.pointerDown(frameX, frameY, button == 0 ? Editor::Pointer::Primary : Editor::Pointer::Pan);
                }
            }
            if (ImGui::GetIO().MouseWheel != 0)
            {
                m_editor.wheel(frameX, frameY, ImGui::GetIO().MouseWheel);
            }
        }
        if (m_dragKind != 0)
        {
            int button = m_dragKind == 1 ? 0 : (ImGui::IsMouseDown(1) ? 1 : 2);
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

    void Panels::hierarchy()
    {
        if (ImGui::Button("Add"))
        {
            ImGui::OpenPopup("add-object");
        }
        if (ImGui::BeginPopup("add-object"))
        {
            if (ImGui::MenuItem("Empty object"))
            {
                m_editor.addObject("");
            }
            for (const std::string &type : m_editor.componentTypes())
            {
                if (type != "Transform" && ImGui::MenuItem(type.c_str()))
                {
                    m_editor.addObject(type);
                }
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete"))
        {
            m_editor.removeSelected();
        }
        ImGui::SameLine();
        if (ImGui::Button("Up"))
        {
            m_editor.moveSelected(-1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Down"))
        {
            m_editor.moveSelected(1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Camera"))
        {
            m_editor.setCameraToSelected();
        }
        ImGui::Separator();

        for (const auto &row : m_editor.hierarchy())
        {
            bool isSelected = m_editor.selected() && *m_editor.selected() == row.m_id;
            std::string text = row.m_name + (row.m_isCamera ? "   camera" : "");
            ImGui::PushID(row.m_id.c_str());
            if (ImGui::Selectable(text.c_str(), isSelected))
            {
                m_editor.select(row.m_id);
            }
            ImGui::PopID();
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

    void Panels::objectFields()
    {
        std::string name = m_editor.selectedObjectField("name").value_or(Json(*m_editor.selected())).get<std::string>();
        caption("Name");
        if (textField("##object-name", &name))
        {
            m_editor.renameSelected(name);
        }

        int priority = m_editor.selectedObjectField("priority").value_or(Json(0)).get<int>();
        caption("Priority");
        if (ImGui::DragInt("##priority", &priority))
        {
            m_editor.setObjectField("priority", Json(priority));
        }

        bool isActive = m_editor.selectedObjectField("active").value_or(Json(true)).get<bool>();
        caption("Active");
        if (ImGui::Checkbox("##active", &isActive))
        {
            m_editor.setObjectField("active", Json(isActive));
        }
    }

    bool Panels::referenceField(const std::string &type, const FieldView &field, bool isFont)
    {
        std::string text = fried::editor::assetReferenceText(field.m_value, isFont);
        bool changed = textField(("##" + field.m_schema.m_name).c_str(), &text);
        if (changed)
        {
            if (auto reference = fried::editor::assetReferenceFromText(text, isFont))
            {
                m_editor.setField(type, field.m_schema.m_name, *reference);
            }
        }

        // An asset dragged from the browser onto the field is the reference.
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(s_assetPayload))
            {
                if (auto reference = fried::editor::assetReferenceFromText(std::string((const char *)payload->Data, payload->DataSize - 1) + (isFont ? "@16" : ""), isFont))
                {
                    m_editor.setField(type, field.m_schema.m_name, *reference);
                }
            }
            ImGui::EndDragDropTarget();
        }
        return changed;
    }

    void Panels::fieldEditor(const std::string &type, const FieldView &field)
    {
        const std::string &name = field.m_schema.m_name;
        const std::string &kind = field.m_schema.m_kind;
        ImGui::PushID(name.c_str());
        caption(name, field.m_isOverridden);

        if (kind == "int")
        {
            int value = field.m_value.is_number() ? field.m_value.get<int>() : 0;
            if (ImGui::DragInt("##v", &value))
            {
                m_editor.setField(type, name, Json(value));
            }
        }
        else if (kind == "float")
        {
            float value = field.m_value.is_number() ? field.m_value.get<float>() : 0.0f;
            float speed = name == "rotation" ? 0.5f : (name.find("scale") == 0 || name == "zoom" ? 0.01f : 1.0f);
            if (ImGui::DragFloat("##v", &value, speed, 0, 0, "%.3f"))
            {
                m_editor.setField(type, name, Json((double)value));
            }
        }
        else if (kind == "bool")
        {
            bool value = field.m_value.is_boolean() && field.m_value.get<bool>();
            if (ImGui::Checkbox("##v", &value))
            {
                m_editor.setField(type, name, Json(value));
            }
        }
        else if (kind == "string")
        {
            std::string value = field.m_value.is_string() ? field.m_value.get<std::string>() : std::string();
            if (textField("##v", &value))
            {
                m_editor.setField(type, name, Json(value));
            }
        }
        else if (kind == "color")
        {
            ImVec4 color = vectorOf(field.m_value);
            if (ImGui::ColorEdit4("##v", &color.x, ImGuiColorEditFlags_AlphaPreviewHalf))
            {
                m_editor.setField(type, name,
                                  Json{{"r", channel(color.x)}, {"g", channel(color.y)}, {"b", channel(color.z)}, {"a", channel(color.w)}});
            }
        }
        else if (kind == "flip")
        {
            std::string current = field.m_value.is_string() ? field.m_value.get<std::string>() : std::string();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##v", current.c_str()))
            {
                for (const std::string &option : field.m_schema.m_values)
                {
                    if (ImGui::Selectable(option.c_str(), option == current))
                    {
                        m_editor.setField(type, name, Json(option));
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
                m_editor.setField(type, name, isUsed ? Json{{"x", 0}, {"y", 0}, {"width", 32}, {"height", 32}} : Json());
            }
            if (field.m_value.is_object())
            {
                int values[4] = {field.m_value.value("x", 0), field.m_value.value("y", 0), field.m_value.value("width", 0), field.m_value.value("height", 0)};
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::DragInt4("##region", values))
                {
                    m_editor.setField(type, name, Json{{"x", values[0]}, {"y", values[1]}, {"width", values[2]}, {"height", values[3]}});
                }
            }
        }
        else if (kind == "object")
        {
            std::string current = field.m_value.is_string() ? field.m_value.get<std::string>() : std::string();
            std::string shown = "(none)";
            std::vector<fried::editor::HierarchyRow> rows = m_editor.hierarchy();
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
                    m_editor.setField(type, name, Json());
                }
                for (const auto &row : rows)
                {
                    ImGui::PushID(row.m_id.c_str());
                    if (ImGui::Selectable(row.m_name.c_str(), row.m_id == current))
                    {
                        m_editor.setField(type, name, Json(row.m_id));
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            referenceField(type, field, kind == "font");
        }
        if (field.m_isOverridden)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("reset"))
            {
                m_editor.clearOverride(type, name);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Use the value every platform shares");
            }
        }
        ImGui::PopID();
    }

    void Panels::inspector()
    {
        if (!m_editor.selected())
        {
            ImGui::TextDisabled("Nothing selected");
            return;
        }

        if (m_editor.platform() != fried::editor::Platform::Pc)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.56f, 0.18f, 1.0f), "Editing %s", fried::editor::platformLabel(m_editor.platform()).c_str());
            ImGui::TextDisabled("Changes are saved as overrides for this platform.");
            ImGui::Spacing();
        }

        objectFields();
        for (const auto &view : m_editor.inspector())
        {
            ImGui::PushID(view.m_type.c_str());
            ImGui::Spacing();
            bool isOpen = ImGui::CollapsingHeader(view.m_type.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
            if (view.m_isRemovable)
            {
                ImGui::SameLine(ImGui::GetWindowWidth() - 36);
                if (ImGui::SmallButton("x"))
                {
                    m_editor.removeComponent(view.m_type);
                    ImGui::PopID();
                    return;
                }
            }
            if (isOpen)
            {
                for (const FieldView &field : view.m_fields)
                {
                    fieldEditor(view.m_type, field);
                }
            }
            ImGui::PopID();
        }

        ImGui::Spacing();
        if (ImGui::Button("Add component", ImVec2(-FLT_MIN, 0)))
        {
            ImGui::OpenPopup("add-component");
        }
        if (ImGui::BeginPopup("add-component"))
        {
            for (const std::string &type : m_editor.addableComponents())
            {
                if (ImGui::MenuItem(type.c_str()))
                {
                    m_editor.addComponent(type);
                }
            }
            ImGui::EndPopup();
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
            }
            if (ImGui::Button(filters[index]))
            {
                m_assetFilter = index;
            }
            if (isActive)
            {
                ImGui::PopStyleColor();
            }
        }
        ImGui::Separator();

        const float cell = 84;
        float width = ImGui::GetContentRegionAvail().x;
        int columns = std::max(1, (int)(width / (cell + ImGui::GetStyle().ItemSpacing.x)));
        int index = 0;
        for (const auto &entry : m_editor.assets())
        {
            if (m_assetFilter != 0 && entry.m_kind != kinds[m_assetFilter])
            {
                continue;
            }
            if (index++ % columns != 0)
            {
                ImGui::SameLine();
            }

            ImGui::PushID(entry.reference().c_str());
            ImGui::BeginGroup();
            const Texture *thumbnail = entry.m_kind == AssetKind::Image ? m_images.get(m_editor.assetFile(entry.m_root, entry.m_path)) : nullptr;
            bool isClicked = false;
            if (thumbnail)
            {
                float ratio = std::min(64.0f / thumbnail->width(), 64.0f / thumbnail->height());
                ImVec2 size(thumbnail->width() * ratio, thumbnail->height() * ratio);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cell - size.x) / 2);
                isClicked = ImGui::ImageButton("##thumb", thumbnail->id(), size);
            }
            else
            {
                const char *tag = entry.m_kind == AssetKind::Font ? "Aa" : entry.m_kind == AssetKind::Audio ? "wav" : entry.m_kind == AssetKind::Scene ? "scn" : "...";
                isClicked = ImGui::Button(tag, ImVec2(cell - 10, 64));
            }
            std::string name = entry.m_path.substr(entry.m_path.rfind('/') == std::string::npos ? 0 : entry.m_path.rfind('/') + 1);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cell);
            ImGui::TextUnformatted(name.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndGroup();

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", entry.reference().c_str());
            }
            if (ImGui::BeginDragDropSource())
            {
                std::string reference = entry.reference();
                ImGui::SetDragDropPayload(s_assetPayload, reference.c_str(), reference.size() + 1);
                ImGui::TextUnformatted(name.c_str());
                ImGui::EndDragDropSource();
            }
            if (isClicked || (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)))
            {
                isClicked = ImGui::IsMouseDoubleClicked(0);
            }
            if (isClicked)
            {
                m_editor.dropAsset(entry.m_root, entry.m_path, m_editor.viewport().frameWidth() / 2.0, m_editor.viewport().frameHeight() / 2.0);
            }
            ImGui::PopID();
        }
    }
}

namespace imgui_backend
{
    void Panels::issues()
    {
        // Reading every asset is not free, so the list is made again when the platform changes, after a fix, and every few seconds.
        double now = ImGui::GetTime();
        bool isStale = now - m_issuesCheckedAt > 3.0 || m_issuesPlatform != m_editor.platform();
        if (ImGui::Button("Check again") || isStale)
        {
            m_issues = m_editor.issues();
            m_issuesCheckedAt = now;
            m_issuesPlatform = m_editor.platform();
        }
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
            ImGui::TextColored(isError ? ImVec4(0.95f, 0.38f, 0.38f, 1.0f) : ImVec4(1.0f, 0.78f, 0.30f, 1.0f), "%s", isError ? "Error" : "Warning");
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
            ImGui::TextColored(ImVec4(0.95f, 0.38f, 0.38f, 1.0f), "%s", m_issuesMessage.c_str());
        }
    }
}
