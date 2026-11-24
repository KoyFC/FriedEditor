#include "panels.h"

#include "style.h"

#include "fried/core/icon.h"

#include "fried/editor/fuzzy.h"

#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace
{
    using fried::core::BuildTarget;
    using fried::editor::Platform;

    constexpr float s_listBase = 170;
    constexpr float s_labelBase = 150;
    constexpr float s_iconPreviewBase = 128;
    constexpr double s_issuesLifetime = 3.0;
    constexpr int s_framerates[] = {0, 30, 60, 75, 90, 120, 144, 165, 240};
    constexpr int s_physicsRates[] = {20, 30, 60, 90, 120, 144, 240};

    // The label on the left and the widget filling the rest, which is how a settings page reads.
    void row(const char *label)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(editor_ui::scaled(s_labelBase));
        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    int lettersIn(const std::string &text)
    {
        return (int)std::count_if(text.begin(), text.end(), [](unsigned char byte) { return (byte & 0xc0) != 0x80; });
    }
}

namespace editor_ui
{
    // What the game is, whatever it is built for: where it starts, how it draws and how often it steps.
    void Panels::projectSettings(bool *isOpen)
    {
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(scaled(760), scaled(600)), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowClass(ownWindowClass());
        if (ImGui::Begin("Project Settings", isOpen, ownWindowFlags("Project Settings")))
        {
            if (ImGui::BeginTabBar("##project-settings-tabs"))
            {
                if (ImGui::BeginTabItem("General"))
                {
                    projectPage();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Input Map", nullptr, m_isInputTabSelected ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    m_isInputTabSelected = false;
                    inputMapPage();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    // What the builds are made of: the game's name and version, the platforms it is built for, and for each its icon, package text and build.
    void Panels::playerSettings(bool *isOpen)
    {
        m_isSettingsShown = false;
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(scaled(820), scaled(600)), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowClass(ownWindowClass());
        if (ImGui::Begin("Player Settings", isOpen, ownWindowFlags("Player Settings")))
        {
            m_isSettingsShown = true;
            if (m_settingsPage > 0 && !m_editor.project().declares(fried::core::s_platforms[m_settingsPage - 1]))
            {
                m_settingsPage = 0;
            }
            Platform shown = m_settingsPage == 0 ? Platform::Pc : fried::core::s_platforms[m_settingsPage - 1];
            size_t targetCount = m_settingsPage == 0 ? 0 : fried::editor::ProjectBuild::targetsOf(shown).size();
            float stripHeight = 0;
            if (targetCount > 0)
            {
                int rows = targetCount > 1 ? 5 : 4;
                stripHeight = rows * ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().WindowPadding.y * 2 + ImGui::GetStyle().ItemSpacing.y;
            }
            ImGui::BeginChild("##pages", ImVec2(scaled(s_listBase), -stripHeight), ImGuiChildFlags_Borders);
            if (ImGui::Selectable("General", m_settingsPage == 0))
            {
                m_settingsPage = 0;
            }
            ImGui::Separator();
            for (int index = 0; index < (int)fried::core::s_platforms.size(); ++index)
            {
                if (!m_editor.project().declares(fried::core::s_platforms[index]))
                {
                    continue;
                }
                if (ImGui::Selectable(fried::editor::platformLabel(fried::core::s_platforms[index]).c_str(), m_settingsPage == index + 1))
                {
                    m_settingsPage = index + 1;
                    m_iconNotes.clear();
                    m_iconError.clear();
                    m_buildMessage.clear();
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("##page", ImVec2(0, -stripHeight));
            if (m_settingsPage == 0)
            {
                playerPage();
            }
            else
            {
                platformPage(fried::core::s_platforms[m_settingsPage - 1]);
            }
            ImGui::EndChild();

            if (targetCount > 0)
            {
                ImGui::BeginChild("##buildSettings", ImVec2(0, 0), ImGuiChildFlags_Borders);
                buildSettingsStrip(shown);
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }

    // A line of text that is saved when the field is left. What was wrong with a refused value stays under the field until a value is accepted.
    void Panels::settingText(const char *label, const std::string &value, int limit, const std::function<std::string(const std::string &)> &set)
    {
        row(label);
        std::string edited = value;
        std::string id = std::string("##") + label;
        if (textField(id.c_str(), &edited))
        {
            m_settingErrors[label] = set(edited);
        }
        if (limit > 0)
        {
            const std::string &typed = m_textEdits[ImGui::GetID(id.c_str())];
            int length = lettersIn(typed);
            ImGui::SetCursorPosX(scaled(s_labelBase));
            ImGui::TextColored(length > limit ? palette::tomato : palette::whiteDim, "%d of %d characters", length, limit);
        }
        auto problem = m_settingErrors.find(label);
        if (problem != m_settingErrors.end() && !problem->second.empty())
        {
            ImGui::SetCursorPosX(scaled(s_labelBase));
            ImGui::PushStyleColor(ImGuiCol_Text, palette::tomato);
            ImGui::TextWrapped("%s", problem->second.c_str());
            ImGui::PopStyleColor();
        }
    }

    void Panels::playerPage()
    {
        fried::editor::ProjectSettings &settings = m_editor.projectSettings();
        const fried::editor::Project &project = m_editor.project();

        ImGui::SeparatorText("Identity");
        settingText("Name", project.name(), 0, [&](const std::string &value) { return settings.setName(value); });
        settingText("Organization", project.organization(), 0, [&](const std::string &value) { return settings.setOrganization(value); });
        settingText("Version", project.version(), 0, [&](const std::string &value) { return settings.setVersion(value); });
        settingText("Window title", project.windowTitle(), 0, [&](const std::string &value) { return settings.setWindowTitle(value); });

        ImGui::SeparatorText("Platforms");
        ImGui::TextDisabled("The engine refuses to build for a platform that is not ticked, and checks the limits of the ones that are.");
        for (Platform platform : fried::core::s_platforms)
        {
            bool isDeclared = project.declares(platform);
            std::string label = fried::editor::platformLabel(platform);
            if (ImGui::Checkbox(label.c_str(), &isDeclared))
            {
                m_settingErrors["Platforms"] = settings.setPlatformDeclared(platform, isDeclared);
            }
        }
        auto platformProblem = m_settingErrors.find("Platforms");
        if (platformProblem != m_settingErrors.end() && !platformProblem->second.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, palette::tomato);
            ImGui::TextWrapped("%s", platformProblem->second.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Every change is saved to project.fried as it is made.");
    }

    void Panels::projectPage()
    {
        fried::editor::ProjectSettings &settings = m_editor.projectSettings();
        const fried::editor::Project &project = m_editor.project();

        ImGui::SeparatorText("Start");
        row("Start scene");
        std::string startScene = project.startScene();
        if (ImGui::BeginCombo("##startScene", startScene.empty() ? "(none)" : startScene.c_str()))
        {
            if (ImGui::Selectable("(none)", startScene.empty()))
            {
                m_settingErrors["Start scene"] = settings.setStartScene("");
            }
            for (const std::string &scene : settings.startSceneChoices())
            {
                if (ImGui::Selectable(scene.c_str(), scene == startScene))
                {
                    m_settingErrors["Start scene"] = settings.setStartScene(scene);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SeparatorText("Autoload");
        ImGui::TextDisabled("Node classes of the project, or scenes, loaded once when the game starts and kept while it runs, for what the whole game needs. They are reached with SceneManager.autoload. A scene dragged from the Assets tab is added too.");
        {
            const ImVec2 sectionLow = ImGui::GetCursorScreenPos();
            std::vector<std::string> listed = project.autoloads();
            std::vector<std::string> next = listed;
            bool isChanged = false;
            for (std::size_t position = 0; position < listed.size(); ++position)
            {
                ImGui::PushID((int)position);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%zu  %s", position + 1, listed[position].c_str());
                ImGui::SameLine(scaled(s_labelBase) + scaled(120));
                if (ImGui::SmallButton("Up") && position > 0)
                {
                    std::swap(next[position], next[position - 1]);
                    isChanged = true;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Down") && position + 1 < next.size())
                {
                    std::swap(next[position], next[position + 1]);
                    isChanged = true;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove"))
                {
                    next.erase(next.begin() + position);
                    isChanged = true;
                }
                ImGui::PopID();
                if (isChanged)
                {
                    break;
                }
            }
            // What can be added is the project's classes and scenes that are not listed yet, found by typing a part of the name.
            std::vector<std::string> candidates;
            for (const std::string &name : settings.autoloadChoices())
            {
                if (std::find(listed.begin(), listed.end(), name) == listed.end())
                {
                    candidates.push_back(name);
                }
            }
            if (ImGui::Button("Add a node class or a scene...", ImVec2(scaled(260), 0)))
            {
                m_autoloadFilter.clear();
                ImGui::OpenPopup("add-autoload");
            }
            if (ImGui::BeginPopup("add-autoload"))
            {
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::SetNextItemWidth(scaled(320));
                bool isEntered = ImGui::InputTextWithHint("##autoloadFilter", "Type to filter", &m_autoloadFilter, ImGuiInputTextFlags_EnterReturnsTrue);
                std::vector<std::size_t> found = fried::editor::fuzzyRank(m_autoloadFilter, candidates);
                ImGui::BeginChild("##autoloadChoices", ImVec2(scaled(320), scaled(220)));
                for (std::size_t position = 0; position < found.size(); ++position)
                {
                    const std::string &name = candidates[found[position]];
                    if (ImGui::Selectable(name.c_str(), position == 0 && !m_autoloadFilter.empty()) || (isEntered && position == 0))
                    {
                        next.push_back(name);
                        isChanged = true;
                        ImGui::CloseCurrentPopup();
                        break;
                    }
                }
                if (found.empty())
                {
                    ImGui::TextDisabled("Nothing matches");
                }
                ImGui::EndChild();
                ImGui::EndPopup();
            }
            // A scene file dragged from the Assets tab onto the list is added as well.
            ImRect target(ImVec2(sectionLow.x, sectionLow.y), ImVec2(sectionLow.x + ImGui::GetContentRegionAvail().x, ImGui::GetItemRectMax().y));
            if (ImGui::BeginDragDropTargetCustom(target, ImGui::GetID("##autoloadDrop")))
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(s_assetPayload))
                {
                    std::string root, path;
                    const std::string suffix = ".scene.json";
                    if (fried::editor::parseAssetReference(std::string((const char *)payload->Data, payload->DataSize - 1), &root, &path) && path.size() > suffix.size()
                        && path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0)
                    {
                        std::string name = std::filesystem::path(path).filename().string();
                        name.resize(name.size() - suffix.size());
                        if (std::find(candidates.begin(), candidates.end(), name) != candidates.end())
                        {
                            next.push_back(name);
                            isChanged = true;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
            if (isChanged)
            {
                m_settingErrors["Autoload"] = settings.setAutoloads(next);
            }
            auto problem = m_settingErrors.find("Autoload");
            if (problem != m_settingErrors.end() && !problem->second.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, palette::tomato);
                ImGui::TextWrapped("%s", problem->second.c_str());
                ImGui::PopStyleColor();
            }
        }

        ImGui::SeparatorText("Display");
        fried::editor::DisplaySettings display = project.display();
        bool isChanged = false;
        row("Mode");
        if (ImGui::BeginCombo("##mode", display.m_mode.c_str()))
        {
            for (const std::string &mode : fried::core::displayModes())
            {
                if (ImGui::Selectable(mode.c_str(), mode == display.m_mode) && mode != display.m_mode)
                {
                    // Leaving the mode that has no size needs one to start from.
                    if (display.m_width <= 0 || display.m_height <= 0)
                    {
                        display.m_width = fried::core::s_suggestedDisplay.m_width;
                        display.m_height = fried::core::s_suggestedDisplay.m_height;
                    }
                    display.m_mode = mode;
                    isChanged = true;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::BeginDisabled(display.m_mode == "default");
        row("Width");
        ImGui::InputInt("##width", &display.m_width, 0);
        isChanged = isChanged || ImGui::IsItemDeactivatedAfterEdit();
        row("Height");
        ImGui::InputInt("##height", &display.m_height, 0);
        isChanged = isChanged || ImGui::IsItemDeactivatedAfterEdit();
        ImGui::EndDisabled();

        row("Filter");
        if (ImGui::BeginCombo("##filter", display.m_filter.c_str()))
        {
            for (const std::string &filter : fried::core::displayFilters())
            {
                if (ImGui::Selectable(filter.c_str(), filter == display.m_filter) && filter != display.m_filter)
                {
                    display.m_filter = filter;
                    isChanged = true;
                }
            }
            ImGui::EndCombo();
        }
        if (isChanged)
        {
            m_settingErrors["Display"] = settings.setDisplay(display);
        }
        if (!m_settingErrors["Display"].empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, palette::tomato);
            ImGui::TextWrapped("%s", m_settingErrors["Display"].c_str());
            ImGui::PopStyleColor();
        }

        bool vsync = project.vsync();
        if (ImGui::Checkbox("Vsync", &vsync))
        {
            m_settingErrors["Vsync"] = settings.setVsync(vsync);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Desktop windows only, consoles ignore it.");

        ImGui::BeginDisabled(vsync);
        int framerate = project.defaultTargetFramerate();
        bool isPreset = std::find(std::begin(s_framerates), std::end(s_framerates), framerate) != std::end(s_framerates);
        bool isCustom = m_isFramerateCustom || !isPreset;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Default target framerate");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(scaled(140));
        std::string shown = isCustom ? "Custom" : framerate == 0 ? "No limit" : std::to_string(framerate) + " fps";
        if (ImGui::BeginCombo("##defaultTargetFramerate", shown.c_str()))
        {
            for (int choice : s_framerates)
            {
                std::string label = choice == 0 ? "No limit" : std::to_string(choice) + " fps";
                if (ImGui::Selectable(label.c_str(), !isCustom && choice == framerate))
                {
                    m_isFramerateCustom = false;
                    m_settingErrors["Framerate"] = settings.setDefaultTargetFramerate(choice);
                }
            }
            if (ImGui::Selectable("Custom", isCustom))
            {
                m_isFramerateCustom = true;
            }
            ImGui::EndCombo();
        }
        if (isCustom)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(scaled(100));
            if (ImGui::InputInt("##customFramerate", &framerate, 0))
            {
                m_settingErrors["Framerate"] = settings.setDefaultTargetFramerate(std::max(framerate, 0));
            }
            ImGui::SameLine();
            ImGui::TextDisabled("fps");
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("Only used while vsync is off, as the rate the game starts at (0 is no limit). The game can change it afterwards, for example from a settings menu.");

        int physicsRate = project.physicsRate();
        bool isPhysicsPreset = std::find(std::begin(s_physicsRates), std::end(s_physicsRates), physicsRate) != std::end(s_physicsRates);
        bool isPhysicsCustom = m_isPhysicsRateCustom || !isPhysicsPreset;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Physics rate");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(scaled(140));
        std::string physicsShown = isPhysicsCustom ? "Custom" : std::to_string(physicsRate) + " steps/s";
        if (ImGui::BeginCombo("##physicsRate", physicsShown.c_str()))
        {
            for (int choice : s_physicsRates)
            {
                std::string label = std::to_string(choice) + " steps/s";
                if (ImGui::Selectable(label.c_str(), !isPhysicsCustom && choice == physicsRate))
                {
                    m_isPhysicsRateCustom = false;
                    m_settingErrors["PhysicsRate"] = settings.setPhysicsRate(choice);
                }
            }
            if (ImGui::Selectable("Custom", isPhysicsCustom))
            {
                m_isPhysicsRateCustom = true;
            }
            ImGui::EndCombo();
        }
        if (isPhysicsCustom)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(scaled(100));
            if (ImGui::InputInt("##customPhysicsRate", &physicsRate, 0))
            {
                m_settingErrors["PhysicsRate"] = settings.setPhysicsRate(physicsRate);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("steps/s");
        }
        ImGui::TextDisabled("The fixed steps a second the game runs fixedUpdate and its kinematic bodies at, whatever the frame rate. A console that cannot afford 60 may ask for 30.");
        if (!m_settingErrors["PhysicsRate"].empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, palette::tomato);
            ImGui::TextWrapped("%s", m_settingErrors["PhysicsRate"].c_str());
            ImGui::PopStyleColor();
        }

        std::pair<int, int> size = project.logicalSize(m_editor.platform());
        ImGui::TextDisabled("On the %s the game draws in %dx%d, which is the frame the viewport shows.", fried::editor::platformLabel(m_editor.platform()).c_str(),
                            size.first, size.second);
        ImGui::Spacing();
        ImGui::TextDisabled("Every change is saved to project.fried as it is made.");
    }

    void Panels::platformPage(Platform platform)
    {
        iconSection(platform);
        packagingSection(platform);
        precisionSection(platform);
        buildSection(platform);
    }

    void Panels::precisionSection(Platform platform)
    {
        if (!fried::core::offersPrecision(platform))
        {
            return;
        }

        const fried::editor::Project &project = m_editor.project();
        fried::editor::Precision current = project.precision(platform);
        ImGui::SeparatorText("Precision");
        row("Float");
        std::string shown = current == fried::editor::Precision::Single ? "Single (32-bit)" : "Double (64-bit)";
        if (fried::core::defaultPrecision(platform) == current)
        {
            shown += " (default)";
        }
        if (ImGui::BeginCombo("##precision", shown.c_str()))
        {
            for (fried::editor::Precision choice : {fried::editor::Precision::Single, fried::editor::Precision::Double})
            {
                std::string label = choice == fried::editor::Precision::Single ? "Single (32-bit)" : "Double (64-bit)";
                if (choice == fried::core::defaultPrecision(platform))
                {
                    label += " (default)";
                }
                if (ImGui::Selectable(label.c_str(), choice == current) && choice != current)
                {
                    m_settingErrors["Precision"] = m_editor.projectSettings().setPrecision(platform, choice);
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SetCursorPosX(scaled(s_labelBase));
        ImGui::TextDisabled("The width of Haxe's Float in this platform's build. Single is faster where there is no floating point unit, as on the DS, but holds whole numbers exactly only up to 16,777,216 and must not be used for the clock. Takes effect at the next build.");
        const std::string &problem = m_settingErrors["Precision"];
        if (!problem.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, palette::tomato);
            ImGui::TextWrapped("%s", problem.c_str());
            ImGui::PopStyleColor();
        }
    }

    void Panels::applyIcon(Platform platform, const std::string &file, bool isForEveryPlatform)
    {
        fried::editor::ProjectSettings &settings = m_editor.projectSettings();
        m_iconNotes.clear();
        m_iconError.clear();
        m_iconSource = file;

        // A path that does not start at the root is one inside the project.
        std::filesystem::path source = std::filesystem::path(file).is_absolute() ? std::filesystem::path(file) : std::filesystem::path(m_editor.project().directory()) / file;

        std::vector<Platform> platforms = {platform};
        if (isForEveryPlatform)
        {
            platforms.assign(fried::core::s_platforms.begin(), fried::core::s_platforms.end());
        }
        for (Platform target : platforms)
        {
            if (fried::core::Icon::paths(target).empty())
            {
                continue;
            }
            std::vector<std::string> notes;
            if (!settings.setIcon(target, source.string(), &notes, &m_iconError))
            {
                return;
            }
            if (target == platform)
            {
                m_iconNotes = notes;
            }
            for (const std::string &written : settings.iconFiles(target))
            {
                m_images.forget(written);
            }
        }
        if (m_iconNotes.empty())
        {
            m_iconNotes.push_back("Used as it is.");
        }
    }

    void Panels::iconSection(Platform platform)
    {
        std::vector<std::string> files = m_editor.projectSettings().iconFiles(platform);
        if (files.empty())
        {
            return;
        }

        ImGui::SeparatorText("Icon");
        ImGui::BeginGroup();
        bool hasIcon = false;
        for (const std::string &file : files)
        {
            if (const Texture *texture = m_images.get(file))
            {
                float scale = std::min(scaled(s_iconPreviewBase) / texture->width(), scaled(s_iconPreviewBase) / texture->height());
                // A small icon is shown at a whole number of times its size, so its pixels stay square.
                scale = scale >= 1.0f ? std::floor(scale) : scale;
                ImGui::Image(texture->id(), ImVec2(texture->width() * scale, texture->height() * scale));
                ImGui::SameLine();
                hasIcon = true;
            }
        }
        if (!hasIcon)
        {
            ImGui::Button("No icon yet", ImVec2(scaled(s_iconPreviewBase), scaled(s_iconPreviewBase)));
            ImGui::SameLine();
        }
        ImGui::EndGroup();
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(s_assetPayload))
            {
                std::string root, path;
                if (fried::editor::parseAssetReference(std::string((const char *)payload->Data, payload->DataSize - 1), &root, &path))
                {
                    applyIcon(platform, m_editor.assetFile(root, path), false);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        ImGui::BeginGroup();
        for (const std::string &path : fried::core::Icon::paths(platform))
        {
            ImGui::TextDisabled("%s", path.c_str());
        }
        ImGui::TextWrapped("Drop an image here, from the Assets panel or from a file manager, or give its path, from the project's folder or whole. It is cut, resized and converted to what this platform reads.");
        ImGui::EndGroup();

        row("Image");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##iconSource", &m_iconSource);
        ImGui::BeginDisabled(m_iconSource.empty());
        if (ImGui::Button("Set this platform's icon"))
        {
            applyIcon(platform, m_iconSource, false);
        }
        ImGui::SameLine();
        if (ImGui::Button("Set every platform's icon"))
        {
            applyIcon(platform, m_iconSource, true);
        }
        ImGui::EndDisabled();

        if (!m_iconError.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, palette::tomato);
            ImGui::TextWrapped("%s", m_iconError.c_str());
            ImGui::PopStyleColor();
        }
        for (const std::string &note : m_iconNotes)
        {
            ImGui::Bullet();
            ImGui::TextWrapped("%s", note.c_str());
        }
    }

    void Panels::packagingSection(Platform platform)
    {
        fried::editor::ProjectSettings &settings = m_editor.projectSettings();
        const fried::editor::Project &project = m_editor.project();
        if (platform == Platform::Vita)
        {
            ImGui::SeparatorText("Package");
            settingText("Title ID", project.vitaTitleId(), 0, [&](const std::string &value) { return settings.setVitaTitleId(value); });
            ImGui::TextDisabled("Four capital letters and five digits, which is how the console tells one game from another.");
        }
        else if (platform == Platform::Nintendo3ds)
        {
            ImGui::SeparatorText("Package");
            settingText("Description", project.packagingText("3ds", "description"), fried::editor::ProjectSettings::packagingTextLimit("3ds"),
                        [&](const std::string &value) { return settings.setPackagingText("3ds", "description", value); });
            ImGui::TextDisabled("Shown under the name in the home menu. Left empty, it is the version.");
        }
        else if (platform == Platform::NintendoDs)
        {
            ImGui::SeparatorText("Package");
            int limit = fried::editor::ProjectSettings::packagingTextLimit("nds");
            settingText("Second line", project.packagingText("nds", "subtitle1"), limit,
                        [&](const std::string &value) { return settings.setPackagingText("nds", "subtitle1", value); });
            settingText("Third line", project.packagingText("nds", "subtitle2"), limit,
                        [&](const std::string &value) { return settings.setPackagingText("nds", "subtitle2", value); });
            ImGui::TextDisabled("The banner's first line is the name. Left empty, these are the organization and the version.");
        }
    }

    BuildTarget Panels::selectedTarget(Platform platform)
    {
        std::vector<BuildTarget> targets = fried::editor::ProjectBuild::targetsOf(platform);
        int &choice = m_buildTargetChoice[platform];
        choice = std::clamp(choice, 0, std::max(0, (int)targets.size() - 1));
        return targets[choice];
    }

    // The settings of a build, kept below both columns so they stay in view whatever the page holds.
    void Panels::buildSettingsStrip(Platform platform)
    {
        fried::editor::ProjectBuild &build = m_editor.projectBuild();
        std::vector<BuildTarget> targets = fried::editor::ProjectBuild::targetsOf(platform);
        int &choice = m_buildTargetChoice[platform];
        choice = std::clamp(choice, 0, (int)targets.size() - 1);
        if (targets.size() > 1)
        {
            row("Build for");
            if (ImGui::BeginCombo("##target", fried::core::buildTargetName(targets[choice]).c_str()))
            {
                for (int index = 0; index < (int)targets.size(); ++index)
                {
                    if (ImGui::Selectable(fried::core::buildTargetName(targets[index]).c_str(), index == choice))
                    {
                        choice = index;
                    }
                }
                ImGui::EndCombo();
            }
        }
        BuildTarget target = targets[choice];

        fried::editor::BuildSettings settings = build.settings(target);
        bool isChanged = false;
        row("Configuration");
        if (ImGui::BeginCombo("##type", fried::core::buildTypeName(settings.m_type).c_str()))
        {
            for (fried::core::BuildType type : {fried::core::BuildType::Debug, fried::core::BuildType::Release})
            {
                if (ImGui::Selectable(fried::core::buildTypeName(type).c_str(), type == settings.m_type))
                {
                    settings.m_type = type;
                    isChanged = true;
                }
            }
            ImGui::EndCombo();
        }
        row("Build folder");
        isChanged = textField("##directory", &settings.m_directory) || isChanged;
        row("Log to a file");
        isChanged = ImGui::Checkbox("##logToFile", &settings.m_logToFile) || isChanged;
        if (isChanged)
        {
            build.setSettings(target, settings);
        }

        fried::editor::BuildProgress progress = m_editor.buildProgress();
        bool isBusy = progress.m_isVisible && !progress.m_isFinished;
        std::string label = "Build for " + fried::core::buildTargetName(target);
        float buttonWidth = scaled(220);
        if (!m_buildMessage.empty())
        {
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, palette::tomato);
            ImGui::TextUnformatted(m_buildMessage.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - buttonWidth));
        ImGui::BeginDisabled(!build.isReady(target) || isBusy);
        if (ImGui::Button(label.c_str(), ImVec2(buttonWidth, 0)))
        {
            std::string error;
            m_buildMessage = m_editor.buildProject(target, &error) ? std::string() : error;
        }
        ImGui::EndDisabled();
    }

    void Panels::buildSection(Platform platform)
    {
        fried::editor::ProjectBuild &build = m_editor.projectBuild();
        std::vector<BuildTarget> targets = fried::editor::ProjectBuild::targetsOf(platform);
        if (targets.empty())
        {
            return;
        }

        ImGui::SeparatorText("Build");
        BuildTarget target = selectedTarget(platform);

        std::string status = build.status(target);
        bool isReady = build.isReady(target);
        row("Toolchain");
        ImGui::PushStyleColor(ImGuiCol_Text, isReady ? palette::leaf : palette::tomato);
        ImGui::TextWrapped("%s", status.c_str());
        ImGui::PopStyleColor();
        ImGui::SetCursorPosX(scaled(s_labelBase));
        if (ImGui::SmallButton("Look again"))
        {
            build.refreshToolchains();
        }

        if (ImGui::TreeNode("Commands"))
        {
            std::string commands = build.commands(target);
            ImGui::TextDisabled("Run in the project's folder:");
            ImGui::TextUnformatted(commands.c_str());
            if (ImGui::SmallButton("Copy"))
            {
                ImGui::SetClipboardText(commands.c_str());
            }
            ImGui::TreePop();
        }

        // The check walks the project's assets, so it is done every few seconds and not every frame.
        if (m_buildIssuesTarget != (int)target || ImGui::GetTime() - m_buildIssuesCheckedAt > s_issuesLifetime)
        {
            m_buildIssues = build.issues(target);
            m_buildIssuesTarget = (int)target;
            m_buildIssuesCheckedAt = ImGui::GetTime();
        }
        if (!m_buildIssues.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(palette::warning, "%d asset issues on this platform", (int)m_buildIssues.size());
            int index = 0;
            for (const fried::editor::AssetIssue &issue : m_buildIssues)
            {
                ImGui::PushID(index++);
                bool isError = issue.m_severity == fried::editor::AssetIssue::Severity::Error;
                ImGui::TextColored(isError ? palette::tomato : palette::warning, "%s", isError ? "Error" : "Warning");
                ImGui::SameLine();
                ImGui::TextWrapped("%s%s%s", issue.m_asset.c_str(), issue.m_asset.empty() ? "" : ": ", issue.m_message.c_str());
                if (issue.m_fix != fried::editor::AssetIssue::Fix::None)
                {
                    if (ImGui::SmallButton(issue.m_fixLabel.c_str()))
                    {
                        std::string error;
                        m_buildMessage = m_editor.fixIssue(issue, &error) ? std::string() : error;
                        m_buildIssuesCheckedAt = -100;
                    }
                }
                ImGui::PopID();
            }
        }
    }

    void Panels::fileDropped(const std::string &file)
    {
        if (!m_isSettingsShown || m_settingsPage == 0)
        {
            m_editor.setMessage("Open Project Settings on a platform to use a dropped image as its icon.");
            return;
        }
        applyIcon(fried::core::s_platforms[m_settingsPage - 1], file, false);
    }
}
