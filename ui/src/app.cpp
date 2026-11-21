#include "app.h"

#include "opengl.h"
#include "panels.h"
#include "pointer.h"
#include "style.h"

#include "fried/editor/editor_state.h"
#include "fried/editor/expression.h"
#include "fried/editor/version.h"

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

namespace editor_ui
{
    namespace
    {
        using fried::editor::Editor;

        std::string layoutFile()
        {
            std::filesystem::path directory = fried::editor::configDirectory();
            if (directory.empty())
            {
                directory = ".";
            }
            std::error_code ignored;
            std::filesystem::create_directories(directory, ignored);
            return (directory / "layout.ini").string();
        }

        constexpr int s_scalePercents[] = {75, 100, 125, 150, 175, 200, 250, 300};

        // The first time there is no saved layout, so the panels are put where an editor has them.
        void buildLayout(ImGuiID dockspace, ImVec2 size)
        {
            ImGui::DockBuilderRemoveNode(dockspace);
            ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace, size);

            ImGuiID center = dockspace;
            ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
            ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.26f, nullptr, &center);
            ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.26f, nullptr, &center);
            ImGui::DockBuilderDockWindow("Hierarchy", left);
            ImGui::DockBuilderDockWindow("Inspector", right);
            ImGui::DockBuilderDockWindow("Issues###Issues", bottom);
            ImGui::DockBuilderDockWindow("Assets", bottom);
            ImGui::DockBuilderDockWindow("Console###Console", bottom);
            ImGui::DockBuilderDockWindow("Viewport", center);
            ImGui::DockBuilderFinish(dockspace);
        }

        void save(Editor &editor)
        {
            std::string error;
            if (!editor.save(&error))
            {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Save", error.c_str(), nullptr);
            }
        }

        void copyNode(Editor &editor)
        {
            std::string text = editor.copySelected();
            if (!text.empty())
            {
                ImGui::SetClipboardText(text.c_str());
            }
        }

        void pasteNode(Editor &editor)
        {
            const char *text = ImGui::GetClipboardText();
            editor.paste(text ? text : "");
        }

        bool sideBar(const char *name, ImGuiDir direction, float height, const std::function<void()> &content)
        {
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
            bool isOpen = ImGui::BeginViewportSideBar(name, ImGui::GetMainViewport(), direction, height, flags);
            if (isOpen)
            {
                content();
            }
            ImGui::End();
            return isOpen;
        }
    }

    float nextUiScale(float current, int step)
    {
        int count = (int)(sizeof(s_scalePercents) / sizeof(s_scalePercents[0]));
        int index = 0;
        for (int candidate = 0; candidate < count; ++candidate)
        {
            if (std::fabs(s_scalePercents[candidate] / 100.0f - current) < std::fabs(s_scalePercents[index] / 100.0f - current))
            {
                index = candidate;
            }
        }
        return s_scalePercents[std::clamp(index + step, 0, count - 1)] / 100.0f;
    }

    int run(Editor &editor)
    {
        bool isVideoReady = false;
#ifdef __linux__
        // Panels dragged out of the editor become windows of their own, which SDL only does on X11. A Wayland session runs it through XWayland, unless the user chose a driver.
        if (std::getenv("SDL_VIDEODRIVER") == nullptr)
        {
            SDL_SetHint(SDL_HINT_VIDEODRIVER, "x11");
            isVideoReady = SDL_Init(SDL_INIT_VIDEO) == 0;
            if (!isVideoReady)
            {
                SDL_ClearError();
                SDL_ResetHint(SDL_HINT_VIDEODRIVER);
            }
        }
#endif
        if (!isVideoReady && SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fried Editor", SDL_GetError(), nullptr);
            return 1;
        }
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        SDL_Window *window = SDL_CreateWindow(("Fried Editor " + fried::editor::version()).c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1500, 900,
                                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window)
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fried Editor", SDL_GetError(), nullptr);
            SDL_Quit();
            return 1;
        }
        SDL_GLContext context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, context);
        SDL_GL_SetSwapInterval(1);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiEvaluateNumberHook = [](const char *text, double *value) {
            std::optional<double> result = fried::editor::evaluateExpression(text);
            if (result)
            {
                *value = *result;
            }
            return result.has_value();
        };
        ImGuiIO &io = ImGui::GetIO();
        static const std::string s_layout = layoutFile();
        io.IniFilename = s_layout.c_str();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_ViewportsEnable;
        io.ConfigViewportsNoTaskBarIcon = true;
        applyStyle(editor.enginePath() + "/assets/NunitoSans.ttf");
        setUiScale(fried::editor::EditorState::uiScale());
        ImGui_ImplSDL2_InitForOpenGL(window, context);
        ImGui_ImplOpenGL3_Init("#version 330");

        Panels panels(editor);
        Pointer pointer(window);
        bool isRunning = true;
        bool isCloseAsked = false;
        bool isSettingsOpen = false;
        bool isLayoutBuilt = false;
        int startupFrame = 0;
        fried::editor::GameRunner::State previousGameState = editor.gameState();

        while (isRunning)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                ImGui_ImplSDL2_ProcessEvent(&event);
                pointer.handleEvent(event);
                if (event.type == SDL_KEYDOWN && (event.key.keysym.mod & KMOD_CTRL))
                {
                    // By the character rather than the key, as "+" is a key of its own on some layouts.
                    SDL_Keycode key = event.key.keysym.sym;
                    int step = key == SDLK_PLUS || key == SDLK_EQUALS || key == SDLK_KP_PLUS ? 1 : key == SDLK_MINUS || key == SDLK_KP_MINUS ? -1 : 0;
                    if (step != 0)
                    {
                        float next = nextUiScale(uiScale(), step);
                        setUiScale(next);
                        fried::editor::EditorState::setUiScale(next);
                    }
                }
                if (event.type == SDL_DROPFILE)
                {
                    panels.fileDropped(event.drop.file);
                    SDL_free(event.drop.file);
                }
                // The windows of the panels have their own events, which are theirs to handle.
                bool isMainWindow = event.type != SDL_WINDOWEVENT || event.window.windowID == SDL_GetWindowID(window);
                if (event.type == SDL_WINDOWEVENT && isMainWindow && event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                {
                    editor.refreshAssets();
                }
                if (event.type == SDL_QUIT || (event.type == SDL_WINDOWEVENT && isMainWindow && event.window.event == SDL_WINDOWEVENT_CLOSE))
                {
                    if (editor.commands().isClean())
                    {
                        isRunning = false;
                    }
                    else
                    {
                        isCloseAsked = true;
                    }
                }
            }

            editor.pump();
            fried::editor::GameRunner::State gameState = editor.gameState();
            if (previousGameState == fried::editor::GameRunner::State::Idle && gameState == fried::editor::GameRunner::State::Building)
            {
                ImGui::SetWindowFocus("Console###Console");
            }
            previousGameState = gameState;

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            pointer.beginFrame();
            panels.setHeldMotion(pointer.takeHeldMotion());

            editor.setSnapHeld(io.KeyCtrl);
            if (!io.WantTextInput)
            {
                bool isControl = io.KeyCtrl;
                if (isControl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
                {
                    io.KeyShift ? editor.commands().redo() : editor.commands().undo();
                }
                else if (isControl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
                {
                    editor.commands().redo();
                }
                else if (isControl && ImGui::IsKeyPressed(ImGuiKey_D, false))
                {
                    editor.duplicateSelected();
                }
                else if (isControl && ImGui::IsKeyPressed(ImGuiKey_C, false))
                {
                    copyNode(editor);
                }
                else if (isControl && ImGui::IsKeyPressed(ImGuiKey_V, false))
                {
                    pasteNode(editor);
                }
                else if (isControl && ImGui::IsKeyPressed(ImGuiKey_P, false))
                {
                    panels.showQuickOpen();
                }
                else if (isControl && ImGui::IsKeyPressed(ImGuiKey_S, false))
                {
                    save(editor);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
                {
                    editor.play();
                }
                else if (!isControl && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsKeyPressed(ImGuiKey_W, false))
                {
                    editor.setGizmoMode(fried::editor::GizmoMode::Move);
                }
                else if (!isControl && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsKeyPressed(ImGuiKey_E, false))
                {
                    editor.setGizmoMode(fried::editor::GizmoMode::Rotate);
                }
                else if (!isControl && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsKeyPressed(ImGuiKey_R, false))
                {
                    editor.setGizmoMode(fried::editor::GizmoMode::Scale);
                }
                else if (!isControl && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsKeyPressed(ImGuiKey_F, false))
                {
                    editor.focusSelected();
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                {
                    editor.removeSelected();
                }
            }

            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Save", "Ctrl+S"))
                    {
                        save(editor);
                    }
                    if (ImGui::MenuItem("Quick open...", "Ctrl+P"))
                    {
                        panels.showQuickOpen();
                    }
                    if (ImGui::BeginMenu("Open scene"))
                    {
                        for (const std::string &path : editor.scenePaths())
                        {
                            std::string label = std::filesystem::path(path).filename().string();
                            if (ImGui::MenuItem(label.c_str(), nullptr, path == editor.scenePath()))
                            {
                                panels.openSceneFile(path);
                            }
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Open recent", !editor.recentScenes().empty()))
                    {
                        for (const std::string &path : editor.recentScenes())
                        {
                            std::string label = std::filesystem::path(path).filename().string();
                            if (ImGui::MenuItem(label.c_str(), nullptr, path == editor.scenePath()))
                            {
                                panels.openSceneFile(path);
                            }
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::MenuItem("Play", "F5"))
                    {
                        editor.play();
                    }
                    if (ImGui::MenuItem("Stop game", nullptr, false, editor.gameState() != fried::editor::GameRunner::State::Idle))
                    {
                        editor.stopGame();
                    }
                    if (ImGui::MenuItem("Rebuild player"))
                    {
                        editor.rebuildPlayer();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Project Settings...", nullptr, isSettingsOpen))
                    {
                        isSettingsOpen = !isSettingsOpen;
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Quit"))
                    {
                        isCloseAsked = !editor.commands().isClean();
                        isRunning = !editor.commands().isClean();
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Edit"))
                {
                    auto &commands = editor.commands();
                    std::string undo = "Undo " + commands.undoLabel();
                    std::string redo = "Redo " + commands.redoLabel();
                    if (ImGui::MenuItem(undo.c_str(), "Ctrl+Z", false, commands.canUndo()))
                    {
                        commands.undo();
                    }
                    if (ImGui::MenuItem(redo.c_str(), "Ctrl+Shift+Z", false, commands.canRedo()))
                    {
                        commands.redo();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, editor.selected().has_value()))
                    {
                        editor.duplicateSelected();
                    }
                    if (ImGui::MenuItem("Copy", "Ctrl+C", false, editor.selected().has_value()))
                    {
                        copyNode(editor);
                    }
                    if (ImGui::MenuItem("Paste", "Ctrl+V"))
                    {
                        pasteNode(editor);
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("View"))
                {
                    if (ImGui::BeginMenu("Interface scale"))
                    {
                        for (int percent : s_scalePercents)
                        {
                            float scale = percent / 100.0f;
                            std::string label = std::to_string(percent) + "%";
                            if (ImGui::MenuItem(label.c_str(), nullptr, std::fabs(uiScale() - scale) < 0.001f))
                            {
                                setUiScale(scale);
                                fried::editor::EditorState::setUiScale(scale);
                            }
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
                ImGui::SameLine(ImGui::GetWindowWidth() - scaled(260));
                ImGui::TextDisabled("%s%s", editor.project().name().c_str(), editor.commands().isClean() ? "" : "  *");
                ImGui::EndMainMenuBar();
            }

            float bar = ImGui::GetFrameHeight() + 10;
            sideBar("##toolbar", ImGuiDir_Up, bar, [&] { panels.toolbar(); });
            sideBar("##status", ImGuiDir_Down, bar - 6, [&] { panels.statusBar(); });

            ImGuiID dockspace = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
            if (!isLayoutBuilt)
            {
                isLayoutBuilt = true;
                if (!ImGui::DockBuilderGetNode(dockspace) || ImGui::DockBuilderGetNode(dockspace)->IsLeafNode())
                {
                    if (!std::filesystem::exists(s_layout) || std::filesystem::file_size(s_layout) < 64)
                    {
                        buildLayout(dockspace, ImGui::GetMainViewport()->Size);
                    }
                }
            }

            if (ImGui::Begin("Viewport"))
            {
                panels.viewport();
            }
            ImGui::End();
            if (ImGui::Begin("Hierarchy"))
            {
                panels.hierarchy();
            }
            ImGui::End();
            if (ImGui::Begin("Inspector"))
            {
                panels.inspector();
            }
            ImGui::End();
            if (ImGui::Begin("Assets"))
            {
                panels.assets();
            }
            ImGui::End();
            // A layout saved before the console existed has no place for it, so it joins the assets.
            if (ImGuiWindow *assets = ImGui::FindWindowByName("Assets"); assets && assets->DockId)
            {
                ImGui::SetNextWindowDockID(assets->DockId, ImGuiCond_FirstUseEver);
            }
            std::string consoleTitle = panels.consoleErrorCount() > 0 ? "Console (" + std::to_string(panels.consoleErrorCount()) + ")###Console" : "Console###Console";
            if (ImGui::Begin(consoleTitle.c_str()))
            {
                panels.console();
            }
            ImGui::End();
            std::string issuesTitle = panels.issueCount() > 0 ? "Issues (" + std::to_string(panels.issueCount()) + ")###Issues" : "Issues###Issues";
            if (ImGui::Begin(issuesTitle.c_str()))
            {
                panels.issues();
            }
            ImGui::End();

            // The editor opens on the assets, unless the project has problems on the platform being previewed, which are what to see first.
            if (startupFrame < 3)
            {
                if (startupFrame == 0)
                {
                    panels.refreshIssues();
                }
                else if (startupFrame == 2)
                {
                    ImGui::SetWindowFocus(panels.issueCount() > 0 ? "Issues###Issues" : "Assets");
                }
                ++startupFrame;
            }

            if (isSettingsOpen)
            {
                panels.projectSettings(&isSettingsOpen);
            }
            panels.buildWindow();
            panels.quickOpen();
            panels.sceneChangeDialog();

            if (isCloseAsked)
            {
                ImGui::OpenPopup("Unsaved changes");
                isCloseAsked = false;
            }
            if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted("Save the scene before closing?");
                ImGui::Spacing();
                if (ImGui::Button("Save"))
                {
                    save(editor);
                    isRunning = !editor.commands().isClean();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Discard"))
                {
                    isRunning = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            pointer.endFrame(panels.isPanning(), panels.takeValueDrag());
            ImGui::Render();
            int width = 0, height = 0;
            SDL_GL_GetDrawableSize(window, &width, &height);
            glViewport(0, 0, width, height);
            glClearColor(0.085f, 0.085f, 0.105f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                SDL_Window *current = SDL_GL_GetCurrentWindow();
                SDL_GLContext currentContext = SDL_GL_GetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                SDL_GL_MakeCurrent(current, currentContext);
            }
            SDL_GL_SwapWindow(window);

            std::string title = editor.document().name() + (editor.commands().isClean() ? "" : "*") + " - " + editor.project().name() + " - Fried Editor";
            SDL_SetWindowTitle(window, title.c_str());
        }

        editor.saveView();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }
}
