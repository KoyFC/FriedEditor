#include "panels.h"

#include "fried/editor/editor.h"
#include "fried/editor/input_map.h"
#include "style.h"
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <SDL.h>

#include <algorithm>
#include <functional>

namespace editor_ui
{
    using fried::editor::InputName;
    using fried::editor::InputNames;
    using Json = nlohmann::json;

    namespace
    {
        bool containsText(const std::string &text, const std::string &filter)
        {
            if (filter.empty())
            {
                return true;
            }
            auto lower = [](std::string value) {
                std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                return value;
            };
            return lower(text).find(lower(filter)) != std::string::npos;
        }

        std::string kindTitle(const std::string &kind)
        {
            return kind == "single" ? "Single" : kind == "axis" ? "Axis" : kind == "vector" ? "Vector" : "Stick";
        }
    }

    void Panels::noteKey(int scancode)
    {
        m_inputKeyPressed = scancode;
    }

    // The input actions of the game, as the `input` block of project.fried, laid out like Unity's input actions editor: the maps on the left, the actions of the chosen map with their bindings in the middle, and the properties of what is chosen on the right.
    void Panels::inputMapPage()
    {
        fried::editor::ProjectSettings &settings = m_editor.projectSettings();
        const InputNames &names = m_editor.inputNames();
        if (names.isEmpty())
        {
            ImGui::TextDisabled("The names of the keys and buttons come from the editor player, which is still starting.");
            return;
        }

        Json block = m_editor.project().inputBlock();
        if (!block.is_object())
        {
            block = Json::object();
        }
        if (!block.contains("maps") || !block["maps"].is_array())
        {
            block["maps"] = Json::array();
        }
        Json &maps = block["maps"];
        bool isChanged = false;
        bool isPickAsked = false;
        std::function<void()> later;

        m_inputMap = std::clamp(m_inputMap, 0, std::max(0, (int)maps.size() - 1));
        Json *map = maps.empty() ? nullptr : &maps[m_inputMap];
        Json *actions = nullptr;
        if (map)
        {
            if (!map->contains("actions") || !(*map)["actions"].is_array())
            {
                (*map)["actions"] = Json::array();
            }
            actions = &(*map)["actions"];
            m_inputAction = std::clamp(m_inputAction, 0, std::max(0, (int)actions->size() - 1));
        }
        Json *action = actions && !actions->empty() ? &(*actions)[m_inputAction] : nullptr;
        if (action && (!action->contains("bindings") || !(*action)["bindings"].is_array()))
        {
            (*action)["bindings"] = Json::array();
        }
        if (action)
        {
            m_inputBinding = std::clamp(m_inputBinding, -1, (int)(*action)["bindings"].size() - 1);
        }

        auto problem = m_settingErrors.find("input");
        if (problem != m_settingErrors.end() && !problem->second.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, palette::tomato);
            ImGui::TextWrapped("%s", problem->second.c_str());
            ImGui::PopStyleColor();
        }

        auto nameField = [&](const char *id, const std::string &current, std::string *result) {
            ImGuiID key = ImGui::GetID(id);
            std::string &buffer = m_inputBuffers[key];
            if (ImGui::GetActiveID() != key)
            {
                buffer = current;
            }
            ImGui::InputText(id, &buffer);
            if (ImGui::IsItemDeactivatedAfterEdit() && buffer != current)
            {
                *result = buffer;
                return true;
            }
            return false;
        };

        // A column has a title bar with its add button, over a body that is darker than the window, so the three read as separate lists.
        auto column = [&](const char *id, const char *title, float width, const char *addLabel, const std::function<void()> &add, bool canAdd, const std::function<void()> &body) {
            ImGui::BeginGroup();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, palette::accent);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(scaled(8), scaled(4)));
            ImGui::BeginChild((std::string(id) + "-title").c_str(), ImVec2(width, ImGui::GetFrameHeight() + scaled(8)), ImGuiChildFlags_AlwaysUseWindowPadding);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(title);
            if (addLabel)
            {
                ImGui::SameLine(width - ImGui::CalcTextSize(addLabel).x - scaled(26));
                ImGui::BeginDisabled(!canAdd);
                if (ImGui::SmallButton(addLabel))
                {
                    later = add;
                }
                ImGui::EndDisabled();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, palette::field);
            ImGui::BeginChild((std::string(id) + "-body").c_str(), ImVec2(width, ImGui::GetContentRegionAvail().y - scaled(22)), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
            body();
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::EndGroup();
        };
        // A row of a list: an action is a bold line with its type as a badge, and a binding sits under it with a mark and a line down its side, so what belongs to what is seen without reading.
        auto choice = [&](const std::string &label, const std::string &badge, bool isSelected, bool isBinding, bool isLast) {
            ImVec2 start = ImGui::GetCursorScreenPos();
            float rowHeight = ImGui::GetFrameHeight() + scaled(4);
            float width = ImGui::GetContentRegionAvail().x;
            ImGui::PushStyleColor(ImGuiCol_Header, palette::accentAt(0.65f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1, 1, 1, 0.08f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, palette::accentAt(0.8f));
            bool isClicked = ImGui::Selectable("##row", isSelected, ImGuiSelectableFlags_SpanAvailWidth, ImVec2(0, rowHeight));
            ImGui::PopStyleColor(3);
            ImDrawList *list = ImGui::GetWindowDrawList();
            float middle = start.y + rowHeight / 2;
            float textY = middle - ImGui::GetFontSize() / 2;
            if (!isBinding)
            {
                list->AddRectFilled(ImVec2(start.x, start.y), ImVec2(start.x + scaled(3), start.y + rowHeight), ImGui::GetColorU32(palette::accentHot));
                list->AddText(ImVec2(start.x + scaled(10), textY), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), label.c_str());
                if (!badge.empty())
                {
                    float badgeWidth = ImGui::CalcTextSize(badge.c_str()).x + scaled(12);
                    ImVec2 low(start.x + width - badgeWidth - scaled(4), middle - scaled(9));
                    list->AddRectFilled(low, ImVec2(low.x + badgeWidth, low.y + scaled(18)), ImGui::GetColorU32(palette::accentAt(0.35f)), scaled(3));
                    list->AddText(ImVec2(low.x + scaled(6), textY), ImGui::GetColorU32(palette::accentText), badge.c_str());
                }
            }
            else
            {
                float lineX = start.x + scaled(16);
                ImU32 line = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.22f));
                list->AddLine(ImVec2(lineX, start.y), ImVec2(lineX, isLast ? middle : start.y + rowHeight), line);
                list->AddLine(ImVec2(lineX, middle), ImVec2(lineX + scaled(10), middle), line);
                list->AddText(ImVec2(start.x + scaled(32), textY), ImGui::GetColorU32(ImGuiCol_TextDisabled), label.c_str());
            }
            return isClicked;
        };

        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float total = ImGui::GetContentRegionAvail().x;
        const float mapsWidth = std::floor(total * 0.22f);
        const float actionsWidth = std::floor(total * 0.40f);
        const float propertiesWidth = total - mapsWidth - actionsWidth - gap * 2;

        std::vector<std::string> mapNames;
        for (const Json &entry : maps)
        {
            mapNames.push_back(entry.value("name", ""));
        }

        column("##maps", "Maps", mapsWidth, "+", [&] {
            maps.push_back({{"name", fried::editor::uniqueInputName(mapNames, "Map")}, {"enabled", true}, {"actions", Json::array()}});
            m_inputMap = (int)maps.size() - 1;
            m_inputAction = 0;
            m_inputBinding = -1;
            isChanged = true;
        }, true, [&] {
            for (int index = 0; index < (int)maps.size(); ++index)
            {
                ImGui::PushID(index);
                if (choice(mapNames[index].empty() ? "(unnamed)" : mapNames[index], std::string(), index == m_inputMap, false, false))
                {
                    m_inputMap = index;
                    m_inputAction = 0;
                    m_inputBinding = -1;
                }
                if (ImGui::BeginPopupContextItem("map-menu"))
                {
                    if (ImGui::MenuItem("Delete"))
                    {
                        later = [&maps, index] { maps.erase(maps.begin() + index); };
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            if (maps.empty())
            {
                ImGui::TextDisabled("No maps. Add one with +");
            }
        });
        ImGui::SameLine();

        column("##actions", map ? ((*map).value("name", "") + " actions").c_str() : "Actions", actionsWidth, "+", [&] {
            std::vector<std::string> taken;
            for (const Json &entry : *actions)
            {
                taken.push_back(entry.value("name", ""));
            }
            actions->push_back({{"name", fried::editor::uniqueInputName(taken, "Action")}, {"type", "button"}, {"bindings", Json::array()}});
            m_inputAction = (int)actions->size() - 1;
            m_inputBinding = -1;
            isChanged = true;
        }, map != nullptr, [&] {
            if (!actions)
            {
                return;
            }
            for (int actionIndex = 0; actionIndex < (int)actions->size(); ++actionIndex)
            {
                Json &entry = (*actions)[actionIndex];
                ImGui::PushID(actionIndex);
                bool isHere = actionIndex == m_inputAction;
                if (actionIndex > 0)
                {
                    ImGui::Dummy(ImVec2(0, scaled(4)));
                }
                if (choice(entry.value("name", ""), entry.value("type", "button"), isHere && m_inputBinding < 0, false, false))
                {
                    m_inputAction = actionIndex;
                    m_inputBinding = -1;
                }
                if (ImGui::BeginPopupContextItem("action-menu"))
                {
                    if (ImGui::MenuItem("Add Binding", nullptr, false, true))
                    {
                        m_inputAction = actionIndex;
                    }
                    for (const std::string &kind : fried::editor::bindingKindsFor(entry.value("type", "button")))
                    {
                        if (ImGui::MenuItem(("Add " + kindTitle(kind) + " Binding").c_str()))
                        {
                            later = [this, &entry, kind, actionIndex] {
                                entry["bindings"].push_back(fried::editor::newBinding(kind));
                                m_inputAction = actionIndex;
                                m_inputBinding = (int)entry["bindings"].size() - 1;
                            };
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Duplicate"))
                    {
                        std::vector<std::string> taken;
                        for (const Json &other : *actions)
                        {
                            taken.push_back(other.value("name", ""));
                        }
                        Json copy = entry;
                        copy["name"] = fried::editor::uniqueInputName(taken, entry.value("name", "Action"));
                        later = [actions, actionIndex, copy] { actions->insert(actions->begin() + actionIndex + 1, copy); };
                    }
                    if (ImGui::MenuItem("Delete"))
                    {
                        later = [actions, actionIndex] { actions->erase(actions->begin() + actionIndex); };
                    }
                    ImGui::EndPopup();
                }
                if (entry.contains("bindings") && entry["bindings"].is_array())
                {
                    for (int bindingIndex = 0; bindingIndex < (int)entry["bindings"].size(); ++bindingIndex)
                    {
                        const Json &binding = entry["bindings"][bindingIndex];
                        std::string kind = fried::editor::bindingKind(binding);
                        std::string text;
                        for (int part = 0; part < fried::editor::bindingPartCount(kind); ++part)
                        {
                            text += (part ? ", " : "") + fried::editor::sourceLabel(fried::editor::bindingPart(binding, part));
                        }
                        ImGui::PushID(bindingIndex);
                        if (choice((kind == "single" ? std::string() : kindTitle(kind) + ": ") + text, std::string(), isHere && m_inputBinding == bindingIndex, true, bindingIndex + 1 == (int)entry["bindings"].size()))
                        {
                            m_inputAction = actionIndex;
                            m_inputBinding = bindingIndex;
                        }
                        if (ImGui::BeginPopupContextItem("binding-menu"))
                        {
                            if (ImGui::MenuItem("Delete"))
                            {
                                later = [&entry, bindingIndex] { entry["bindings"].erase(entry["bindings"].begin() + bindingIndex); };
                            }
                            ImGui::EndPopup();
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::PopID();
            }
            if (actions->empty())
            {
                ImGui::TextDisabled("No actions. Add one with +");
            }
        });
        ImGui::SameLine();

        column("##properties", "Properties", propertiesWidth, nullptr, [] {}, false, [&] {
            const float label = scaled(90);
            auto labelled = [&](const char *text) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(text);
                ImGui::SameLine(label);
                ImGui::SetNextItemWidth(-FLT_MIN);
            };
            if (m_inputBinding < 0 && action == nullptr && map != nullptr)
            {
                ImGui::TextDisabled("Choose or add an action");
            }
            if (m_inputBinding < 0 && map != nullptr && action == nullptr)
            {
                // Only the map's own properties.
            }
            if (map && (m_inputBinding < 0 && action == nullptr))
            {
                ImGui::SeparatorText("Map");
                std::string renamed;
                labelled("Name");
                if (nameField("##map-name", (*map).value("name", ""), &renamed))
                {
                    (*map)["name"] = renamed;
                    isChanged = true;
                }
                bool isEnabled = (*map).value("enabled", true);
                labelled("Enabled");
                if (ImGui::Checkbox("##map-enabled", &isEnabled))
                {
                    (*map)["enabled"] = isEnabled;
                    isChanged = true;
                }
            }
            if (map && action && m_inputBinding < 0)
            {
                ImGui::SeparatorText("Action");
                std::string type = (*action).value("type", "button");
                std::string newName;
                labelled("Name");
                if (nameField("##action-name", (*action).value("name", ""), &newName))
                {
                    (*action)["name"] = newName;
                    isChanged = true;
                }
                labelled("Type");
                if (ImGui::BeginCombo("##action-type", type.c_str()))
                {
                    for (const std::string &candidate : fried::editor::inputActionTypes())
                    {
                        if (ImGui::Selectable(candidate.c_str(), candidate == type))
                        {
                            (*action)["type"] = candidate;
                            std::vector<std::string> kinds = fried::editor::bindingKindsFor(candidate);
                            Json kept = Json::array();
                            for (const Json &binding : (*action)["bindings"])
                            {
                                if (std::find(kinds.begin(), kinds.end(), fried::editor::bindingKind(binding)) != kinds.end())
                                {
                                    kept.push_back(binding);
                                }
                            }
                            (*action)["bindings"] = kept;
                            isChanged = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::TextDisabled("Button: pressed or not. Value: a number from -1 to 1. Vector2: a direction. Bindings the new type does not take are removed.");
                if (type != "vector2")
                {
                    float threshold = (float)(*action).value("threshold", 0.5);
                    labelled("Threshold");
                    if (ImGui::DragFloat("##threshold", &threshold, 0.01f, 0.0f, 1.0f, "%.2f"))
                    {
                        (*action)["threshold"] = std::round(threshold * 100.0) / 100.0;
                        isChanged = true;
                    }
                }
                ImGui::Spacing();
                ImGui::TextDisabled("Add a binding");
                for (const std::string &kind : fried::editor::bindingKindsFor(type))
                {
                    if (ImGui::Button(("+ " + kindTitle(kind)).c_str()))
                    {
                        Json *bindings = &(*action)["bindings"];
                        later = [this, bindings, kind] {
                            bindings->push_back(fried::editor::newBinding(kind));
                            m_inputBinding = (int)bindings->size() - 1;
                        };
                    }
                    ImGui::SameLine();
                }
                ImGui::NewLine();
                ImGui::Spacing();
                if (ImGui::Button("Delete Action"))
                {
                    int index = m_inputAction;
                    later = [actions, index] { actions->erase(actions->begin() + index); };
                }
            }
            if (map && action && m_inputBinding >= 0)
            {
                Json &binding = (*action)["bindings"][m_inputBinding];
                std::string kind = fried::editor::bindingKind(binding);
                ImGui::SeparatorText((kindTitle(kind) + " binding").c_str());
                for (int part = 0; part < fried::editor::bindingPartCount(kind); ++part)
                {
                    std::string partLabel = fried::editor::bindingPartLabel(kind, part);
                    labelled(partLabel.empty() ? "Source" : partLabel.c_str());
                    ImGui::PushID(part);
                    std::string text = fried::editor::bindingPart(binding, part);
                    if (ImGui::Button((fried::editor::sourceLabel(text) + "##source").c_str(), ImVec2(-FLT_MIN, 0)))
                    {
                        m_inputPick = {m_inputMap, m_inputAction, m_inputBinding, part, kind};
                        m_inputPickFilter.clear();
                        m_inputKeyPressed = -1;
                        isPickAsked = true;
                    }
                    ImGui::PopID();
                }
                ImGui::TextDisabled("Click a source, then press a key or choose one from the list.");
                ImGui::Spacing();
                if (ImGui::Button("Delete Binding"))
                {
                    int index = m_inputBinding;
                    Json *bindings = &(*action)["bindings"];
                    later = [bindings, index] { bindings->erase(bindings->begin() + index); };
                    m_inputBinding = -1;
                }
            }
        });

        // Choosing a source: type to find it, or press the key. It is opened here, where it is drawn, since a popup is known by the stack of ids it is opened in.
        if (isPickAsked)
        {
            ImGui::OpenPopup("input-pick");
        }
        if (ImGui::BeginPopup("input-pick"))
        {
            auto bindingOf = [&]() -> Json * {
                if (m_inputPick.m_map < (int)maps.size() && maps[m_inputPick.m_map]["actions"].size() > (std::size_t)m_inputPick.m_action)
                {
                    Json &bindings = maps[m_inputPick.m_map]["actions"][m_inputPick.m_action]["bindings"];
                    if (bindings.size() > (std::size_t)m_inputPick.m_binding)
                    {
                        return &bindings[m_inputPick.m_binding];
                    }
                }
                return nullptr;
            };
            auto choose = [&](const std::string &text) {
                if (Json *binding = bindingOf())
                {
                    fried::editor::setBindingPart(binding, m_inputPick.m_part, text);
                    isChanged = true;
                }
                ImGui::CloseCurrentPopup();
            };
            const bool isStick = m_inputPick.m_kind == "stick";
            ImGui::TextDisabled(isStick ? "Choose an axis of the stick" : "Press a key, or choose one below");
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::SetNextItemWidth(scaled(260));
            ImGui::InputTextWithHint("##pick-filter", "Search", &m_inputPickFilter);
            if (!isStick && m_inputKeyPressed >= 0 && m_inputKeyPressed != SDL_SCANCODE_ESCAPE)
            {
                std::string key = fried::editor::keyNameOf(names, m_inputKeyPressed);
                if (!key.empty())
                {
                    choose(fried::editor::sourceText("key", key));
                }
            }
            ImGui::BeginChild("##pick-list", ImVec2(scaled(260), scaled(260)), ImGuiChildFlags_Borders);
            auto section = [&](const char *title, const std::vector<InputName> &entries, const std::string &kind) {
                bool isTitled = false;
                for (const InputName &entry : entries)
                {
                    if (!containsText(entry.m_name, m_inputPickFilter) && !containsText(title, m_inputPickFilter))
                    {
                        continue;
                    }
                    if (!isTitled)
                    {
                        ImGui::TextDisabled("%s", title);
                        isTitled = true;
                    }
                    if (ImGui::Selectable((entry.m_name + "##" + kind).c_str()))
                    {
                        choose(fried::editor::sourceText(kind, entry.m_name));
                    }
                }
            };
            if (isStick)
            {
                section("Gamepad axes", names.m_gamepadAxes, "");
            }
            else
            {
                section("Keys", names.m_keys, "key");
                std::vector<InputName> mouse = names.m_mouseButtons;
                mouse.push_back({"ScrollX", 0});
                mouse.push_back({"ScrollY", 0});
                section("Mouse", mouse, "mouse");
                section("Gamepad buttons", names.m_gamepadButtons, "pad");
                section("Gamepad axes", names.m_gamepadAxes, "padAxis");
                section("Touch", {{"0", 0}, {"1", 1}}, "touch");
            }
            ImGui::EndChild();
            ImGui::EndPopup();
        }
        m_inputKeyPressed = -1;

        if (later)
        {
            int keptBinding = m_inputBinding;
            later();
            m_inputBinding = m_inputBinding == keptBinding ? -1 : m_inputBinding;
            isChanged = true;
        }
        if (isChanged)
        {
            m_settingErrors["input"] = settings.setInputBlock(block, names);
        }
        ImGui::TextDisabled("Every change is saved to project.fried as it is made.");
    }
}
